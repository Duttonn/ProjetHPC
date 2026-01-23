# Contexte du Projet Motion HPC

Ce document sert de mémoire technique et d'état des lieux pour le projet d'optimisation de la chaîne de détection de mouvement "Motion".

## 1. Vue d'ensemble du Projet
- **Objectif** : Accélérer une application de streaming (détection et suivi d'objets) en utilisant le parallélisme (SIMD, OpenMP, GPU) tout en restant **identique au bit près** au modèle d'or (`motion2`).
- **Algorithmes clés** : Sigma-Delta (ΣΔ), Morphologie Mathématique (Érosion/Dilatation/Ouverture/Fermeture), CCL (Labeling), CCA (Analyse), k-NN (Matching), Tracking.
- **Métriques** : Débit en Images Par Seconde (FPS). Mesures effectuées hors I/O (vidéo bufferisée).

## 2. État de l'Implémentation (au 23 Janvier 2026)

### ✅ Optimisations Terminées
1. **Simplification du Graphe (TP3)** : Implémenté dans `motion.c`. Utilisation du pattern *produce/memorize* pour éviter de recalculer les RoIs de l'image $t-1$.
2. **SIMD (MIPP)** :
    - **Sigma-Delta** : Fusion des 4 boucles en 1 seule passe. Utilisation de `mipp::blend` pour supprimer les branchements. Vectorisation 32 pixels (AVX2).
    - **Morphologie** : Vectorisation avec chargements décalés (shifted loads).
3. **Multi-threading (OpenMP)** : Parallélisation par ligne (`#pragma omp parallel for schedule(dynamic)`) sur ΣΔ et Morpho.
4. **Morphologie Séparable** : Élément structurant $3\times3$ décomposé en $(3\times1) \circ (1\times3)$. Réduction du nombre d'opérations de 9 à 6 par pixel.
5. **Bit-Packing** : Fonctions `morpho_pack_binary` et `morpho_unpack_binary` implémentées. Stockage de 8 pixels par octet (gain bande passante x8).
6. **Fusion d'Opérateurs** : Fonction `sigma_delta_morpho_fused` créée pour enchaîner ΣΔ + Ouverture + Fermeture en une seule passe de cache.

### ⏳ En cours / À Intégrer
1. **Intégration GPU (OpenCL)** : Kernels écrits dans `kernels/motion_kernels.cl` (ΣΔ, Morpho, Fused). Reste à écrire le code "host" (init OpenCL, buffers).
2. **Pipeline de Lignes** : Finaliser l'intégration de la fusion dans la boucle principale de `motion.c` (branche `feat/cpu-pipeline-fusion`).

## 3. Environnement de Travail (Dalek Cluster)
- **Frontend** : `front.dalek.lip6.fr` (Compilation uniquement).
- **Noeuds de calcul** : Partition `az4-n4090` (AMD Ryzen 9 7945HX, 16 cœurs/32 threads, GPU RTX 4090).
- **Outils** : GCC 13.3, CMake 3.28, FFmpeg 6.1.
- **Validation** : `diff -r logs_ref logs_new` doit être vide.

## 4. Structure Git & Collaboration
- **Remote** : `https://github.com/Duttonn/ProjetHPC`
- **Branches (Découpage pour parallélisation)** :
    - `main` : Version stable validée.
    - `feat/gpu-acceleration` : Intégration OpenCL (Host code).
    - `feat/cpu-pipeline-fusion` : Fusion ΣΔ + Morpho et pipelinage par lignes.
    - `feat/bit-packing-integration` : Passage au format 1-bit dans la boucle principale.
    - `feat/reporting-viz` : Scripts Python, graphes de speedup et rapport.

## 5. Performances Actuelles (Sur az4-n4090)
| Résolution | Baseline (motion2) | Optimisé (motion) | Speedup |
|------------|--------------------|-------------------|---------|
| **1080p**  | 89 FPS             | 262 FPS           | **x2.94** |
| **4K**     | 29 FPS             | 67 FPS            | **x2.31** |

- **Gain ΣΔ seul** : x18.7 (grâce au SIMD AVX2).
- **Gain Morpho seul** : x9.0 (grâce au SIMD + Séparabilité).
- **Scaling** : Performance maximale atteinte à **8 threads OpenMP**.

## 6. Points de Vigilance (Anti-Debug)
- **Identité binaire** : Ne jamais modifier les seuils ou la logique ΣΔ.
- **Bords** : La morphologie ne traite pas les bords (copie simple). Attention aux accès `j-1` ou `i+1`.
- **MIPP** : Toujours vérifier que `MOTION_USE_MIPP` est défini, sinon le code retombe en scalaire lent.
- **OpenCL** : Les transferts CPU <-> GPU sont coûteux. Garder les données sur GPU le plus longtemps possible.
