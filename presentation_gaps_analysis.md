# Analyse des Parties Manquantes - Présentation vs PDF Projet

## ✅ Sections Présentes et Complètes

### 1. Section 2.1 - Task Graph Simplification
- ✓ Slide 3 couvre la simplification (TP3)
- ✓ Explique le passage de motion2 à motion
- ✓ Montre le gain théorique 2×

### 2. Section 2.5.1 - "Standard" Optimizations
- ✓ SIMD (Slide 4)
- ✓ OpenMP (Slide 7)
- ✓ Loop fusion mentionné

### 3. Section 2.5.2 - Operators Fusion
- ✓ Slide 5 détaille la fusion d'opérateurs
- ✓ Lien avec CM2 (Cache L1/L2)
- ✓ Bénéfices expliqués

### 4. Section 2.5.3 - Morphologie Séparable
- ✓ Slide 6 explique 3×3 → 1×3 + 3×1
- ✓ Réduction 9 ops → 6 ops

### 5. Résultats Expérimentaux
- ✓ Tableaux FPS et latences (Slides 8-9)
- ✓ Graphes de scaling

---

## ⚠️ Sections Incomplètes

### 1. Section 2.5.3 - Pipeline of Row Operators (MANQUE DE DÉTAILS)

**Ce qui est dit dans le PDF (page 7):**
> "Image operators are split into 'row' operators. These 'row' operators are pipelined. This improves data persistence in caches close to the processor. This is known as **Cache Level Parallelism**."

**Ce qui manque dans notre présentation:**
- Explication du concept de "row operators"
- Différence entre:
  - Appliquer un opérateur sur toute l'image puis le suivant
  - Pipeline de "row operators" (traiter ligne par ligne)
- Prologue/Epilogue requis pour le pipeline

**Recommandation:** Ajouter une slide ou sous-section expliquant:
```
Au lieu de:
  1. Sigma-Delta(toute l'image) → IB
  2. Morpho Opening(toute l'image) → IB2
  3. Morpho Closing(toute l'image) → IB

Pipeline de row operators:
  Pour chaque bloc de 16 lignes:
    1. Sigma-Delta(lignes i à i+15)
    2. Opening(lignes i à i+15)
    3. Closing(lignes i à i+15)
  → Les données restent en cache entre chaque étape
```

### 2. Section 2.2 - Validation (MANQUE)

**Ce qui est dit dans le PDF (page 5):**
- Processus de validation complet avec `diff logs_ref logs_new`
- Importance cruciale: "computations must be correct!"

**Ce qui manque:**
- Slide ou section dédiée à la validation
- Commandes de validation
- Preuve que `diff` retourne vide

**Recommandation:** Ajouter une slide "Validation" après la slide 3 (Task Graph):
```markdown
## Validation Bit-à-Bit

### Processus
1. Génération des logs de référence (motion2):
   ./bin/motion2 --vid-in-stop 20 --log-path logs_ref

2. Génération des logs optimisés (motion):
   ./bin/motion --vid-in-stop 20 --log-path logs_new

3. Comparaison:
   diff -r logs_ref logs_new
   → Aucune différence (vide) ✓

### Garantie
- Résultats identiques à motion2
- Aucune régression fonctionnelle
- Optimisations purement techniques
```

### 3. Section 2.4 - Méthodologie de Mesure (INCOMPLET)

**Ce qui est dit dans le PDF (page 6):**
- `--vid-in-buff` pour buffer 100 frames (cache video decoding)
- `--stats` pour voir latences par étape
- Tests sur différentes résolutions

**Ce qui manque:**
- Détails de la méthodologie de benchmark
- Pourquoi 100 frames?
- Commandes exactes utilisées

**Recommandation:** Ajouter une slide "Méthodologie de Benchmark":
```markdown
## 4. Méthodologie de Benchmark

### Configuration des Tests
- **Vidéo:** traffic/1080p_day_street_top_view_snow.mp4
- **Frames:** 100 (--vid-in-stop 100)
- **Buffer:** --vid-in-buff (cache le décodage vidéo)
- **Stats:** --stats (latences par étape)

### Commande de Benchmark
./bin/motion --vid-in-buff --vid-in-stop 100 \
  --vid-in-path traffic/1080p.mp4 --stats

### Métriques Mesurées
- **FPS moyen** (throughput global)
- **Latences par étape** (Sigma-Delta, Morpho, CCL)
- **Speedup** vs baseline (motion2)
```

### 4. Section 3 - Architecture Matérielle (INCOMPLET)

