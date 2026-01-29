# Optimisation HPC - Détection de Mouvement
## Projet Motion - Du Séquentiel au Parallèle

**Auteurs:** Nicolas Dutton, Antoine
**Cours:** High Performance Computing - EI5-SE
**Date:** Janvier 2026

---

# Plan de la Présentation

1. **Architecture de Test**
2. **Contexte et Objectifs**
3. **Simplification Algorithmique**
4. **Validation Bit-à-Bit**
5. **Vectorisation SIMD**
6. **Fusion d'Opérateurs**
7. **Morphologie Séparable**
8. **Parallélisation OpenMP**
9. **Résultats Globaux**
10. **Discussion et Analyse**
11. **Conclusion**

---

# 1. Architecture de Test - Cluster Dalek

## Processeur: AMD Ryzen 9 7945HX

### Spécifications Techniques
- **Cœurs:** 16 cœurs physiques / 32 threads logiques
- **Fréquence:** 2.5 GHz base, jusqu'à 5.4 GHz boost
- **Architecture:** Zen 4 (5nm)
- **ISA:** x86-64 avec extensions AVX-512

---

## Hiérarchie Mémoire

- **Cache L1:** 32 KB instruction + 32 KB données par cœur
  - Latence: **0.5 ns** (~1-2 cycles)
- **Cache L2:** 1 MB par cœur
  - Latence: **7 ns** (~15 cycles)
- **Cache L3:** 64 MB partagé entre tous les cœurs
  - Latence: **40 ns** (~100 cycles)
- **RAM:** DDR5-4800 (38 GB/s bande passante)
  - Latence: **~100 ns** (~250 cycles)

---

# 2. Contexte et Objectifs

## Détection de Mouvement par Caméra Fixe

### Pipeline de Traitement (3 étapes)
```
Grayscale Image
     ↓
[1. Sigma-Delta] → Détection pixels en mouvement
     ↓ (Image binaire 0/1)
[2. Morphologie] → Nettoyage (Opening + Closing)
     ↓ (Image binaire nettoyée)
[3. CCL + CCA] → Identification des régions (RoIs)
     ↓
   Tracking
```

---

# 3. Simplification Algorithmique 

## Problème: Redondance des Calculs

### Graphe Initial (motion2)
```
Frame t-1:  I[t-1] → Σ∆ → Morpho → CCL → RoIs[t-1]
                                              ↓
Frame t:    I[t]   → Σ∆ → Morpho → CCL → RoIs[t]
                                              ↓
                                         [k-NN Matching]
```

**Observation Critique:**
À l'itération t, on traite **I[t] ET I[t-1]**
Mais I[t-1] a déjà été traité à l'itération t-1 !

---

## Solution: Produce/Memorize Pattern

### Solution: Produce/Memorize Pattern

```
Frame t:    I[t]   → Σ∆ → Morpho → CCL → RoIs[t]
                                         ↓ (mémorisé)
Frame t+1:  I[t+1] → Σ∆ → Morpho → CCL → RoIs[t+1]
                                         ↓
                    [k-NN Matching avec RoIs[t] mémorisé]
```

---

## Gain de la Simplification

### Pourquoi c'est Plus Rapide?

**Avant (motion2):**
- N frames → 2N passages dans le pipeline (t et t-1)
- Travail: **2N**

**Après (motion):**
- N frames → N passages dans le pipeline
- Travail: **N**

**Gain théorique: 2× (division par 2 du travail)**

---

# 4. Validation Bit-à-Bit

```
motion2 (référence)          motion (optimisé)
        │                            │
        ▼                            ▼
  ./bin/motion2 --log-path     ./bin/motion --log-path
        │                            │
        ▼                            ▼
    logs_ref/                    logs_new/
        │                            │
        └──────────┐  ┌──────────────┘
                   ▼  ▼
              diff logs_ref logs_new
                     │
                     ▼
        Résultat: VIDE = IDENTIQUE ✓
```

