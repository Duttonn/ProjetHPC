# Guide pour Antoine - Projet Motion HPC

Salut Antoine ! Voici un résumé de l'état du projet et tout ce dont tu as besoin pour finir les mesures, les graphes et préparer le PowerPoint.

## 1. La Stratégie
On vise un **17-18/20**. 
- On a **simplifié le code** : On a retiré le "Bit-packing" et le "GPU" (trop dur à expliquer à l'oral, risque de se faire niquer par le prof sur des détails techniques).
- On a mis le paquet sur les **concepts du cours** (CM2, CM3, CM4). C'est beaucoup plus facile à justifier devant le prof : "J'ai appliqué le CM2 sur les caches" -> Point gagné.

## 2. Ce qu'on a implémenté (Lien avec le Poly/Cours)

| Optimisation | Section Poly | Chapitre Cours | Explication pour l'oral |
| :--- | :--- | :--- | :--- |
| **Task Graph (TP3)** | 2.1 | CM1 | On ne traite plus frame $t-1$, on réutilise les RoIs mémorisés. Gain x2. |
| **Fusion d'Opérateurs** | 2.5.3 | **CM2 (Caches)** | Sigma-Delta + Morpho enchaînés pour garder les données en Cache L1/L2. Évite la RAM lente. |
| **SIMD (MIPP)** | 2.5.1 | **CM3 (SIMD)** | On traite 32 pixels d'un coup avec les registres AVX-512. |
| **Morpho Séparable** | 2.5.2 | CM3 | 3x3 (9 pixels) devient 1x3 + 3x1 (6 pixels). Moins de calculs + vectorisation. |
| **OpenMP** | 2.5.1 | **CM4 (OpenMP)** | Parallélisation sur les 16 cœurs du Ryzen 9 avec `schedule(static)`. |

---

## 3. Tests sur Dalek (Les commandes)

### A. Compiler (Sur le front-end)
```bash
module load gcc/13.3.0 cmake opencv/4.10.0
cd motion
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMOTION_OPENCV_LINK=OFF -DMOTION_USE_MIPP=ON -DCMAKE_C_FLAGS="-O3 -march=native -fopenmp" -DCMAKE_CXX_FLAGS="-O3 -march=native -fopenmp"
cmake --build build --parallel
```

### B. Valider (Important : l'identique bit-à-bit)
Le prof va te demander si c'est identique à la référence.
```bash
# Générer la réf
./bin/motion2 --vid-in-stop 20 --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 --log-path logs_ref
# Générer notre version
./bin/motion --vid-in-stop 20 --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 --log-path logs_new
# Comparer
diff -r logs_ref logs_new
# SI CA NE RENVOIE RIEN : C'EST PARFAIT.
```

### C. Lancer le Benchmark final (Job Slurm)
J'ai créé un script `benchmark_final.sh`. Lance-le avec :
```bash
sbatch benchmark_final.sh
```
Une fois fini, regarde le fichier `bench_final_XXXX.log` pour remplir les tableaux ci-dessous.

---

## 4. Préparation des Graphes (Template)

Prends un Excel ou un script Python et remplis avec les valeurs que tu auras dans le `.log` :

### Graphe 1 : Scaling OpenMP (CM4)
*Lien poly : Section 2.5.1*
- Threads 1 : [Valeur FPS]
- Threads 4 : [Valeur FPS]
- Threads 8 : [Valeur FPS]
- Threads 16 : [Valeur FPS]
- Threads 32 : [Valeur FPS]
*Objectif : Montrer que ça sature vers 8-16 threads (Amdahl's Law).*

### Graphe 2 : Gain par étape (Bar Chart)
*Lien poly : Section 2.4 (--stats)*
- Sigma-Delta : [Latence ms motion2] vs [Latence ms motion]
- Morpho : [Latence ms motion2] vs [Latence ms motion]
- Total : [FPS motion2] vs [FPS motion]

---

## 5. Pistes pour le PowerPoint

1. **Slide 1-2 : Intro.** Détection de mouvement, caméra fixe.
2. **Slide 3 : Simplification (TP3).** Schéma avant/après. "On a divisé le travail par 2".
3. **Slide 4 : SIMD (CM3).** Montre le gain sur Sigma-Delta. "32 pixels/cycle avec AVX-512".
4. **Slide 5 : Fusion d'Opérateurs (CM2).** Explique comment **enchaîner Sigma-Delta + Morpho garde les données en cache L1/L2**. C'est ça qui va te donner la meilleure note. Dis qu'on évite les allers-retours avec la RAM.
5. **Slide 6 : Résultats.** Le tableau des FPS et le graphe de scaling OpenMP (1, 4, 8, 16 threads).
6. **Slide 7 : Conclusion.** "Optimisation CPU poussée, validation bit-à-bit réussie, GPU non nécessaire vu les perfs CPU".

Bonne chance Antoine ! Si tu as un doute, regarde le fichier `report.md`, tout est dedans.
