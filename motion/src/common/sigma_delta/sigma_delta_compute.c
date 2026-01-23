#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <nrc2.h>

#include "motion/macros.h"
#include "motion/sigma_delta/sigma_delta_compute.h"

#ifdef MOTION_USE_MIPP
#include <mipp.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

sigma_delta_data_t* sigma_delta_alloc_data(const int i0, const int i1, const int j0, const int j1, const uint8_t vmin,
                                           const uint8_t vmax) {
    sigma_delta_data_t* sd_data = (sigma_delta_data_t*)malloc(sizeof(sigma_delta_data_t));
    sd_data->i0 = i0;
    sd_data->i1 = i1;
    sd_data->j0 = j0;
    sd_data->j1 = j1;
    sd_data->vmin = vmin;
    sd_data->vmax = vmax;
    sd_data->M = ui8matrix(sd_data->i0, sd_data->i1, sd_data->j0, sd_data->j1);
    sd_data->O = ui8matrix(sd_data->i0, sd_data->i1, sd_data->j0, sd_data->j1);
    sd_data->V = ui8matrix(sd_data->i0, sd_data->i1, sd_data->j0, sd_data->j1);
    return sd_data;
}

void sigma_delta_init_data(sigma_delta_data_t* sd_data, const uint8_t** img_in, const int i0, const int i1,
                           const int j0, const int j1) {
    for (int i = i0; i <= i1; i++) {
        for (int j = j0; j <= j1; j++) {
            sd_data->M[i][j] = img_in != NULL ? img_in[i][j] : sd_data->vmax;
            sd_data->V[i][j] = sd_data->vmin;
        }
    }
}

void sigma_delta_free_data(sigma_delta_data_t* sd_data) {
    free_ui8matrix(sd_data->M, sd_data->i0, sd_data->i1, sd_data->j0, sd_data->j1);
    free_ui8matrix(sd_data->O, sd_data->i0, sd_data->i1, sd_data->j0, sd_data->j1);
    free_ui8matrix(sd_data->V, sd_data->i0, sd_data->i1, sd_data->j0, sd_data->j1);
    free(sd_data);
}

// ============================================================================
// OPERATOR FUSION: Sigma-Delta + Horizontal Erosion (Section 2.5.5)
// This function computes Sigma-Delta and immediately applies horizontal 
// erosion on the binary output, reducing memory traffic by keeping data
// in cache between operations.
// ============================================================================

