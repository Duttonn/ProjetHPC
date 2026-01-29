# Présentation Complète - PRÊTE À UTILISER

## Fichiers Générés

### 1. Présentation Principale
📄 **[presentation.md](presentation.md)** - Diaporama complet (21 slides)
- Structure logique suivant le plan du projet
- Toutes les optimisations expliquées avec lien au cours
- Résultats avec les **meilleures données** (2.5.2)

### 2. Données pour Graphes
📊 **CSV files:**
- `graph_data_openmp_scaling.csv` - Scaling OpenMP (1, 2, 4, 8, 16 threads)
- `graph_data_latencies.csv` - Latences par étape (motion2 vs motion)
- `graph_data_fps_comparison.csv` - FPS global toutes configurations

### 3. Générateur de Graphes
🐍 **[generate_graphs.py](generate_graphs.py)** - Script Python automatique
- Génère 4 graphes PNG haute qualité (300 DPI)
- Prêts pour insertion PowerPoint

### 4. Documentation de Référence
📚 **[data_comparison.md](data_comparison.md)** - Comparaison des 3 datasets
- Explique les différences entre les mesures
- Justification du choix du dataset 2.5.2

---

## Résultats Finaux Utilisés

### Performance Globale
| Métrique | Valeur | Gain |
|----------|--------|------|
| **motion2 (baseline)** | 90 FPS | 1.0× |
| **motion optimisé (1 thread)** | 181 FPS | 2.0× |
| **motion optimisé (16 threads)** | 213 FPS | **2.4×** |

### Gains par Optimisation
| Optimisation | Cours | Gain |
|--------------|-------|------|
| SIMD (AVX-512) | CM3 | 2.1× (Sigma-Delta) |
| Fusion d'Opérateurs | CM2 | 1.6× (global) |
| Morphologie Séparable | CM3 | 1.5× (Morpho) |
| OpenMP 16 threads | CM4 | 1.18× (scaling) |

### Latences Totales
- **motion2:** 8.67 ms/frame
- **motion optimisé:** 5.52 ms/frame
- **Gain:** 1.57× plus rapide

---

## Comment Utiliser

### Option A: Convertir Markdown en PowerPoint
```bash
# Avec pandoc (si installé)
pandoc presentation.md -o presentation.pptx

# Ou copier-coller le contenu dans PowerPoint
# Chaque "---" marque une nouvelle slide
```

### Option B: Générer les Graphes PNG
```bash
# Installer les dépendances (une seule fois)
pip install matplotlib pandas

# Générer les 4 graphes
cd ~/ProjetHPC
python3 generate_graphs.py
```

**Sortie:**
- `graph1_openmp_scaling.png` - Scaling OpenMP (FPS et Speedup)
- `graph2_latencies_comparison.png` - Latences par étape
- `graph3_fps_comparison.png` - FPS global (bar chart)
- `graph4_stacked_latencies.png` - Breakdown temps par frame

### Option C: Import CSV dans Excel/Google Sheets
1. Ouvrir les fichiers `.csv` dans Excel
2. Créer les graphes manuellement avec l'assistant graphique
3. Personnaliser les couleurs/styles selon vos préférences

---

## Structure de la Présentation

### Slides 1-2: Introduction (2 slides)
- Contexte: Détection de mouvement
- Objectifs du projet

### Slides 3: Task Graph (TP3)
- Simplification algorithmique
- Gain 2× théorique

### Slides 4-6: Optimisations CPU (3 slides)
- **Slide 4:** SIMD (CM3) - 2.1× sur Sigma-Delta
- **Slide 5:** Fusion d'Opérateurs (CM2) - Cache L1/L2
- **Slide 6:** Morphologie Séparable (CM3) - 9→6 ops

### Slide 7: OpenMP (CM4)
- Scaling positif modéré (1.18×)
- Loi d'Amdahl expliquée

### Slides 8-9: Résultats (2 slides)
- Tableaux FPS et latences
- Graphes de scaling

### Slide 10: Discussion
- Analyse des gains
- Limites du scaling OpenMP

### Slide 11: Conclusion
- Synthèse des objectifs atteints
- Perspectives

---

## Points Clés pour l'Oral

### 1. Validation Bit-à-Bit (IMPORTANT)
```bash
diff -r logs_ref logs_new  # → Aucune différence
```
**Message:** "Nos optimisations ne changent pas les résultats, c'est garanti."

### 2. Lien avec le Cours (CRUCIAL pour la note)
| Slide | Concept | Référence Cours |
|-------|---------|-----------------|
| 4 | SIMD | **CM3** (Vectorisation) |
| 5 | Cache L1/L2 | **CM2** (Hiérarchie Mémoire) |
| 6 | Morpho Séparable | CM3 (Réduction Ops) |
| 7 | OpenMP | **CM4** (Parallélisme) |

**Phrase magique:** "Cette optimisation applique directement le CM2 sur la localité de cache."

### 3. Gain Final (À retenir)
- **2.4× plus rapide** que motion2 (90 → 213 FPS)
- **1.57× par frame** (8.67 → 5.52 ms)
- **Validation réussie** (0 différence)

### 4. Explication du Scaling Modeste (1.18×)
**Question du prof probable:** "Pourquoi seulement 1.18× avec 16 cœurs?"

**Réponse préparée:**
1. **Loi d'Amdahl:** Partie séquentielle (I/O vidéo) limite le speedup
2. **Memory-bound:** Le décodage vidéo et CCL saturent la bande passante RAM
3. **Overhead OpenMP:** Synchronisation threads + création/destruction coûteuse
4. **Mais positif:** Le scaling reste croissant (pas de dégradation)

---

## Checklist Avant Présentation

- [ ] Lire [presentation.md](presentation.md) en entier
- [ ] Générer les 4 graphes PNG (`python3 generate_graphs.py`)
- [ ] Insérer les graphes dans PowerPoint
- [ ] Vérifier les numéros de slides dans antoine.md correspondent
- [ ] Préparer la réponse à "Pourquoi pas GPU?" (voir conclusion slide 11)
- [ ] Tester la validation bit-à-bit devant le prof si demandé
- [ ] Relire les sections CM2, CM3, CM4 du cours pour les questions

---

## Commandes Utiles pour Démo Live (si demandé)

### Compilation
```bash
module load gcc/13.3.0 cmake opencv/4.10.0
cd motion
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMOTION_USE_MIPP=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -fopenmp" \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -fopenmp"
cmake --build build --parallel
```

### Validation
```bash
./bin/motion2 --vid-in-stop 20 --vid-in-path ../traffic/1080p.mp4 --log-path logs_ref
./bin/motion --vid-in-stop 20 --vid-in-path ../traffic/1080p.mp4 --log-path logs_new
diff -r logs_ref logs_new  # Doit être vide
```

### Benchmark
```bash
export OMP_NUM_THREADS=16
./bin/motion --vid-in-buff --vid-in-stop 100 \
  --vid-in-path ../traffic/1080p.mp4 --stats
```

---

## Conseils Finaux

1. **Insister sur CM2/CM3/CM4** → C'est ça qui rapporte des points
2. **Validation = sécurité** → Pas de régression, résultats corrects
3. **Scaling modeste = normal** → Loi d'Amdahl bien connue en HPC
4. **GPU pas nécessaire** → CPU suffit largement (213 FPS > 30 FPS requis)

**Objectif: 17-18/20** → Tous les concepts du cours appliqués ✓

Bonne chance pour la présentation ! 🚀
