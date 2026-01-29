#!/bin/bash

# Script pour parser les résultats des benchmarks

cd ~/ProjetHPC

echo "====================================="
echo "RÉSUMÉ DES RÉSULTATS - MOTION PROJECT"
echo "====================================="
echo ""

# Tableau 1: FPS par configuration
echo "## TABLEAU 1: Performances (FPS)"
echo "| Configuration | Avg FPS | Total FPS |"
echo "|---------------|---------|-----------|"

# motion2 (référence)
if [ -f results_motion2.txt ]; then
    AVG_FPS=$(grep "avg.*FPS" results_motion2.txt | grep -oP '\d+(?= FPS)')
    TOTAL_FPS=$(grep "Total.*FPS" results_motion2.txt | grep -oP '\d+\.\d+(?= FPS)')
    echo "| motion2 (ref) | $AVG_FPS | $TOTAL_FPS |"
fi

# motion avec différents threads
for THREADS in 1 4 8 16 32; do
    FILE="results_motion_${THREADS}threads.txt"
    if [ -f "$FILE" ]; then
        AVG_FPS=$(grep "avg.*FPS" "$FILE" | grep -oP '\d+(?= FPS)')
        TOTAL_FPS=$(grep "Total.*FPS" "$FILE" | grep -oP '\d+\.\d+(?= FPS)')
        echo "| motion ($THREADS threads) | $AVG_FPS | $TOTAL_FPS |"
    fi
done

echo ""
echo "## TABLEAU 2: Latences moyennes (ms)"
echo "| Configuration | Sigma-Delta | Morphology | CCL | Total |"
echo "|---------------|-------------|------------|-----|-------|"

# motion2
if [ -f results_motion2.txt ]; then
    SD=$(grep "Sigma-Delta" results_motion2.txt | grep -oP '\d+\.\d+(?= ms)')
    MORPH=$(grep "Morphology" results_motion2.txt | grep -oP '\d+\.\d+(?= ms)')
    CCL=$(grep "CC Labeling" results_motion2.txt | grep -oP '\d+\.\d+(?= ms)')
    TOTAL=$(grep "=> Total" results_motion2.txt | grep -oP '\d+\.\d+(?= ms)')
    echo "| motion2 | $SD | $MORPH | $CCL | $TOTAL |"
fi

# motion avec différents threads
for THREADS in 1 4 8 16 32; do
    FILE="results_motion_${THREADS}threads.txt"
    if [ -f "$FILE" ]; then
        SD=$(grep "Sigma-Delta" "$FILE" | grep -oP '\d+\.\d+(?= ms)')
        MORPH=$(grep "Morphology" "$FILE" | grep -oP '\d+\.\d+(?= ms)')
        CCL=$(grep "CC Labeling" "$FILE" | grep -oP '\d+\.\d+(?= ms)')
        TOTAL=$(grep "=> Total" "$FILE" | grep -oP '\d+\.\d+(?= ms)')
        echo "| motion ($THREADS) | $SD | $MORPH | $CCL | $TOTAL |"
    fi
done

echo ""
echo "## DONNÉES POUR GRAPHE DE SCALING"
echo "Threads,AvgFPS,TotalFPS"
for THREADS in 1 4 8 16 32; do
    FILE="results_motion_${THREADS}threads.txt"
    if [ -f "$FILE" ]; then
        AVG_FPS=$(grep "avg.*FPS" "$FILE" | grep -oP '\d+(?= FPS)')
        TOTAL_FPS=$(grep "Total.*FPS" "$FILE" | grep -oP '\d+\.\d+(?= FPS)')
        echo "$THREADS,$AVG_FPS,$TOTAL_FPS"
    fi
done