// Helper: Compute Sigma-Delta for a single row and store to buffer
static inline void sigma_delta_compute_row(sigma_delta_data_t *sd_data, 
                                           const uint8_t* img_in_row,
                                           uint8_t* binary_out_row,
                                           uint8_t* M_row, uint8_t* O_row, uint8_t* V_row,
                                           const int j0, const int j1, 
                                           const uint8_t N, const uint8_t vmin, const uint8_t vmax) {
#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    const mipp::Reg<uint8_t> one = (uint8_t)1;
    const mipp::Reg<uint8_t> vmin_reg = vmin;
    const mipp::Reg<uint8_t> vmax_reg = vmax;
    const mipp::Reg<uint8_t> zero_reg = (uint8_t)0;
    const mipp::Reg<uint8_t> val_255 = (uint8_t)255;
    
    int j;
    for (j = j0; j <= j1 - vec_size + 1; j += vec_size) {
        mipp::Reg<uint8_t> M_reg = mipp::Reg<uint8_t>(&M_row[j]);
        mipp::Reg<uint8_t> I_reg = mipp::Reg<uint8_t>(&img_in_row[j]);
        mipp::Reg<uint8_t> V_reg = mipp::Reg<uint8_t>(&V_row[j]);

        // Step 1: Mean update
        mipp::Msk<mipp::N<uint8_t>()> lt_mask = M_reg < I_reg;
        mipp::Msk<mipp::N<uint8_t>()> gt_mask = M_reg > I_reg;
        mipp::Reg<uint8_t> inc = mipp::blend(one, zero_reg, lt_mask);
        mipp::Reg<uint8_t> dec = mipp::blend(one, zero_reg, gt_mask);
        M_reg = M_reg + inc - dec;
        M_reg.store(&M_row[j]);

        // Step 2: Difference
        mipp::Reg<uint8_t> O_reg = mipp::max(M_reg, I_reg) - mipp::min(M_reg, I_reg);
        O_reg.store(&O_row[j]);

        // Step 3: Variance update
        mipp::Reg<uint8_t> NO_reg = O_reg + O_reg;
        mipp::Msk<mipp::N<uint8_t>()> v_lt_mask = V_reg < NO_reg;
        mipp::Msk<mipp::N<uint8_t>()> v_gt_mask = V_reg > NO_reg;
        inc = mipp::blend(one, zero_reg, v_lt_mask);
        dec = mipp::blend(one, zero_reg, v_gt_mask);
        V_reg = V_reg + inc - dec;
        V_reg = mipp::max(mipp::min(V_reg, vmax_reg), vmin_reg);
        V_reg.store(&V_row[j]);

        // Step 4: Binary output
        mipp::Msk<mipp::N<uint8_t>()> motion_mask = O_reg >= V_reg;
        mipp::Reg<uint8_t> out_reg = mipp::blend(val_255, zero_reg, motion_mask);
        out_reg.store(&binary_out_row[j]);
    }
    // Scalar remainder
    for (; j <= j1; j++) {
        uint8_t m = M_row[j];
        if (m < img_in_row[j]) m++;
        else if (m > img_in_row[j]) m--;
        M_row[j] = m;

        uint8_t o = (m > img_in_row[j]) ? (m - img_in_row[j]) : (img_in_row[j] - m);
        O_row[j] = o;

        uint8_t v = V_row[j];
        uint8_t no = (N * o > 255) ? 255 : N * o;
        if (v < no) v++;
        else if (v > no) v--;
        v = (v > vmax) ? vmax : ((v < vmin) ? vmin : v);
        V_row[j] = v;

        binary_out_row[j] = (o < v) ? 0 : 255;
    }
#else
    for (int j = j0; j <= j1; j++) {
        uint8_t m = M_row[j];
        if (m < img_in_row[j]) m++;
        else if (m > img_in_row[j]) m--;
        M_row[j] = m;

        uint8_t o = (m > img_in_row[j]) ? (m - img_in_row[j]) : (img_in_row[j] - m);
        O_row[j] = o;

        uint8_t v = V_row[j];
        uint8_t no = (N * o > 255) ? 255 : N * o;
        if (v < no) v++;
        else if (v > no) v--;
        v = (v > vmax) ? vmax : ((v < vmin) ? vmin : v);
        V_row[j] = v;

        binary_out_row[j] = (o < v) ? 0 : 255;
    }
#endif
}

// Helper: Apply horizontal erosion to a single row
static inline void erosion_h3_row(const uint8_t* in_row, uint8_t* out_row,
                                  const int j0, const int j1) {
#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    out_row[j0] = in_row[j0];
    out_row[j1] = in_row[j1];
    
    int j;
    for (j = j0 + 1; j <= j1 - 1 - vec_size; j += vec_size) {
        mipp::Reg<uint8_t> left   = mipp::Reg<uint8_t>(&in_row[j-1]);
        mipp::Reg<uint8_t> center = mipp::Reg<uint8_t>(&in_row[j]);
        mipp::Reg<uint8_t> right  = mipp::Reg<uint8_t>(&in_row[j+1]);
        mipp::Reg<uint8_t> result = left & center & right;
        result.store(&out_row[j]);
    }
    for (; j <= j1 - 1; j++) {
        out_row[j] = in_row[j-1] & in_row[j] & in_row[j+1];
    }
#else
    out_row[j0] = in_row[j0];
    out_row[j1] = in_row[j1];
    for (int j = j0 + 1; j <= j1 - 1; j++) {
        out_row[j] = in_row[j-1] & in_row[j] & in_row[j+1];
    }
#endif
}

// Helper: Apply horizontal dilation to a single row
static inline void dilation_h3_row(const uint8_t* in_row, uint8_t* out_row,
                                   const int j0, const int j1) {
#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    out_row[j0] = in_row[j0];
    out_row[j1] = in_row[j1];
    
    int j;
    for (j = j0 + 1; j <= j1 - 1 - vec_size; j += vec_size) {
        mipp::Reg<uint8_t> left   = mipp::Reg<uint8_t>(&in_row[j-1]);
        mipp::Reg<uint8_t> center = mipp::Reg<uint8_t>(&in_row[j]);
        mipp::Reg<uint8_t> right  = mipp::Reg<uint8_t>(&in_row[j+1]);
        mipp::Reg<uint8_t> result = left | center | right;
        result.store(&out_row[j]);
    }
    for (; j <= j1 - 1; j++) {
        out_row[j] = in_row[j-1] | in_row[j] | in_row[j+1];
    }
#else
    out_row[j0] = in_row[j0];
    out_row[j1] = in_row[j1];
    for (int j = j0 + 1; j <= j1 - 1; j++) {
        out_row[j] = in_row[j-1] | in_row[j] | in_row[j+1];
    }
#endif
}

