# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Zero-Question Execution Policy

Claude MUST NOT ask the user for confirmation, clarification, or input
before generating commands, code, or workflows.

Default behavior:
- Make reasonable assumptions
- Choose safe, standard defaults
- Proceed with execution-ready instructions

Claude may ask a question ONLY if:
- executing would risk irreversible data loss, or
- multiple mutually exclusive options exist AND the choice cannot be inferred.

If assumptions are made, Claude MUST:
- state them explicitly
- continue without waiting for user input.


---


## Project Overview

Motion HPC is a streaming application for detecting and tracking moving objects from video sequences. The goal is to optimize the Motion detection pipeline using HPC techniques (SIMD, OpenMP, GPU) while maintaining bitwise-identical output to the golden model (`motion2`).

**Key Files:**
- `motion/src/main/motion2.c` - Golden model (reference implementation)
- `motion/src/main/motion.c` - Optimized implementation (to be created)
- `motion/src/common/sigma_delta/sigma_delta_compute.c` - Sigma-Delta algorithm
- `motion/src/common/morpho/morpho_compute.c` - Morphological operations

---

## Build Commands

### Local Build (macOS/Linux)
```bash
cd motion
mkdir -p build && cd build
cmake .. -DMOTION_OPENCV_LINK=ON -DMOTION_USE_MIPP=ON \
         -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O3 -funroll-loops -fstrict-aliasing -march=native" \
         -DCMAKE_C_FLAGS="-O3 -funroll-loops -fstrict-aliasing -march=native"
make -j$(nproc)
```

### Dalek Cluster Build (FRONTEND ONLY)
```bash
module purge && module load gcc/11.2 cmake ninja opencv
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMOTION_OPENCV_LINK=ON -DENABLE_OMP=ON -DENABLE_VECTO=ON
cmake --build build --parallel
```

### Debug Build (Without OpenCV - for valgrind/ASAN)
```bash
cmake -B build -DMOTION_OPENCV_LINK=OFF
cmake --build build
```

---

## Run Commands

### Performance Benchmark (FPS)
```bash
./bin/motion2 --vid-in-buff --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 \
              --vid-in-stop 100 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --stats
```

### Validation (Compare motion vs motion2)
```bash
./bin/motion2 --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 \
              --vid-in-stop 20 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --log-path logs_ref
./bin/motion  --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 \
              --vid-in-stop 20 --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --log-path logs_new
diff logs_ref logs_new
# Empty diff = CORRECT, Non-empty diff = WRONG
```

### Visual Output
```bash
./bin/motion2 --vid-in-path ../traffic/1080p_day_street_top_view_snow.mp4 \
              --flt-s-min 2000 --knn-d 50 --trk-obj-min 5 --vid-out-play --vid-out-id
```

---

## Code Architecture

### Directory Structure
```
motion/
├── src/
│   ├── main/
│   │   └── motion2.c          # Main executable (610 lines)
│   └── common/
│       ├── sigma_delta/       # Motion detection algorithm
│       ├── morpho/            # Mathematical morphology
│       ├── CCL/               # Connected Component Labeling
│       ├── features/          # RoI feature extraction
│       ├── kNN/               # Object matching
│       ├── tracking/          # Temporal tracking
│       ├── video/             # Video I/O
│       └── visu/              # Visualization
├── include/c/motion/          # Header files
└── lib/                       # Dependencies (MIPP, ffmpeg-io, nrc2, c-vector)
```

### Processing Pipeline
```
Input Frame (t)
      │
      ▼
┌─────────────────────────────────────────────────────────────────┐
│ 1. Sigma-Delta (ΣΔ) - Per-pixel motion detection                │
│    ├─ Step 1: Mean update Mt (increment/decrement toward It)    │
│    ├─ Step 2: Difference Ot = |Mt - It|                         │
│    ├─ Step 3: Variance Vt update + clamp [Vmin=1, Vmax=254]     │
│    └─ Step 4: Label Lt = (Ot < Vt) ? 0 : 255                    │
└─────────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. Mathematical Morphology - Noise removal                       │
│    ├─ Opening (3×3): erosion then dilation - removes small noise│
│    └─ Closing (3×3): dilation then erosion - fills small holes  │
└─────────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. CCL - Connected Component Labeling (labels connected regions)│
│ 4. CCA - Extract RoIs (bounding box, surface, centroid)         │
│ 5. Surface Filtering (Smin < S < Smax)                          │
└─────────────────────────────────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. k-NN Matching - Associate RoIs between frames t-1 and t      │
│ 7. Temporal Tracking - Maintain object IDs across frames        │
└─────────────────────────────────────────────────────────────────┘
```