**Nos optimisations accélèrent SANS changer les résultats**

---

# 5. Vectorisation SIMD

## Principe: Data-Level Parallelism

### Exécution Scalaire (1 pixel/cycle)
```
Cycle 1: |  P0  |
Cycle 2: |  P1  |
Cycle 3: |  P2  |
...
Cycle 32:|  P31 |
```
**Temps total: 32 cycles pour 32 pixels**


---

### Exécution SIMD AVX-512 (32 pixels/cycle)
```
Cycle 1: | P0 | P1 | P2 | ... | P31 |
```
**Temps total: 1 cycle pour 32 pixels**
**Gain théorique: 32×**

## Code Scalaire vs SIMD

---


## Pourquoi SIMD est Plus Rapide?

### 1. Parallélisme de Données
- **32 pixels traités en parallèle** dans un seul registre 512 bits
- Même instruction appliquée à 32 données simultanément

### 2. Moins d'Instructions Exécutées
- Scalaire: 1920 itérations de boucle
- SIMD: 60 itérations de boucle
- **Réduction 32× du nombre d'itérations**

### 3. Meilleure Utilisation du CPU
- Unités de calcul SIMD dédiées (saturées)
- Pipeline CPU rempli efficacement

---

## Résultats SIMD

### Résultats: Latence Sigma-Delta

```
┌─────────────────────┬──────────────┬─────────┐
│ Version             │ Latence (ms) │ Speedup │
├─────────────────────┼──────────────┼─────────┤
│ motion2 (scalaire)  │     1.20     │  1.0×   │
│ motion (SIMD AVX)   │     0.569    │  2.1×   │
└─────────────────────┴──────────────┴─────────┘
```

**Gain réel: 2.1× (au lieu de 32× théorique)**

### Pourquoi pas 32×?
- Overhead load/store mémoire
- Latence accès RAM (même avec cache)
- Dépendances de données
---

# 6. Fusion d'Opérateurs

## Problème: Cache Misses = Latence RAM

### Pipeline Séquentiel (motion2)

```
Étape 1: Sigma-Delta
  IG (RAM) ──→ [CPU] ──→ IB (RAM écriture)
                              ↓ (100ns latence!)
Étape 2: Morpho Opening
  IB (RAM lecture) ──→ [CPU] ──→ IB2 (RAM écriture)
                                     ↓ (100ns latence!)
Étape 3: Morpho Closing
  IB2 (RAM lecture) ──→ [CPU] ──→ IB (RAM écriture)
```

**Coût: 3 écritures RAM + 2 lectures RAM = 5 × 100ns = 500ns par pixel**

---

## Hiérarchie Mémoire - Rappel

### Hiérarchie Mémoire - Rappel

```
L1 Cache:  0.5ns   |████████████████████████████████| Rapide!
L2 Cache:  7ns     |███|
L3 Cache:  40ns    |
RAM:       100ns                                       | Lent!
```

**Ratio: RAM = 200× plus lent que L1 !**

---

## Solution: Fusion en Une Seule Passe

```
IG (RAM) ──→ [Σ∆] ──→ IB (L1 Cache 0.5ns)
                        ↓
                    [Opening] ──→ IB2 (L1 Cache 0.5ns)
                                   ↓
                               [Closing] ──→ IB (RAM écriture)
```

**Coût: 1 lecture RAM + 1 écriture RAM = 2 × 100ns = 200ns par pixel**

---

## Pourquoi la Fusion Accélère?

- **Accès RAM:** 5 → 2 (gain 2.5×)
- **Localité temporelle:** données restent en cache L1 entre étapes
- **Latence:** L1 = 0.5ns vs RAM = 100ns (200× plus rapide)
- **Synergie SIMD:** cache L1 alimente les unités vectorielles à pleine vitesse

---

## Résultats de la Fusion

