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

### 3.2 Pipelining par Blocs (CM2 : Localité de Cache)
Au lieu de traiter l'image entière pour chaque étape (Sigma-Delta, puis Erosion, puis Dilation), nous traitons l'image par **blocs de 16 lignes**.
- **Pourquoi ?** Pour que les données restent dans le **Cache L1/L2**. Comme vu en cours (CM2), accéder à la RAM est beaucoup plus lent que d'accéder au cache. En traitant par blocs, on évite les "Cache Misses".

### 3.3 Parallélisme OpenMP (CM4)
Nous avons utilisé `#pragma omp parallel for` pour distribuer le traitement des lignes sur les 16 cœurs physiques du processeur.
- Le choix d'un `schedule(dynamic)` permet d'équilibrer la charge entre les cœurs.

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

L'optimisation la plus marquante a été la combinaison du SIMD et du Pipelining, qui permet de saturer la puissance de calcul du processeur tout en minimisant les transferts mémoire.