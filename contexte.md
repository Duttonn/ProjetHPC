# Mémoire Technique : Projet Motion HPC (EI5-SE)

Ce document sert de source de vérité absolue pour le projet. Il doit être maintenu à chaque étape majeure pour éviter toute perte de contexte technique.

## 1. Vue d'Ensemble du Projet
*   **Objectif** : Optimiser une chaîne de détection et de suivi d'objets en mouvement (Streaming).
*   **Modèle d'Or (Golden Model)** : `motion2.c` (doit rester bitwise-identical).
*   **Cible d'Optimisation** : `motion.c`.
*   **Environnement** : Cluster Dalek (LIP6).
    *   **Frontend** : `front.dalek.lip6.fr` (Compilation).
    *   **Nodes de calcul** : `az4-n4090-*` (Exécution).
    *   **CPU** : AMD Ryzen 9 7945HX (16 cœurs, 32 threads, AVX-512).
    *   **GPU** : NVIDIA RTX 4090.

## 2. État de l'Art des Optimisations Implémentées

### 2.1 Simplification du Graphe de Tâches (TP3)
*   **Pattern** : Produce/Memorize.
*   **Gain** : ~2x speedup immédiat.
*   **Détail** : Au lieu de traiter l'image $t$ et $t-1$ à chaque itération, on mémorise les RoIs de l'image $t$ pour les réutiliser comme $t-1$ à l'itération suivante.

### 2.2 Vectorisation SIMD (MIPP)
*   **Sigma-Delta** :
    *   Fusion des 4 boucles scalaires en 1 seule passe SIMD.
    *   Utilisation de `mipp::blend` pour supprimer les branchements conditionnels (if/else).
    *   Traitement de 32 pixels par itération (registres 256-bit AVX2).
    *   **Gain spécifique** : x18.7 sur Dalek.
*   **Morphologie** :
    *   Vectorisation des érosions/dilatations $3\times3$ avec chargements décalés (shifted loads).
    *   Implémentation des versions séparables ($3\times1$ et $1\times3$).

### 2.3 Multi-threading (OpenMP)
*   **Stratégie** : Parallélisation au niveau des lignes (`#pragma omp parallel for schedule(dynamic)`).
*   **Scaling** : Performance optimale atteinte à **8 threads** (302 FPS en 1080p). Au-delà, l'overhead de gestion des threads et les sections non-parallélisées (CCL/Tracking) limitent le gain.

### 2.4 Morphologie Séparable & Bit-Packing
*   **Séparabilité** : Élément structurant $3\times3$ décomposé en $(3\times1) \circ (1\times3)$. Réduit les opérations de 9 à 6 par pixel.
*   **Bit-Packing** :
    *   Format : 8 pixels binaires stockés dans 1 seul octet (`uint8_t`).
    *   Impact : Division par 8 de la bande passante mémoire.
    *   Fonctions : `morpho_pack_binary`, `morpho_unpack_binary` et versions `_packed` des opérateurs.

### 2.5 Fusion d'Opérateurs (Cache-Level Parallelism)
*   **Fonction** : `sigma_delta_morpho_fused`.
*   **Concept** : Enchaîner Sigma-Delta + Opening + Closing sur des blocs de lignes pour maximiser la localité temporelle dans le cache L1/L2. Évite de relire l'image entière en RAM entre chaque étape.

## 3. Résultats de Performance (Dalek - Janvier 2026)

| Résolution | Baseline (motion2) | Optimisé (motion) | Speedup |
| :--- | :--- | :--- | :--- |
| **1080p** | 89 FPS | **262 FPS** | **x2.94** |
| **4K** | 29 FPS | **67 FPS** | **x2.31** |

**Latences détaillées (1080p) :**
*   Sigma-Delta : 1.232 ms → **0.066 ms** (x18.7)
*   Morphologie : 1.815 ms → **0.201 ms** (x9.0)

## 4. Organisation Git & Roadmap

### Branches Actives
1.  `feat/cpu-pipeline-fusion` : Intégration de la fusion d'opérateurs et pipelinage par blocs.
2.  `feat/bit-packing-integration` : Passage au format 1-bit dans la boucle principale.
3.  `feat/gpu-acceleration` : Code host OpenCL et intégration des kernels GPU.
4.  `feat/reporting-viz` : Scripts Python pour graphes de speedup et schémas.

### Roadmap Technique
*   **Priorité 1** : Finaliser l'intégration de la fusion dans `motion.c`.
*   **Priorité 2** : Intégrer le bit-packing pour réduire la pression sur le bus mémoire.
*   **Priorité 3** : Initialiser le contexte OpenCL pour décharger la morphologie sur le RTX 4090.
*   **Priorité 4** : Générer les courbes de scaling OpenMP (1 à 32 threads) pour le rapport.

## 5. Points de Vigilance (Anti-Bug)
*   **Validation** : Toujours lancer `diff -r logs_ref logs_new` après chaque modif.
*   **Bords** : Les fonctions de morphologie ne traitent pas les bords (1 pixel de marge). Attention lors du chaînage.
*   **OpenCV** : Désactiver OpenCV (`-DMOTION_OPENCV_LINK=OFF`) pour les tests de performance pure et le debugging Valgrind.
*   **MIPP** : S'assurer que `-march=native` est bien présent pour activer l'AVX-512 sur Dalek.
