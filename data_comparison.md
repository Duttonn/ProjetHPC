# Comparaison des Résultats de Benchmark

## Dataset 1: CSV existant (2.5.1)
**Configuration:** 1080p, 200 frames

### motion2 vs motion (1 thread)
| Version | FPS | Gain |
|---------|-----|------|
| motion2 (baseline) | 90 | 1.0× |
| motion (optimisé) | 144-160 | 1.6× |

### Scaling OpenMP - motion
| Threads | FPS | Speedup |
|---------|-----|---------|
| 1 | 144 | 1.00× |
| 2 | 121 | 0.84× |
| 4 | 119 | 0.83× |
| 8 | 66 | 0.46× |
| 16 | 88 | 0.61× |

**Observation:** Scaling négatif prononcé

---

## Dataset 2: CSV existant (2.5.2)
**Configuration:** 1080p

### Scaling OpenMP - motion
| Threads | Avg FPS | Speedup |
|---------|---------|---------|
| 1 | 181 | 1.00× |
| 2 | 172 | 0.95× |
| 4 | 189 | 1.04× |
| 8 | 206 | 1.14× |
| 16 | 213 | 1.18× |

### Latences par Étape (ms)
| Étape | Temps (ms) |
|-------|------------|
| Video decoding | 0.166 |
| Sigma-Delta | 0.569 |
| Morphology | 1.131 |
| CC Labeling | 2.340 |
| CC Analysis | 1.300 |
| **Total** | **5.516** |

**Observation:** Scaling positif modéré (1.18× avec 16 threads)

---

## Dataset 3: Nos Benchmarks Dalek (Janvier 2026)
**Configuration:** 1080p, 100 frames, `traffic/1080p_day_street_top_view_snow.mp4`
**Hardware:** AMD Ryzen 9 7945HX, Cluster Dalek

### motion2 vs motion (1 thread)
| Version | Avg FPS | Total FPS |
|---------|---------|-----------|
| motion2 | 85 | 86.01 |
| motion (1t) | 110 | 70.05 |

**Gain:** 1.29× en Avg FPS

### Scaling OpenMP - motion
| Threads | Avg FPS | Speedup |
|---------|---------|---------|
| 1 | 110 | 1.00× |
| 4 | 107 | 0.97× |
| 8 | 105 | 0.95× |
| 16 | 101 | 0.92× |
| 32 | 94 | 0.85× |

### Latences par Étape (ms)
| Étape | motion2 | motion (1t) | Speedup |
|-------|---------|-------------|---------|
| Sigma-Delta | 1.200 | 5.235* | 0.23× |
| Morphology | 1.706 | 5.235* | 0.33× |
| CC Labeling | 4.325 | 1.696 | 2.55× |
| **Total** | **11.627** | **14.277** | **0.81×** |

*Sigma-Delta et Morphology fusionnés dans motion

**Observation:** Scaling négatif, mais CCL 2.5× plus rapide

---

## Analyse des Différences

### Pourquoi 3 datasets différents?

1. **Dataset 2.5.1 vs Dataset 3 (nos tests):**
   - **FPS différents:** 144 vs 110 (1 thread)
   - **Cause probable:** Options de compilation ou vidéo différente
   - Dataset 2.5.1 montre des FPS plus élevés mais scaling encore pire

2. **Dataset 2.5.2 vs Dataset 2.5.1:**
   - **Scaling inverse:** Positif (1.18×) vs Négatif (0.61×)
   - **Latences:** Beaucoup plus rapides (5.5ms vs 14.3ms total)
   - **Hypothèse:** Section 2.5.2 = implémentation différente ou flags optimisés

3. **Dataset 2.5.2 vs Dataset 3:**
   - Latences 2.5.2 beaucoup plus rapides (5.5ms vs 14.3ms)
   - FPS 2.5.2 beaucoup plus élevés (181-213 vs 94-110)
   - **Question:** Le code utilisé pour 2.5.2 est-il différent?

---

## Recommandations pour la Présentation

### Option A: Utiliser Dataset 2.5.2 (meilleurs résultats)
✓ Montre scaling positif (1.18× avec 16 threads)
✓ Latences faibles (5.5ms total)
✓ FPS élevés (181-213)
✗ Mais: Incohérent avec nos tests récents

### Option B: Utiliser Dataset 3 (nos tests Dalek)
✓ Résultats vérifiables (fichiers .txt disponibles)
✓ Validation bit-à-bit confirmée
✓ Gain CCL prouvé (2.5×)
✗ Mais: Scaling négatif, performances globales inférieures

### Option C: Présenter les deux avec explication
✓ Honnête et transparent
✓ Montre l'importance des conditions de test
✓ Explique variabilité des benchmarks HPC
- **Message:** "Selon les options de compilation et la vidéo, les résultats varient"

---

## Décision Suggérée

**Utiliser Dataset 2.5.2 pour la présentation** si vous avez:
1. Le code source correspondant
2. Les flags de compilation utilisés
3. La confirmation que c'est votre implémentation finale

**Sinon, utiliser Dataset 3** (nos tests) avec:
- Fichiers de résultats vérifiables
- Validation bit-à-bit prouvée
- Explication du scaling négatif (memory-bound)

**Message clé pour l'oral:**
"Les benchmarks HPC sont sensibles aux conditions (compilation, vidéo, charge système).
L'important est la **validation bit-à-bit** et la **maîtrise des concepts du cours**."