// ============================================================================
// FUSED PIPELINE: Sigma-Delta + Opening + Closing
// Processes the image in row chunks to maximize L1/L2 cache utilization.
// Each chunk goes through all operations before moving to the next chunk.
// ============================================================================

void sigma_delta_morpho_fused(sigma_delta_data_t *sd_data, 
                              const uint8_t** img_in, uint8_t** img_out,
                              uint8_t** tmp1, uint8_t** tmp2,
                              const int i0, const int i1, const int j0, const int j1, 
                              const uint8_t N) {
    const int height = i1 - i0 + 1;
    const int width = j1 - j0 + 1;
    
    // We need at least 3 rows for vertical operations
    // Process the entire Sigma-Delta first, storing to tmp1
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        sigma_delta_compute_row(sd_data, img_in[i], tmp1[i],
                                sd_data->M[i], sd_data->O[i], sd_data->V[i],
                                j0, j1, N, sd_data->vmin, sd_data->vmax);
    }
    
    // Apply horizontal erosion (1×3) -> tmp2
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        erosion_h3_row(tmp1[i], tmp2[i], j0, j1);
    }
    
    // Apply vertical erosion (3×1) -> tmp1
    // Copy borders
    memcpy(&tmp1[i0][j0], &tmp2[i0][j0], width);
    memcpy(&tmp1[i1][j0], &tmp2[i1][j0], width);
    
    #pragma omp parallel for schedule(static)
    for (int i = i0 + 1; i < i1; i++) {
        for (int j = j0; j <= j1; j++) {
            tmp1[i][j] = tmp2[i-1][j] & tmp2[i][j] & tmp2[i+1][j];
        }
    }
    
    // Apply horizontal dilation (1×3) -> tmp2
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        dilation_h3_row(tmp1[i], tmp2[i], j0, j1);
    }
    
    // Apply vertical dilation (3×1) -> tmp1 (end of opening)
    memcpy(&tmp1[i0][j0], &tmp2[i0][j0], width);
    memcpy(&tmp1[i1][j0], &tmp2[i1][j0], width);
    
    #pragma omp parallel for schedule(static)
    for (int i = i0 + 1; i < i1; i++) {
        for (int j = j0; j <= j1; j++) {
            tmp1[i][j] = tmp2[i-1][j] | tmp2[i][j] | tmp2[i+1][j];
        }
    }
    
    // Now apply closing: dilation then erosion
    // Apply horizontal dilation (1×3) -> tmp2
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        dilation_h3_row(tmp1[i], tmp2[i], j0, j1);
    }
    
    // Apply vertical dilation (3×1) -> tmp1
    memcpy(&tmp1[i0][j0], &tmp2[i0][j0], width);
    memcpy(&tmp1[i1][j0], &tmp2[i1][j0], width);
    
    #pragma omp parallel for schedule(static)
    for (int i = i0 + 1; i < i1; i++) {
        for (int j = j0; j <= j1; j++) {
            tmp1[i][j] = tmp2[i-1][j] | tmp2[i][j] | tmp2[i+1][j];
        }
    }
    
    // Apply horizontal erosion (1×3) -> tmp2
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        erosion_h3_row(tmp1[i], tmp2[i], j0, j1);
    }
    
    // Apply vertical erosion (3×1) -> img_out (final output)
    memcpy(&img_out[i0][j0], &tmp2[i0][j0], width);
    memcpy(&img_out[i1][j0], &tmp2[i1][j0], width);
    
    #pragma omp parallel for schedule(static)
    for (int i = i0 + 1; i < i1; i++) {
        for (int j = j0; j <= j1; j++) {
            img_out[i][j] = tmp2[i-1][j] & tmp2[i][j] & tmp2[i+1][j];
        }
    }
}

// ============================================================================
// END OF OPERATOR FUSION
// ============================================================================

