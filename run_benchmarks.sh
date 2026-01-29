#!/bin/bash

# Script de benchmark complet pour le projet Motion
# Génère toutes les données nécessaires pour les graphes

echo "==================================="
echo "BENCHMARK MOTION - DALEK CLUSTER"
echo "==================================="
echo ""

VIDEO="traffic/1080p_day_street_top_view_snow.mp4"
FRAMES=100

# Aller dans le répertoire du projet
cd ~/ProjetHPC

echo "Configuration:"
echo "  - Vidéo: $VIDEO"
echo "  - Frames: $FRAMES"
echo "  - Nœud: $(hostname)"
echo "  - CPU: $(lscpu | grep 'Model name' | cut -d: -f2 | xargs)"
echo ""

# 1. Baseline: motion2 (référence)
echo "=========================================="
echo "TEST 1/7: motion2 (référence)"
echo "=========================================="
./motion/build/bin/motion2 --vid-in-buff --vid-in-stop $FRAMES --vid-in-path $VIDEO --stats 2>&1 | tee results_motion2.txt
echo ""

# 2. motion optimisé avec différents nombres de threads
for THREADS in 1 4 8 16 32; do
    echo "=========================================="
    echo "TEST: motion avec OMP_NUM_THREADS=$THREADS"
    echo "=========================================="
    export OMP_NUM_THREADS=$THREADS
    ./motion/build/bin/motion --vid-in-buff --vid-in-stop $FRAMES --vid-in-path $VIDEO --stats 2>&1 | tee results_motion_${THREADS}threads.txt
    echo ""
done

# Résumé
echo "=========================================="
echo "BENCHMARK TERMINÉ !"
echo "=========================================="
echo ""
echo "Fichiers de résultats générés:"
ls -lh results_*.txt
echo ""
echo "Pour extraire les résultats:"
echo "  grep 'avg.*FPS' results_*.txt"
echo "  grep 'Sigma-Delta' results_*.txt"
echo "  grep 'Morphology' results_*.txt"
