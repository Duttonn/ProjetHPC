#!/bin/bash
#SBATCH --job-name=motion_bench
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=00:10:00
#SBATCH --output=bench_final_%j.log
#SBATCH --partition=az4-n4090

# 1. Charger les modules
module purge
module load gcc/13.3.0 cmake opencv/4.10.0

# 2. Compilation propre
cd motion
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DMOTION_OPENCV_LINK=OFF -DMOTION_USE_MIPP=ON \
  -DCMAKE_C_FLAGS="-O3 -march=native -fopenmp" \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -fopenmp"
cmake --build build --parallel

# 3. Exécution des tests (1080p)
echo "--- BENCHMARK 1080p ---"
./bin/motion --vid-in-buff --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 \
             --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats

# 4. Test de scaling (pour les graphes)
echo "--- SCALING TEST ---"
for t in 1 2 4 8 16 32; do
    export OMP_NUM_THREADS=$t
    echo "Threads: $t"
    ./bin/motion --vid-in-buff --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 \
                 --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats | grep "FPS =" | tail -n 1
done