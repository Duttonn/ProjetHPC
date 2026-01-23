#!/bin/bash
#SBATCH --job-name=motion_bench
#SBATCH --partition=az4-n4090
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
#SBATCH --time=00:30:00
#SBATCH --output=benchmark_dalek_%j.out
#SBATCH --error=benchmark_dalek_%j.err

echo "=============================================="
echo "Motion HPC Benchmark on Dalek Compute Node"
echo "=============================================="
echo "Date: $(date)"
echo "Node: $(hostname)"
echo "CPU Info:"
lscpu | grep -E "Model name|CPU\(s\)|Thread|Core|Socket|MHz"
echo ""

cd /mnt/nfs/users/duttonn/ProjetHPC/motion/build

# Define paths
VIDEO_1080P="/mnt/nfs/users/duttonn/ProjetHPC/traffic/1080p_day_street_top_view_snow.mp4"
VIDEO_4K="/mnt/nfs/users/duttonn/ProjetHPC/traffic/2160p_day_highway_car_tolls.mp4"

# Set OpenMP threads
export OMP_NUM_THREADS=16

echo "=============================================="
echo "1. VALIDATION: Comparing motion vs motion2"
echo "=============================================="

# Clean up old logs
rm -rf logs_ref logs_new

# Run reference (motion2)
echo "Running motion2 (reference)..."
./bin/motion2 --vid-in-path "$VIDEO_1080P" \
              --vid-in-stop 20 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 \
              --log-path logs_ref

# Run optimized (motion)
echo "Running motion (optimized)..."
./bin/motion --vid-in-path "$VIDEO_1080P" \
             --vid-in-stop 20 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 \
             --log-path logs_new

# Compare outputs
echo ""
echo "Validation result:"
if diff -q logs_ref logs_new > /dev/null 2>&1; then
    echo "✓ PASSED: Outputs are identical"
else
    echo "✗ FAILED: Outputs differ!"
    diff logs_ref logs_new | head -20
fi
echo ""

echo "=============================================="
echo "2. BENCHMARK: motion2 (baseline) - 100 frames"
echo "=============================================="
echo "Running motion2 with --stats..."
./bin/motion2 --vid-in-buff --vid-in-path "$VIDEO_1080P" \
              --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats

echo ""
echo "=============================================="
echo "3. BENCHMARK: motion (optimized) - 100 frames"
echo "=============================================="
echo "Running motion with --stats..."
./bin/motion --vid-in-buff --vid-in-path "$VIDEO_1080P" \
             --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats

echo ""
echo "=============================================="
echo "4. SCALING TEST: Varying thread counts"
echo "=============================================="

for threads in 1 2 4 8 16; do
    export OMP_NUM_THREADS=$threads
    echo "--- OMP_NUM_THREADS=$threads ---"
    ./bin/motion --vid-in-buff --vid-in-path "$VIDEO_1080P" \
                 --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats
done

echo ""
echo "=============================================="
echo "5. 4K VIDEO BENCHMARK (if available)"
echo "=============================================="
export OMP_NUM_THREADS=16
if [ -f "$VIDEO_4K" ]; then
    echo "Running motion2 on 4K video..."
    ./bin/motion2 --vid-in-buff --vid-in-path "$VIDEO_4K" \
                  --vid-in-stop 50 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats
    echo ""
    echo "Running motion on 4K video..."
    ./bin/motion --vid-in-buff --vid-in-path "$VIDEO_4K" \
                 --vid-in-stop 50 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats
else
    echo "4K video not found at $VIDEO_4K, skipping."
fi

echo ""
echo "=============================================="
echo "Benchmark completed at $(date)"
echo "=============================================="
