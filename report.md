# Motion Detection HPC Project - Technical Report

**Course:** High Performance Computing - Polytech Sorbonne EI5-SE
**Project:** Motion Detection Optimization
**Author:** [Student Name]
**Date:** January 2026

---

## 1. Introduction

Ce projet porte sur l'optimisation d'une application de détection et de suivi de mouvement en temps réel. L'objectif est de maximiser le débit (FPS) tout en garantissant des résultats identiques bit-à-bit à la référence (`motion2`).

### Objectifs
1. **Simplifier** le graphe de tâches (TP3).
2. **Optimiser** via le parallélisme de données (SIMD) et de contrôle (OpenMP).
3. **Améliorer** la gestion de la mémoire (Localité de cache via le Pipelining).

---

## 2. Simplification du Graphe (TP3)

### Analyse du problème
L'implémentation originale traite deux images par itération ($I_t$ et $I_{t-1}$), ce qui double inutilement les calculs puisque $I_t$ est recalculée à l'itération suivante.

### Solution : Produce/Memorize
Nous avons implémenté un pattern où seule l'image courante est traitée. Les résultats (RoIs) sont mémorisés pour être utilisés à l'itération suivante. 
**Résultat :** Gain immédiat de **x2** en performance.

---

## 3. Optimisations CPU (CM2, CM3, CM4)

### 3.1 Vectorisation SIMD (CM3)
En utilisant la bibliothèque **MIPP**, nous exploitons les registres larges (AVX) du processeur AMD Ryzen 9.
- **Sigma-Delta :** Fusion des boucles et calcul de 32 pixels par instruction.
- **Morphologie :** Accès horizontaux optimisés pour le SIMD.

### 3.2 Fusion d'Opérateurs et Morphologie Séparable (CM2/CM3 : Localité de Cache)
Nous avons implémenté une **fusion d'opérateurs** via `sigma_delta_morpho_fused()` qui enchaîne Sigma-Delta + Opening + Closing en une seule passe.

#### Bénéfices pour la localité de cache (CM2):
- Les données traitées par Sigma-Delta restent **en cache L1/L2** pour être immédiatement utilisées par la morphologie.
- OpenMP `schedule(static)` donne à chaque thread des **chunks contigus de lignes**, garantissant un accès séquentiel optimal.
- On évite les écritures/relectures inutiles en RAM entre chaque étape du pipeline.

#### Morphologie séparable (CM3):
- Décomposition des opérations 3×3 en passes **horizontales (1×3)** puis **verticales (3×1)**.
- Réduction du nombre d'opérations par pixel: **9 → 6** comparaisons.
- Les passes horizontales sont **vectorisées avec SIMD** pour maximiser le débit.

### 3.3 Parallélisme OpenMP (CM4)
Nous avons utilisé `#pragma omp parallel for` pour distribuer le traitement des lignes sur les 16 cœurs physiques du processeur.
- Le choix d'un `schedule(static)` garantit que chaque thread traite des **lignes contigües**, optimisant la localité de cache.
- Toutes les passes (Sigma-Delta, érosions, dilatations) sont parallélisées au niveau des lignes.

---

## 4. Résultats Expérimentaux (Dalek Cluster)

### Configuration
- **CPU :** AMD Ryzen 9 7945HX (16 cœurs, AVX-512).
- **Compilateur :** GCC 13.3.0 avec `-O3 -march=native -fopenmp`.

### Performances (1080p)
| Version | FPS | Speedup |
|---------|-----|---------|
| motion2 (référence) | 89 | 1.0x |
| motion (optimisé) | **260** | **2.9x** |

---

## 5. Conclusion

Ce projet a permis de mettre en pratique les trois piliers du HPC enseignés en cours :
1. **L'efficacité algorithmique** (TP3).
2. **L'exploitation de l'architecture** (SIMD et Caches).
3. **Le parallélisme multi-cœurs** (OpenMP).

L'optimisation la plus marquante a été la **fusion d'opérateurs combinée au SIMD**, qui permet de:
1. Saturer la puissance de calcul du processeur (32 pixels/cycle)
2. Minimiser les transferts mémoire (localité de cache L1/L2)
3. Garantir la **validité bit-à-bit** des résultats (`diff -r logs_ref logs_new` retourne vide)