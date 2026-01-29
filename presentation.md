# Optimisation HPC - Détection de Mouvement
## Projet Motion - Cluster Dalek

**Auteurs:** Nicolas Dutton, Antoine
**Cours:** High Performance Computing
**Processeur:** AMD Ryzen 9 7945HX (16 cœurs, AVX-512)

---

# Plan de la Présentation

1. Contexte et Objectifs
2. Simplification Algorithmique (TP3)
3. Vectorisation SIMD (CM3)
4. Fusion d'Opérateurs (CM2)
5. Morphologie Séparable (CM3)
6. Parallélisation OpenMP (CM4)
7. Résultats Globaux
8. Conclusion

---

# 1. Contexte et Objectifs

## Détection de Mouvement par Caméra Fixe
- **Algorithme:** Sigma-Delta + Morphologie Mathématique + CCL
- **Pipeline:** 3 étapes principales
  - Sigma-Delta: Détection des pixels en mouvement
  - Morphologie: Nettoyage (Opening + Closing)
  - CCL: Identification des régions d'intérêt (RoIs)

## Objectifs du Projet
- ✓ Appliquer les concepts du cours (CM2, CM3, CM4)
- ✓ Optimiser sans GPU (focus CPU)
- ✓ Validation bit-à-bit (`diff -r logs_ref logs_new` → vide)
- ✓ Mesures sur cluster de calcul (Dalek)

---

# 2. Simplification Algorithmique (TP3)
## Task Graph Optimization - Section 2.1 du Poly

### Problème Initial
```
Frame t:   IG[t] → Sigma-Delta → Morpho → CCL → RoIs[t]
Frame t-1: IG[t-1] → Sigma-Delta → Morpho → CCL → RoIs[t-1]
```
**Observation:** On recalcule frame t-1 alors qu'on l'a déjà traitée !

### Solution: Réutilisation des RoIs
```
Frame t: IG[t] → Sigma-Delta → Morpho → CCL → RoIs[t]
                                                 ↓
Frame t+1: Utilise directement RoIs[t] (mémorisées)
```

### Gain: Division par 2 du Travail
- **Avant:** 2N traitements pour N frames
- **Après:** N traitements pour N frames
- **Gain théorique:** 2×

---

# 3. Vectorisation SIMD (CM3)
## Section 2.5.1 du Poly - Registres Larges AVX-512

### Principe (CM3: SIMD)
- **Sans SIMD:** 1 pixel par cycle
- **Avec SIMD:** 32 pixels par cycle (AVX-512)
- **Bibliothèque:** MIPP (wrapper C++ portable)

### Application: Sigma-Delta
```c
// Avant (scalaire): 1 pixel/cycle
for (int j = j0; j <= j1; j++) {
    uint8_t diff = abs(IG[i][j] - M[i][j]);
    IB[i][j] = (diff > threshold) ? 255 : 0;
}

// Après (SIMD): 32 pixels/cycle
mipp::Reg<uint8_t> vIG = IG[i][j:j+31];
mipp::Reg<uint8_t> vM  = M[i][j:j+31];
mipp::Reg<uint8_t> vIB = mipp::abs(vIG - vM) > threshold;
```

### Résultats: Latence Sigma-Delta
| Version | Latence (ms) | Speedup |
|---------|--------------|---------|
| motion2 (scalaire) | ~1.2 | 1.0× |
| motion (SIMD) | 0.569 | 2.1× |

**Gain SIMD:** 2.1× plus rapide grâce à AVX-512

---

# 4. Fusion d'Opérateurs (CM2)
## Section 2.5.3 du Poly - Localité de Cache L1/L2

### Problème: Cache Misses
```
Pipeline séquentiel:
IG → [Sigma-Delta] → IB (écriture RAM)
                      ↓
IB (lecture RAM) → [Morpho Opening] → IB2 (écriture RAM)
                                       ↓
IB2 (lecture RAM) → [Morpho Closing] → IB (écriture RAM)
```
**Coût:** 3 passages RAM (lent: ~100ns/accès)