### Résultats: Latence Fusionnée

```
┌──────────────────┬─────────────┬─────────┬──────────────────┐
│ Version          │ Sigma-Delta │ Morpho  │ Total SD+Morpho  │
├──────────────────┼─────────────┼─────────┼──────────────────┤
│ motion2 (séparé) │   1.20 ms   │ 1.70 ms │     2.90 ms      │
│ motion (fusionné)│   0.569 ms  │ 1.131ms │     1.700 ms     │
└──────────────────┴─────────────┴─────────┴──────────────────┘
```

**Gain: 1.7× (2.9ms → 1.7ms)**

---

# 7. Morphologie Séparable
### Morphologie 3×3 Standard (9 comparaisons/pixel)

```
Érosion 3×3: min de 9 voisins
┌───┬───┬───┐
│ 1 │ 2 │ 3 │  →  result = min(1,2,3,4,5,6,7,8,9)
├───┼───┼───┤
│ 4 │ X │ 5 │      9 comparaisons par pixel!
├───┼───┼───┤
│ 6 │ 7 │ 8 │
└───┴───┴───┘
```

**Image 1920×1080: 1920 × 1080 × 9 = 18.6M comparaisons**

---

## Morphologie Séparable (6 comparaisons/pixel)
```
Décomposition: 3×3 = (1×3) ∘ (3×1).        

Passe 1 - Horizontale 1×3:                  Passe 2 - Verticale 3×1:
┌───┬───┬───┐
│ A │ X │ B │  →  temp = min(A, X, B)
└───┴───┴───┘
   3 comparaisons                                 ┌───┐
                                                  │ C │
                                                  ├───┤
                                                  │temp│  →  result = min(C, temp, D)
                                                  ├───┤
                                                  │ D │
                                                  └───┘
                                                     3 comparaisons   
```
**Total: 6 comparaisons au lieu de 9**
**Image 1920×1080: 1920 × 1080 × 6 = 12.4M comparaisons**

---

## Avantages de la Séparabilité

### Pourquoi c'est Plus Rapide?

### 1. Réduction du Nombre d'Opérations
- **9 ops → 6 ops = réduction 33%**
- Moins de calculs = moins de temps CPU

### 2. Vectorisation Optimale (Passe Horizontale)

**Accès contigus = cache-friendly + SIMD-friendly**


---

## Résultats Morphologie Séparable

### Résultats: Latence Morphologie.

```
┌──────────────────────┬──────────────────┬──────────────┐
│ Version              │ Ops/pixel        │ Latence (ms) │
├──────────────────────┼──────────────────┼──────────────┤
│ motion2 (3×3 direct) │        9         │     1.70     │
│ motion (séparable)   │        6         │     1.131    │
└──────────────────────┴──────────────────┴──────────────┘
```

**Gain: 1.5× (1.7ms → 1.13ms)**

### Bonus: Combinaison avec Fusion
- Morpho séparable + Fusion d'opérateurs
- Passes horizontales restent en cache L1
- **Synergie des optimisations!**

---

# 8. Parallélisation OpenMP

## Principe: Décomposition de Domaine

### Architecture: 16 Cœurs Physiques

```
   ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
   │Core0│Core1│Core2│Core3│Core4│Core5│Core6│Core7│
   └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
┌─────┬─────┬──────┬──────┬──────┬──────┬──────┬──────┐
│Core8│Core9│Core10│Coer11│Core12│Core13│Core14│Core15│
└─────┴─────┴──────┴──────┴──────┴──────┴──────┴──────┘
            Ryzen 9 7945HX (16 cœurs)
```

**Idée:** Distribuer le traitement sur les 16 cœurs

---

## Implémentation OpenMP

### Implémentation: OpenMP Parallel For

```c
#pragma omp parallel for schedule(static)
for (int i = i0; i <= i1; i++) {
    // Chaque thread traite un sous-ensemble de lignes
    sigma_delta_morpho_fused(..., i, i, ...);
}
```

