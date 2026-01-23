#!/bin/bash
#SBATCH --job-name=motion_bench
#SBATCH --output=benchmark_final_%j.out
#SBATCH --error=benchmark_final_%j.err
#SBATCH --partition=dalek
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
#SBATCH --time=00:15:00

echo "=============================================="
echo "MOTION DETECTION BENCHMARK - COMPUTE NODE"
echo "=============================================="
echo "Node: $(hostname)"
echo "Date: $(date)"
echo "CPUs: $SLURM_CPUS_PER_TASK"
lscpu | grep "Model name"
echo ""

# Use absolute paths
BUILD_DIR="/mnt/nfs/users/duttonn/ProjetHPC/motion/build"
VIDEO="/mnt/nfs/users/duttonn/ProjetHPC/traffic/1080p_day_street_top_view_snow.mp4"

cd $BUILD_DIR

echo "=============================================="
echo "1. BASELINE (motion2) - 1080p, 100 frames"
echo "=============================================="
./bin/motion2 --vid-in-buff --vid-in-path $VIDEO \
    --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1

echo ""
echo "=============================================="
echo "2. OPTIMIZED (motion) - 1080p, 100 frames"
echo "=============================================="
./bin/motion --vid-in-buff --vid-in-path $VIDEO \
    --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1

echo ""
echo "=============================================="
echo "3. VALIDATION TEST (20 frames)"
echo "=============================================="
rm -rf /tmp/logs_ref_$$ /tmp/logs_new_$$
./bin/motion2 --vid-in-path $VIDEO \
    --vid-in-stop 20 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --log-path /tmp/logs_ref_$$ 2>&1 | tail -5
./bin/motion --vid-in-path $VIDEO \
    --vid-in-stop 20 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --log-path /tmp/logs_new_$$ 2>&1 | tail -5
echo "Comparing logs..."
if diff -rq /tmp/logs_ref_$$ /tmp/logs_new_$$ > /dev/null 2>&1; then
    echo "VALIDATION: PASSED - outputs are identical"
else
    echo "VALIDATION: FAILED - outputs differ"
    diff /tmp/logs_ref_$$/00001.txt /tmp/logs_new_$$/00001.txt 2>&1 | head -20
fi
rm -rf /tmp/logs_ref_$$ /tmp/logs_new_$$

echo ""
echo "=============================================="
echo "4. THREAD SCALING TEST (motion, 1080p)"
echo "=============================================="
for threads in 1 2 4 8 16; do
    echo ""
    echo "--- OMP_NUM_THREADS=$threads ---"
    OMP_NUM_THREADS=$threads ./bin/motion --vid-in-buff --vid-in-path $VIDEO \
        --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats 2>&1 | grep -E "(Took|Sigma-Delta|Morphology|CC Labeling|Total)"
done

echo ""
echo "=============================================="
echo "Benchmark completed at $(date)"
echo "=============================================="