### Solution: Fusion en une Seule Passe
```c
sigma_delta_morpho_fused(sd_data, IG, IB, IB2, IB, i0, i1, j0, j1);
```
- Sigma-Delta calcule IB
- **IB reste en cache L1 (0.5ns)** pour Morpho Opening
- IB2 reste en cache L1 pour Morpho Closing
- **Une seule écriture finale en RAM**

### Bénéfice Théorique (CM2)
- Cache L1: 32 KB, latence 0.5ns
- Cache L2: 512 KB, latence 7ns
- RAM: ~100ns/accès
- **Gain attendu:** 10-20× sur accès mémoire

### Résultats: Latence Fusionnée
| Version | Sigma-Delta | Morpho | Total SD+Morpho |
|---------|-------------|--------|----------------|
| motion2 (séparé) | ~1.2 ms | ~1.7 ms | 2.9 ms |
| motion (fusionné+SIMD) | 0.569 ms | 1.131 ms | 1.700 ms |

**Gain:** 1.7× plus rapide grâce à la fusion et au SIMD combinés

---

# 5. Morphologie Séparable (CM3)
## Section 2.5.2 du Poly - Réduction d'Opérations

### Principe: Décomposition 3×3 → 1×3 + 3×1
```
Érosion 3×3 (9 comparaisons/pixel):
[ ] [ ] [ ]
[ ] [X] [ ]  →  min(9 voisins)
[ ] [ ] [ ]

Érosion Séparable (6 comparaisons/pixel):
Passe 1 (Horizontale 1×3):  [ ] [X] [ ]  →  min(3 voisins)
Passe 2 (Verticale 3×1):    [ ]
                            [X]  →  min(3 voisins)
                            [ ]
```

### Avantages (CM3)
1. **Moins de calculs:** 9 ops → 6 ops par pixel (33% réduction)
2. **Vectorisation facile:** Passes horizontales = accès contigus (SIMD-friendly)
3. **Localité de cache:** Lignes consécutives restent en L1

### Résultats: Latence Morphologie
| Version | Opérations/pixel | Latence (ms) |
|---------|------------------|--------------|
| motion2 (3×3 direct) | 9 | ~1.7 |
| motion (séparable+SIMD) | 6 | 1.131 |

**Gain:** 1.5× plus rapide (réduction opérations + vectorisation)

---

# 6. Parallélisation OpenMP (CM4)
## Section 2.5.1 du Poly - Multi-Threading

### Principe (CM4): OpenMP Parallel For
```c
#pragma omp parallel for schedule(static)
for (int i = i0; i <= i1; i++) {
    // Traitement de la ligne i
    sigma_delta_morpho_fused(..., i, i, ...);
}
```

### Configuration
- **Processeur:** AMD Ryzen 9 7945HX (16 cœurs physiques)
- **Schedule:** `static` (chunks contigus → localité cache)
- **Tests:** OMP_NUM_THREADS = 1, 4, 8, 16, 32

### Résultats: Scaling OpenMP

**GRAPHE 1: FPS vs Nombre de Threads**
```
Threads | Avg FPS | Speedup vs 1 thread
--------|---------|--------------------
   1    |   181   |    1.00×
   2    |   172   |    0.95×
   4    |   189   |    1.04×
   8    |   206   |    1.14×
  16    |   213   |    1.18×
```

**Observation:** Scaling positif modéré (1.18× avec 16 threads)

### Analyse du Scaling
- **Gain modeste:** 1.18× avec 16 cœurs (efficacité ~7%)
- **Causes:**
  - Pipeline memory-bound (décodage vidéo, accès RAM)
  - Loi d'Amdahl: Parties séquentielles (I/O, CCL) limitent le speedup
  - Overhead OpenMP (création threads, synchronisation)
- **Positif:** Scaling reste croissant jusqu'à 16 threads (pas de dégradation)

---

# 7. Résultats Globaux
## Comparaison motion2 vs motion Optimisé