**Avec 16 threads → chaque thread traite 1080/16 = 67 lignes**

---

## Stratégie de Scheduling

```
Image 1080 lignes, 16 threads:

Thread 0:  lignes 0-67    ┐
Thread 1:  lignes 68-135  │ Chunks contigus
Thread 2:  lignes 136-203 │ → Cache locality
...                        │
Thread 15: lignes 1013-1080┘
```

**Avantage:** Chaque thread traite des **lignes consécutives**
- Cache L2 partagé entre threads proches
- Moins de cache conflicts
- Prédictibilité (pas de vol de travail)

---

## Pourquoi OpenMP Accélère?

### 1. Exploitation du Parallélisme Matériel
- 16 cœurs disponibles
- Calculs indépendants (lignes séparées)

### 2. Scalabilité Théorique
- **16 threads:** T/16 secondes = 16x plus rapide

### 3. Localité de Cache
- chunks contigus

---

## Résultats OpenMP Scaling

### Graphe 1: FPS vs Nombre de Threads

<p align="center"><img src="graph_openmp_scaling_fps.png" alt="OpenMP Scaling - FPS" width="72%"></p>

---

### Graphe 2: Speedup vs Nombre de Threads

<p align="center"><img src="graph_openmp_speedup.png" alt="OpenMP Scaling - Speedup" width="72%"></p>

---

### Tableau Détaillé

```
┌─────────┬─────────┬──────────────────┬────────────┐
│ Threads │ Avg FPS │ Speedup vs 1t    │ Efficacité │
├─────────┼─────────┼──────────────────┼────────────┤
│    1    │   181   │      1.00×       │   100%     │
│    2    │   172   │      0.95×       │    48%     │
│    4    │   189   │      1.04×       │    26%     │
│    8    │   206   │      1.14×       │    14%     │
│   16    │   213   │      1.18×       │     7%     │
└─────────┴─────────┴──────────────────┴────────────┘
```

**Gain réel: 1.18× avec 16 threads (au lieu de 16× idéal)**

---

## Pourquoi Scaling Modeste?

### 1. Loi d'Amdahl

```
Speedup = 1 / ((1-P) + P/N)

Avec:
- P = fraction parallélisable
- N = nombre de threads
- (1-P) = fraction séquentielle
```

**Notre cas:**
- Décodage vidéo: séquentiel (buffered mais coûteux)
- CCL: bottleneck (2.34ms, ~42% du temps)
- k-NN matching: séquentiel
- **Partie parallélisable estimée: P ≈ 50%**

**Speedup max théorique: 1/(0.5) = 2×**

---

## Limites du Parallélisme

### 2. Memory Bandwidth Bottleneck

```
16 cœurs × 38 GB/s RAM / 16 = 2.4 GB/s par cœur

Image 1920×1080×3 (RGB) = 6.2 MB
100 FPS → 620 MB/s par cœur nécessaire

Avec 16 threads:
16 × 620 MB/s = 9.9 GB/s < 38 GB/s (OK)

MAIS: Accès non parfaits (cache misses, false sharing, ...)
→ Saturation partielle de la bande passante
```
---

### 3. Overhead OpenMP
- Création/synchronisation threads: ~1-2 µs
- Barrières implicites (fin de parallel for)
- Cache coherency protocol (MESI)

### 4. Workload Memory-Bound (pas Compute-Bound)
- CPU attend la RAM plus qu'il ne calcule
- Ajouter plus de threads ne résout pas le problème RAM

---

## Conclusion OpenMP


**Scaling modeste (1.18×) mais:**
- ✓ Scaling **positif** (pas de dégradation)
- ✓ Combiné avec SIMD+Fusion = **2.4× total**

**C'est normal en HPC** Memory-bound workloads ne scalent jamais linéairement.

---

# 9. Résultats Globaux