### Data Structures
```c
// Sigma-Delta state
typedef struct {
    uint8_t **M;    // Mean/background image
    uint8_t **O;    // Difference image
    uint8_t **V;    // Variance image
} sigma_delta_data_t;

// Morphology buffer
typedef struct {
    uint8_t **IB;   // Temporary binary image
} morpho_data_t;

// Region of Interest
typedef struct {
    uint32_t id, xmin, xmax, ymin, ymax, S;
    float x, y;     // Centroid
} RoI_t;
```

---

## Optimization Pipeline (TP Requirements)

### Phase 1: Task Graph Simplification (TP3)
Create `motion.c` implementing simplified graph:
- Remove duplicate t-1/t computations
- Add `produce` (output CCs at t) and `memorize` (store for t+1)

### Phase 2: Standard Optimizations
1. **Loop unrolling/fusion** - Combine loops where possible
2. **SIMD (MIPP)** - Vectorize Sigma-Delta and morphology
3. **OpenMP** - Row-based parallelization
4. **GPU (OpenCL/CUDA)** - Offload compute-heavy tasks

### Phase 3: Advanced Optimizations
1. **Separable morphology**: (3×3) → (3×1) + (1×3)
2. **Bit-packing**: Store 8 binary pixels per byte
3. **Pipeline row operators**: Improve cache locality
4. **Operator fusion**: Merge ΣΔ + morphology in single pass

---

## Critical Constraints

### Correctness Rules
- `motion2` is the **golden model** - all outputs must be bitwise-identical
- **NEVER** change: thresholds, ΣΔ logic, variance clamping, morphology semantics
- MPI is **FORBIDDEN**
- Performance metric = **FPS only** (exclude I/O, decoding, visualization)

### Dalek Cluster Rules
- **FRONTEND** (`front.dalek.lip6.fr`): Compile ONLY
- **COMPUTE NODES** (`az4-*`): Execute ONLY
- Request node: `salloc -p az4-n4090 -N 1 -n 1 --time=02:00:00 bash`

### SIMD Rules (MIPP)
- Prefer MIPP over raw intrinsics
- No silent scalar fallbacks
- Parallelize over rows, avoid false sharing

### Validation (MANDATORY before any commit)
```bash
diff logs_ref logs_new  # Must be empty
```

---

## Project Deliverables

1. **`motion.c`** - Optimized implementation with simplified task graph
2. **`report.pdf`** - Document optimizations, benchmarks, speedup curves
3. **`code.zip`** - Source code without build/ and video files

### Report Structure
1. Introduction/Presentation
2. **Description of Optimizations** (with figures)
3. Computer Architecture (testbed description)
4. **Experimentation Results** (FPS, speedup curves, comparisons)
5. Conclusion

---

## Codebase Search with mgrep

Use `mgrep` for semantic search when looking up code patterns, functions, or understanding the codebase.

### Search Commands
```bash
# Semantic search for concepts
mgrep "sigma delta motion detection"

# Search with content display
mgrep -c "morphology erosion implementation"

# Get AI-generated answer
mgrep -a "how does connected component labeling work?"

# Limit results
mgrep -m 5 "RoI feature extraction"

# Search in specific path
mgrep "tracking algorithm" motion/src/common/tracking/
```

### When to Use mgrep
- Finding function implementations across multiple files
- Understanding how algorithms are connected
- Locating data structure definitions
- Exploring unfamiliar parts of the codebase
- Finding usage patterns of specific APIs