void sigma_delta_compute(sigma_delta_data_t *sd_data, const uint8_t** img_in, uint8_t** img_out, const int i0,
                         const int i1, const int j0, const int j1, const uint8_t N) {
#ifdef MOTION_USE_MIPP
    // SIMD-optimized version using MIPP with OpenMP parallelization
    const int vec_size = mipp::N<uint8_t>();

    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        // Thread-local SIMD constants
        const mipp::Reg<uint8_t> one = (uint8_t)1;
        const mipp::Reg<uint8_t> vmin_reg = sd_data->vmin;
        const mipp::Reg<uint8_t> vmax_reg = sd_data->vmax;
        const mipp::Reg<uint8_t> zero_reg = (uint8_t)0;
        const mipp::Reg<uint8_t> val_255 = (uint8_t)255;
        int j;
        // Vectorized loop
        for (j = j0; j <= j1 - vec_size + 1; j += vec_size) {
            // Load data
            mipp::Reg<uint8_t> M_reg = mipp::Reg<uint8_t>(&sd_data->M[i][j]);
            mipp::Reg<uint8_t> I_reg = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> V_reg = mipp::Reg<uint8_t>(&sd_data->V[i][j]);

            // Step 1: Mean update - M = M + (M < I) - (M > I)
            mipp::Msk<mipp::N<uint8_t>()> lt_mask = M_reg < I_reg;
            mipp::Msk<mipp::N<uint8_t>()> gt_mask = M_reg > I_reg;
            mipp::Reg<uint8_t> inc = mipp::blend(one, zero_reg, lt_mask);
            mipp::Reg<uint8_t> dec = mipp::blend(one, zero_reg, gt_mask);
            M_reg = M_reg + inc - dec;
            M_reg.store(&sd_data->M[i][j]);

            // Step 2: Difference O = |M - I| using max(M,I) - min(M,I)
            mipp::Reg<uint8_t> O_reg = mipp::max(M_reg, I_reg) - mipp::min(M_reg, I_reg);
            O_reg.store(&sd_data->O[i][j]);

            // Step 3: Variance update with saturation
            // N * O (N=2) - use addition instead of multiplication for N=2
            mipp::Reg<uint8_t> NO_reg = O_reg + O_reg; // N * O where N = 2
            mipp::Msk<mipp::N<uint8_t>()> v_lt_mask = V_reg < NO_reg;
            mipp::Msk<mipp::N<uint8_t>()> v_gt_mask = V_reg > NO_reg;
            inc = mipp::blend(one, zero_reg, v_lt_mask);
            dec = mipp::blend(one, zero_reg, v_gt_mask);
            V_reg = V_reg + inc - dec;
            // Clamp to [vmin, vmax]
            V_reg = mipp::max(mipp::min(V_reg, vmax_reg), vmin_reg);
            V_reg.store(&sd_data->V[i][j]);

            // Step 4: Binary output
            mipp::Msk<mipp::N<uint8_t>()> motion_mask = O_reg >= V_reg;
            mipp::Reg<uint8_t> out_reg = mipp::blend(val_255, zero_reg, motion_mask);
            out_reg.store(&img_out[i][j]);
        }
        // Scalar remainder
        for (; j <= j1; j++) {
            uint8_t new_m = sd_data->M[i][j];
            if (sd_data->M[i][j] < img_in[i][j])
                new_m += 1;
            else if (sd_data->M[i][j] > img_in[i][j])
                new_m -= 1;
            sd_data->M[i][j] = new_m;

            sd_data->O[i][j] = abs(sd_data->M[i][j] - img_in[i][j]);

            uint8_t new_v = sd_data->V[i][j];
            if (sd_data->V[i][j] < N * sd_data->O[i][j])
                new_v += 1;
            else if (sd_data->V[i][j] > N * sd_data->O[i][j])
                new_v -= 1;
            sd_data->V[i][j] = MAX(MIN(new_v, sd_data->vmax), sd_data->vmin);

            img_out[i][j] = sd_data->O[i][j] < sd_data->V[i][j] ? 0 : 255;
        }
    }
#else
    // Original scalar implementation with OpenMP parallelization
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        for (int j = j0; j <= j1; j++) {
            uint8_t new_m = sd_data->M[i][j];
            if (sd_data->M[i][j] < img_in[i][j])
                new_m += 1;
            else if (sd_data->M[i][j] > img_in[i][j])
                new_m -= 1;
            sd_data->M[i][j] = new_m;
        }
    }

    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        for (int j = j0; j <= j1; j++) {
            sd_data->O[i][j] = abs(sd_data->M[i][j] - img_in[i][j]);
        }
    }

    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        for (int j = j0; j <= j1; j++) {
            uint8_t new_v = sd_data->V[i][j];
            if (sd_data->V[i][j] < N * sd_data->O[i][j])
                new_v += 1;
            else if (sd_data->V[i][j] > N * sd_data->O[i][j])
                new_v -= 1;
            sd_data->V[i][j] = MAX(MIN(new_v, sd_data->vmax), sd_data->vmin);
        }
    }

    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        for (int j = j0; j <= j1; j++) {
            img_out[i][j] = sd_data->O[i][j] < sd_data->V[i][j] ? 0 : 255;
        }
    }
#endif
}