## Graphe 1: Performance FPS par Configuration

<p align="center"><img src="graph_fps_comparison.png" alt="Comparaison FPS - Toutes Configurations" width="72%"></p>


---

## Tableau Performances FPS

**Gain final: 2.4× (90 → 213 FPS)**

```
┌─────────────────────────┬─────────┬─────────┐
│ Configuration           │ Avg FPS │ Speedup │
├─────────────────────────┼─────────┼─────────┤
│ motion2 (référence)     │   ~90   │  1.0×   │
│ motion (1 thread)       │   181   │  2.0×   │
│ motion (2 threads)      │   172   │  1.9×   │
│ motion (4 threads)      │   189   │  2.1×   │
│ motion (8 threads)      │   206   │  2.3×   │
│ motion (16 threads)     │   213   │  2.4×   │
└─────────────────────────┴─────────┴─────────┘
```

---

## Décomposition des Latences

<p align="center"><img src="graph_latencies_comparison.png" alt="Latences par Étape - motion2 vs motion" width="72%"></p>

---

## Latences Empilées (Breakdown Total)

<p align="center"><img src="graph_latency_stacked.png" alt="Latences Empilées - Décomposition" width="72%"></p>

---

## Comparaison:
- motion2: 1.20ms (Σ∆) + 1.70ms (Morpho) + 4.30ms (CCL) = 8.67ms total
- motion: 0.569ms (Σ∆) + 1.131ms (Morpho) + 2.340ms (CCL) = 5.516ms total

---

## Tableau Latences Détaillées

```
┌──────────────────┬─────────┬──────────────────┬─────────┐
│ Étape            │ motion2 │ motion (optimisé)│ Speedup │
├──────────────────┼─────────┼──────────────────┼─────────┤
│ Video decoding   │ 0.17 ms │     0.166 ms     │  1.0×   │
│ Sigma-Delta      │ 1.20 ms │     0.569 ms     │  2.1×   │ ← SIMD
│ Morphology       │ 1.70 ms │     1.131 ms     │  1.5×   │ ← Séparable+SIMD
│ CC Labeling      │ 4.30 ms │     2.340 ms     │  1.8×   │
│ CC Analysis      │ 1.30 ms │     1.300 ms     │  1.0×   │
├──────────────────┼─────────┼──────────────────┼─────────┤
│ TOTAL            │ 8.67 ms │     5.516 ms     │  1.6×   │
└──────────────────┴─────────┴──────────────────┴─────────┘
```

---

## Contribution de Chaque Optimisation

```
90 FPS ──→ Task Graph (2.0×) ──→ SIMD+Fusion+Séparable ──→ OpenMP ──→ 213 FPS
(1.0×)                                                                  (2.4×)
```

- **Task Graph:** 2× -- suppression des calculs redondants
- **SIMD:** 2.1× sur Sigma-Delta -- 32 pixels/cycle (AVX-512)
- **Fusion:** 1.7× sur SD+Morpho -- cache L1 au lieu de RAM
- **Morpho Séparable:** 1.5× -- 9 ops → 6 ops par pixel
- **OpenMP:** 1.18× -- 16 cœurs, scaling limité par Amdahl

---

# 10. Discussion et Analyse

### Ce qui a Bien Marché

✓ **Fusion d'Opérateurs** - L'optimisation MVP
  - Exploite la hiérarchie mémoire
  - Cache L1: 200× plus rapide que RAM
  - Combinaison naturelle avec SIMD

✓ **SIMD** - Gain significatif
  - AVX-512: 32 pixels/cycle
  - Bibliothèque MIPP: portabilité
  - Code lisible et maintenable

✓ **Task Graph** - Low-hanging fruit
  - Gain immédiat sans complexité
  - Correctness proof simple

---

## Limites Rencontrées

### 1. Scaling OpenMP Modeste (1.18×)

**Causes Identifiées:**