**Ce qui est dit dans le PDF (page 8):**
> "Before the experimentation section, you will describe your testbed: the computer architecture (CPU, GPU & memory)."

**Ce qui manque:**
- Détails complets du CPU (cache L1/L2/L3, fréquence)
- Mémoire RAM (taille, bande passante)
- Justification du choix de ne pas utiliser GPU

**Recommandation:** Ajouter une slide dédiée après l'introduction:
```markdown
## 2. Architecture de Test - Cluster Dalek

### Processeur: AMD Ryzen 9 7945HX
- **Cœurs:** 16 cœurs physiques / 32 threads
- **Fréquence:** 2.5 GHz base, 5.4 GHz boost
- **ISA:** x86-64, AVX-512 (SIMD)
- **Cache L1:** 32 KB I + 32 KB D par cœur
- **Cache L2:** 1 MB par cœur
- **Cache L3:** 64 MB partagé

### Mémoire
- **RAM:** DDR5-4800 (bande passante ~38 GB/s)
- **TDP:** 55W

### Choix GPU
- GPU disponible mais non utilisé
- CPU suffit largement (213 FPS > 30 FPS requis)
- Évite latence CPU ↔ GPU
```

---

## ❌ Sections Volontairement Omises

Ces optimisations ont été retirées selon [antoine.md](antoine.md:7):

### 1. Section 2.5.4 - Bit-Packing
**Raison:** "Trop dur à expliquer à l'oral, risque de se faire niquer par le prof"

### 2. Section 2.5.6/2.5.8 - GPU
**Raison:** "GPU non nécessaire vu les perfs CPU" + complexité implémentation

### 3. Section 2.5.7 - Logical vs Binary Coding
**Raison:** Non applicable sans bit-packing

---

## 📋 Recommandations Prioritaires

### Priorité 1: Ajouter Validation (CRITIQUE)
Le PDF insiste: "computations must be correct!" et le prof va demander.
→ Ajouter slide "Validation Bit-à-Bit" après Task Graph

### Priorité 2: Détailler Architecture Matérielle
Section obligatoire du rapport (page 8 du PDF)
→ Ajouter slide dédiée "Architecture de Test"

### Priorité 3: Clarifier Pipeline de Row Operators
Différencier fusion d'opérateurs (slide 5) et pipeline par blocs (2.5.3)
→ Modifier slide 5 pour expliquer les deux concepts

### Priorité 4: Méthodologie de Benchmark
Expliquer pourquoi 100 frames, --vid-in-buff, etc.
→ Ajouter slide ou section dans slide 8

### Priorité 5 (Optionnel): Justifier Absence GPU
Anticiper la question "Pourquoi pas GPU?" (slide conclusion)
→ Déjà présent mais peut être renforcé

---

## 📊 Structure Finale Recommandée

1. **Titre**
2. **Plan**
3. **Architecture de Test** ← AJOUTER
4. **Contexte et Objectifs**
5. **Task Graph Simplification (TP3)**
6. **Validation Bit-à-Bit** ← AJOUTER
7. **Méthodologie de Benchmark** ← AJOUTER (ou intégrer dans slide 8)
8. **Vectorisation SIMD (CM3)**
9. **Fusion d'Opérateurs + Pipeline (CM2)** ← CLARIFIER
10. **Morphologie Séparable (CM3)**
11. **Parallélisation OpenMP (CM4)**
12. **Résultats Globaux**
13. **Discussion et Analyse**
14. **Conclusion**
15. **Questions**

---

## ✅ Points Forts de Notre Présentation

1. ✓ Structure logique suivant le projet
2. ✓ Liens explicites avec les chapitres du cours (CM2, CM3, CM4)
3. ✓ Résultats chiffrés avec gains clairs
4. ✓ Tableaux et graphes prêts
5. ✓ Explication des limites (scaling modeste)
6. ✓ Validation mentionnée (mais à développer)

---

## 🎯 Actions Concrètes

### Action 1: Créer slide "Validation"
Insérer après slide 3, avant optimisations CPU

### Action 2: Créer slide "Architecture de Test"
Insérer après "Plan", avant "Contexte"

### Action 3: Améliorer slide 5 (Fusion)
Distinguer:
- Fusion d'opérateurs (enchaîner SD+Morpho)
- Pipeline de row operators (traiter par blocs de lignes)

### Action 4: Ajouter méthodologie dans slide 8
Expliquer --vid-in-buff, 100 frames, --stats

### Action 5: Renforcer conclusion sur GPU
Justifier choix CPU-only avec performance/complexité
