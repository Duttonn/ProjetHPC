#!/bin/bash
#SBATCH --job-name=benchmark_bitpack
#SBATCH --output=benchmark_bitpack_%j.out
#SBATCH --error=benchmark_bitpack_%j.err
#SBATCH --partition=az4-n4090
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
#SBATCH --time=00:10:00

echo "=============================================="
echo "Motion Detection Benchmark with Bit-Packing"
echo "=============================================="
echo "Date: $(date)"
echo "Node: $(hostname)"
echo "=============================================="

cd /mnt/nfs/users/duttonn/ProjetHPC/motion/build

VIDEO="/mnt/nfs/users/duttonn/ProjetHPC/traffic/1080p_day_street_top_view_snow.mp4"
VIDEO_4K="/mnt/nfs/users/duttonn/ProjetHPC/traffic/2160p_day_highway_car_tolls.mp4"

echo ""
echo "=== CPU Info ==="
lscpu | grep -E "^(Model name|CPU\(s\)|Thread|Core|CPU max MHz)"

echo ""
echo "=============================================="
echo "1. BASELINE (motion2) - 1080p"
echo "=============================================="
./bin/motion2 --vid-in-buff --vid-in-path "$VIDEO" \
    --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1 | \
    grep -E "(Sigma-Delta|Morphology|Total|FPS)"

echo ""
echo "=============================================="
echo "2. OPTIMIZED (motion) - 1080p"
echo "=============================================="
./bin/motion --vid-in-buff --vid-in-path "$VIDEO" \
    --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1 | \
    grep -E "(Sigma-Delta|Morphology|Total|FPS)"

echo ""
echo "=============================================="
echo "3. VALIDATION"
echo "=============================================="
./bin/motion2 --vid-in-path "$VIDEO" --vid-in-stop 20 \
    --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --log-path /tmp/logs_ref 2>/dev/null
./bin/motion --vid-in-path "$VIDEO" --vid-in-stop 20 \
    --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --log-path /tmp/logs_new 2>/dev/null

if diff -q /tmp/logs_ref /tmp/logs_new > /dev/null 2>&1; then
    echo "VALIDATION: PASSED - outputs are identical"
else
    echo "VALIDATION: FAILED - outputs differ!"
    diff /tmp/logs_ref /tmp/logs_new | head -20
fi

echo ""
echo "=============================================="
echo "4. THREAD SCALING TEST"
echo "=============================================="
for t in 1 2 4 8 16; do
    echo "--- OMP_NUM_THREADS=$t ---"
    OMP_NUM_THREADS=$t ./bin/motion --vid-in-buff --vid-in-path "$VIDEO" \
        --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1 | \
        grep -E "(Sigma-Delta|Morphology|Total|FPS)"
done

echo ""
echo "=============================================="
echo "5. 4K VIDEO TEST"
echo "=============================================="
if [ -f "$VIDEO_4K" ]; then
    echo "--- motion2 (baseline) ---"
    ./bin/motion2 --vid-in-buff --vid-in-path "$VIDEO_4K" \
        --vid-in-stop 50 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1 | \
        grep -E "(Sigma-Delta|Morphology|Total|FPS)"
    
    echo "--- motion (optimized) ---"
    ./bin/motion --vid-in-buff --vid-in-path "$VIDEO_4K" \
        --vid-in-stop 50 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1 | \
        grep -E "(Sigma-Delta|Morphology|Total|FPS)"
else
    echo "4K video not found, skipping"
fi

echo ""
echo "=============================================="
echo "Benchmark completed at $(date)"
echo "=============================================="