**A) Loi d'Amdahl**
```
Speedup_max = 1 / (S + P/N)

Avec S = 0.5 (50% séquentiel)
     P = 0.5 (50% parallèle)
     N = 16 threads

Speedup_max = 1 / (0.5 + 0.5/16) = 1.88×
Speedup_réel = 1.18×
```
**Efficacité: 60% du maximum théorique**

**B) Memory Bandwidth Bottleneck**
- 16 cœurs accèdent simultanément à la RAM
- Saturation partielle de la bande passante

---

## Écart Théorique/Pratique

```
┌─────────────┬───────────┬────────────┬───────────┐
│ Optimisation│ Théorique │   Réel     │ Ratio     │
├─────────────┼───────────┼────────────┼───────────┤
│ SIMD        │    32×    │    2.1×    │   6.5%    │
│ OpenMP      │    16×    │    1.18×   │   7.4%    │
└─────────────┴───────────┴────────────┴───────────┘
```

**Pourquoi cet écart?**
- **Latence mémoire** (goulot d'étranglement)
- **Overhead** (load/store, synchronisation)
- **Partie séquentielle** (Amdahl)

**C'est normal** Les gains théoriques supposent:
- Calcul infiniment rapide 
- Mémoire infiniment rapide 
- Parallélisme parfait

<!-- --- -->

<!-- ## Optimisations Non Implémentées

### 3. Optimisations Non Implémentées

**GPU (Section 2.5.6 du PDF)**
- Raison: CPU suffit (213 FPS > 30 FPS requis)
- Latence CPU↔GPU non justifiée
- Complexité d'implémentation élevée

**Bit-Packing (Section 2.5.4 du PDF)**
- Raison: Complexité vs gain incertain
- Difficile à expliquer/débugger
- Risque pour la validation

**Pipeline de Row Operators (Section 2.5.3 détaillé)**
- Partiellement implémenté via fusion
- Prologue/épilogue complexe
- Gain marginal après fusion -->

---

# 11. Conclusion

### Objectifs Initiaux

✓ **Appliquer les concepts du cours**
  - Caches: Fusion d'opérateurs → **1.7× gain**
  - SIMD: AVX-512 vectorisation → **2.1× gain**
  - Algo: Morpho séparable → **1.5× gain**
  - OpenMP: Parallélisation → **1.18× gain**

✓ **Validation bit-à-bit réussie**
  - `diff -r logs_ref logs_new` → vide ✓
  - Aucune régression fonctionnelle

✓ **Mesures sur cluster Dalek**
  - AMD Ryzen 9 7945HX (16 cœurs)
  - Benchmarks reproductibles

---

## Performance Finale

### Gain Global: 2.4× (90 → 213 FPS)

```
┌────────────────────┬──────────┬──────────┐
│ Métrique           │ motion2  │  motion  │
├────────────────────┼──────────┼──────────┤
│ FPS moyen          │   90     │   213    │
│ Latence/frame      │  8.67 ms │  5.52 ms │
│ Throughput         │  1.0×    │  2.4×    │
└────────────────────┴──────────┴──────────┘
```

<!-- ---

## L'Optimisation Clé

**Pourquoi c'est l'optimisation la plus importante?**

1. **Exploite la ressource critique** (cache L1)
2. **Synergie avec SIMD** (alimente les unités vectorielles)
3. **Gain significatif** (1.7×) avec code simple
4. **Enseignement du CM2** parfaitement appliqué

**"Cache is king in modern HPC"**

---

## Messages Clés - Oral

### Messages Clés à Retenir

**Pour l'Oral:**

1. **Validation garantie**
   - `diff` vide = résultats identiques
   - Pas d'approximation

2. **Lien avec le cours explicite**
   - "Cette optimisation applique le CM2 sur..."
   - Concepts théoriques → mise en pratique

3. **Scaling modeste expliqué**
   - Loi d'Amdahl (partie séquentielle)
   - Memory-bound (pas compute-bound)
   - **C'est normal en HPC!**

   4. **Choix CPU-only justifié**
   - 213 FPS > 30 FPS requis
   - GPU = overhead non justifié
   - Focus sur optimisations CPU -->

<!-- ## Messages Clés - Rapport



**Pour le Rapport:**

1. **Méthodologie rigoureuse**
   - Baseline claire (motion2)
   - Mesures reproductibles (--vid-in-buff, --stats)
   - Validation systématique

2. **Résultats chiffrés**
   - Tableaux de latences
   - Graphes de scaling
   - Gains par optimisation

--- -->

<!-- ## Analyse Critique et Perspectives

3. **Analyse critique**
   - Limites identifiées
   - Écart théorique/pratique expliqué
   - Perspectives d'amélioration

### Perspectives

### Court Terme (Extensions Possibles)

1. **Prefetching** - Anticipation des accès mémoire
2. **Cache Blocking** - Tiling pour L3
3. **CCL Parallèle** - Union-Find distribué

### Long Terme (Si FPS > 300 Requis)

1. **GPU Offload** - OpenCL/CUDA
2. **Hétérogène CPU+GPU** - Split workload
3. **FPGA** - Pipeline matériel dédié -->

---

# Merci pour votre Attention!

<!-- ## Questions?

---

## Ressources et Références

### Code Source
- **Repository:** `~/ProjetHPC/motion/`
- **Fichier principal:** `src/main/motion.c`
- **Commit validé:** bit-for-bit identical to motion2

### Résultats Complets
- **Logs de validation:** `logs_ref/` vs `logs_new/`
- **Benchmarks:** `results_motion_*.txt`
- **Graphes:** Générés à partir des CSVs

### Validation
```bash
# Commande de validation (toujours disponible)
diff -r logs_ref logs_new
# → Retour vide = Succès ✓
```

### Benchmark Reproductible
```bash
# Commande pour reproduire les résultats
export OMP_NUM_THREADS=16
./bin/motion --vid-in-buff --vid-in-stop 100 \
  --vid-in-path traffic/1080p_day_street_top_view_snow.mp4 \
  --stats
```

---

## Contact et Environnement

### Contact
- **Cluster:** Dalek (Polytech Sorbonne)
- **Processeur:** AMD Ryzen 9 7945HX
- **Environnement:** GCC 13.3.0, -O3 -march=native -fopenmp

---

## Annexe: Commandes Utiles

### Compilation
```bash
module load gcc/13.3.0 cmake opencv/4.10.0
cd motion
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DMOTION_OPENCV_LINK=OFF \
  -DMOTION_USE_MIPP=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -fopenmp" \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -fopenmp"
cmake --build build --parallel
```

### Tests de Validation
```bash
# Générer référence
./bin/motion2 --vid-in-stop 20 \
  --vid-in-path ../traffic/1080p.mp4 \
  --log-path logs_ref

# Générer optimisé
./bin/motion --vid-in-stop 20 \
  --vid-in-path ../traffic/1080p.mp4 \
  --log-path logs_new

# Comparer
diff -r logs_ref logs_new
```

---

## Benchmark Multi-Threads

### Benchmark avec Différents Threads
```bash
for THREADS in 1 2 4 8 16; do
    export OMP_NUM_THREADS=$THREADS
    ./bin/motion --vid-in-buff --vid-in-stop 100 \
      --vid-in-path ../traffic/1080p.mp4 \
      --stats | tee results_${THREADS}t.txt
done
```

---

# FIN

**Objectif 17-18/20:** Tous les concepts du cours appliqués ✓
**Performance:** 2.4× gain (90 → 213 FPS) ✓
**Validation:** Bit-à-bit identical ✓
**Explicabilité:** Chaque optimisation justifiée ✓

**Bonne chance pour la soutenance! 🚀** -->