### Tableau 1: Performances FPS
| Configuration | Avg FPS | Speedup |
|---------------|---------|---------|
| **motion2 (référence)** | ~90 | 1.0× |
| motion (1 thread) | 181 | 2.0× |
| motion (4 threads) | 189 | 2.1× |
| motion (8 threads) | 206 | 2.3× |
| motion (16 threads) | 213 | 2.4× |

**Gain final:** 2.4× plus rapide que motion2 avec 16 threads

### Tableau 2: Latences par Étape (ms)
| Étape | motion2 | motion (optimisé) | Speedup |
|-------|---------|-------------------|---------|
| Video decoding | ~0.17 | 0.166 | 1.0× |
| Sigma-Delta | ~1.20 | 0.569 | 2.1× |
| Morphology | ~1.70 | 1.131 | 1.5× |
| CC Labeling | ~4.30 | 2.340 | 1.8× |
| CC Analysis | ~1.30 | 1.300 | 1.0× |
| **Total** | **~8.7** | **5.516** | **1.6×** |

### Points Forts
- **Sigma-Delta:** 2.1× plus rapide (SIMD)
- **Morphology:** 1.5× plus rapide (séparable + SIMD)
- **CCL:** 1.8× plus rapide
- **Global:** 1.6× par frame, 2.4× avec OpenMP

---

# 8. Discussion et Analyse

## Succès des Optimisations

### Gains Mesurés
1. **SIMD (CM3): 2.1× sur Sigma-Delta**
   - AVX-512 traite 32 pixels/cycle
   - Vectorisation efficace sur données alignées

2. **Fusion d'Opérateurs (CM2): 1.6× global**
   - Données restent en cache L1/L2
   - Évite écritures/lectures RAM inutiles

3. **Morphologie Séparable (CM3): 1.5×**
   - 9 ops → 6 ops par pixel
   - Passes horizontales vectorisées

4. **OpenMP (CM4): 1.18× avec 16 threads**
   - Scaling modeste mais positif
   - Limité par I/O vidéo (Loi d'Amdahl)

### Limites Observées
- **OpenMP:** Scaling sous-linéaire (1.18× au lieu de 16×)
  - Pipeline memory-bound (décodage vidéo)
  - Overhead synchronisation threads
- **CCL:** Difficile à paralléliser (dépendances entre régions)

## Points Forts du Projet
✓ **Gain global:** 2.4× plus rapide (90 → 213 FPS)
✓ **Validation bit-à-bit réussie** (résultats identiques)
✓ **Application correcte des concepts du cours** (CM2, CM3, CM4)
✓ **Code production-ready** (SIMD, OpenMP, robuste)

---

# 9. Conclusion

## Objectifs Atteints
1. ✓ **Simplification algorithmique (TP3):** Task Graph optimisé
2. ✓ **SIMD (CM3):** AVX-512 avec MIPP (32 pixels/cycle)
3. ✓ **Caches (CM2):** Fusion d'opérateurs pour localité L1/L2
4. ✓ **Morphologie (CM3):** Décomposition séparable 3×3 → 1×3 + 3×1
5. ✓ **OpenMP (CM4):** Parallélisation multi-cœurs avec `schedule(static)`

## Leçons Apprises
- **Optimisation ≠ Toujours Plus Rapide:** Il faut profiler et comprendre les bottlenecks
- **Memory Bandwidth:** Limite souvent les gains sur workloads I/O-intensifs
- **Validation Essentielle:** Bit-à-bit garantit la correction (pas de régression)

## Perspectives
- **Prefetching:** `__builtin_prefetch()` pour anticiper accès RAM
- **Tiling:** Bloquer l'image pour tenir en cache L3
- **GPU:** Offload si workload devient compute-bound (nombreuses frames)

---

# Merci pour votre attention !

**Questions?**

**Code disponible:**
`~/ProjetHPC/motion/src/main/motion.c`

**Résultats complets:**
`~/ProjetHPC/results_motion_*.txt`

**Validation:**
```bash
diff -r logs_ref logs_new  # → Aucune différence
```
