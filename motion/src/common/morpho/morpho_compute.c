#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <nrc2.h>

#include "motion/macros.h"
#include "motion/morpho/morpho_compute.h"

#ifdef MOTION_USE_MIPP
#include <mipp.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

morpho_data_t* morpho_alloc_data(const int i0, const int i1, const int j0, const int j1) {
    morpho_data_t* morpho_data = (morpho_data_t*)malloc(sizeof(morpho_data_t));
    morpho_data->i0 = i0;
    morpho_data->i1 = i1;
    morpho_data->j0 = j0;
    morpho_data->j1 = j1;
    morpho_data->IB = ui8matrix(morpho_data->i0, morpho_data->i1, morpho_data->j0, morpho_data->j1);
    morpho_data->IB2 = ui8matrix(morpho_data->i0, morpho_data->i1, morpho_data->j0, morpho_data->j1);
    return morpho_data;
}

void morpho_init_data(morpho_data_t* morpho_data) {
    zero_ui8matrix(morpho_data->IB , morpho_data->i0, morpho_data->i1, morpho_data->j0, morpho_data->j1);
    zero_ui8matrix(morpho_data->IB2, morpho_data->i0, morpho_data->i1, morpho_data->j0, morpho_data->j1);
}

void morpho_free_data(morpho_data_t* morpho_data) {
    free_ui8matrix(morpho_data->IB, morpho_data->i0, morpho_data->i1, morpho_data->j0, morpho_data->j1);
    free_ui8matrix(morpho_data->IB2, morpho_data->i0, morpho_data->i1, morpho_data->j0, morpho_data->j1);
    free(morpho_data);
}

// Separable morphology: horizontal erosion (1x3)
static void morpho_compute_erosion_h3(const uint8_t** img_in, uint8_t** img_out, const int i0, const int i1,
                                      const int j0, const int j1) {
    // Copy borders
    for (int i = i0; i <= i1; i++) {
        img_out[i][j0] = img_in[i][j0];
        img_out[i][j1] = img_in[i][j1];
    }

#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        int j;
        for (j = j0 + 1; j <= j1 - 1 - vec_size; j += vec_size) {
            mipp::Reg<uint8_t> left   = mipp::Reg<uint8_t>(&img_in[i][j-1]);
            mipp::Reg<uint8_t> center = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> right  = mipp::Reg<uint8_t>(&img_in[i][j+1]);
            mipp::Reg<uint8_t> result = left & center & right;
            result.store(&img_out[i][j]);
        }
        for (; j <= j1 - 1; j++) {
            img_out[i][j] = img_in[i][j-1] & img_in[i][j] & img_in[i][j+1];
        }
    }
#else
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        for (int j = j0 + 1; j <= j1 - 1; j++) {
            img_out[i][j] = img_in[i][j-1] & img_in[i][j] & img_in[i][j+1];
        }
    }
#endif
}

// Separable morphology: vertical erosion (3x1)
static void morpho_compute_erosion_v3(const uint8_t** img_in, uint8_t** img_out, const int i0, const int i1,
                                      const int j0, const int j1) {
    // Copy borders
    for (int j = j0; j <= j1; j++) {
        img_out[i0][j] = img_in[i0][j];
        img_out[i1][j] = img_in[i1][j];
    }

#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        int j;
        for (j = j0; j <= j1 - vec_size + 1; j += vec_size) {
            mipp::Reg<uint8_t> top    = mipp::Reg<uint8_t>(&img_in[i-1][j]);
            mipp::Reg<uint8_t> center = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> bottom = mipp::Reg<uint8_t>(&img_in[i+1][j]);
            mipp::Reg<uint8_t> result = top & center & bottom;
            result.store(&img_out[i][j]);
        }
        for (; j <= j1; j++) {
            img_out[i][j] = img_in[i-1][j] & img_in[i][j] & img_in[i+1][j];
        }
    }
#else
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        for (int j = j0; j <= j1; j++) {
            img_out[i][j] = img_in[i-1][j] & img_in[i][j] & img_in[i+1][j];
        }
    }
#endif
}

// Separable morphology: horizontal dilation (1x3)
static void morpho_compute_dilation_h3(const uint8_t** img_in, uint8_t** img_out, const int i0, const int i1,
                                       const int j0, const int j1) {
    // Copy borders
    for (int i = i0; i <= i1; i++) {
        img_out[i][j0] = img_in[i][j0];
        img_out[i][j1] = img_in[i][j1];
    }

#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        int j;
        for (j = j0 + 1; j <= j1 - 1 - vec_size; j += vec_size) {
            mipp::Reg<uint8_t> left   = mipp::Reg<uint8_t>(&img_in[i][j-1]);
            mipp::Reg<uint8_t> center = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> right  = mipp::Reg<uint8_t>(&img_in[i][j+1]);
            mipp::Reg<uint8_t> result = left | center | right;
            result.store(&img_out[i][j]);
        }
        for (; j <= j1 - 1; j++) {
            img_out[i][j] = img_in[i][j-1] | img_in[i][j] | img_in[i][j+1];
        }
    }
#else
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0; i <= i1; i++) {
        for (int j = j0 + 1; j <= j1 - 1; j++) {
            img_out[i][j] = img_in[i][j-1] | img_in[i][j] | img_in[i][j+1];
        }
    }
#endif
}

// Separable morphology: vertical dilation (3x1)
static void morpho_compute_dilation_v3(const uint8_t** img_in, uint8_t** img_out, const int i0, const int i1,
                                       const int j0, const int j1) {
    // Copy borders
    for (int j = j0; j <= j1; j++) {
        img_out[i0][j] = img_in[i0][j];
        img_out[i1][j] = img_in[i1][j];
    }

#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        int j;
        for (j = j0; j <= j1 - vec_size + 1; j += vec_size) {
            mipp::Reg<uint8_t> top    = mipp::Reg<uint8_t>(&img_in[i-1][j]);
            mipp::Reg<uint8_t> center = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> bottom = mipp::Reg<uint8_t>(&img_in[i+1][j]);
            mipp::Reg<uint8_t> result = top | center | bottom;
            result.store(&img_out[i][j]);
        }
        for (; j <= j1; j++) {
            img_out[i][j] = img_in[i-1][j] | img_in[i][j] | img_in[i+1][j];
        }
    }
#else
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        for (int j = j0; j <= j1; j++) {
            img_out[i][j] = img_in[i-1][j] | img_in[i][j] | img_in[i+1][j];
        }
    }
#endif
}

void morpho_compute_erosion3(const uint8_t** img_in, uint8_t** img_out, const int i0, const int i1, const int j0,
                             const int j1) {
    assert(img_in != NULL);
    assert(img_out != NULL);
    assert(img_in != (const uint8_t**)img_out);

    // copy the borders
    for (int i = i0; i <= i1; i++) {
        img_out[i][j0] = img_in[i][j0];
        img_out[i][j1] = img_in[i][j1];
    }
    for (int j = j0; j <= j1; j++) {
        img_out[i0][j] = img_in[i0][j];
        img_out[i1][j] = img_in[i1][j];
    }

#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        int j;
        // Vectorized inner loop - process vec_size pixels at a time
        for (j = j0 + 1; j <= j1 - 1 - vec_size; j += vec_size) {
            // Load 3 rows with left, center, right shifts
            // Row i-1
            mipp::Reg<uint8_t> r0_l = mipp::Reg<uint8_t>(&img_in[i-1][j-1]);
            mipp::Reg<uint8_t> r0_c = mipp::Reg<uint8_t>(&img_in[i-1][j]);
            mipp::Reg<uint8_t> r0_r = mipp::Reg<uint8_t>(&img_in[i-1][j+1]);
            // Row i
            mipp::Reg<uint8_t> r1_l = mipp::Reg<uint8_t>(&img_in[i][j-1]);
            mipp::Reg<uint8_t> r1_c = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> r1_r = mipp::Reg<uint8_t>(&img_in[i][j+1]);
            // Row i+1
            mipp::Reg<uint8_t> r2_l = mipp::Reg<uint8_t>(&img_in[i+1][j-1]);
            mipp::Reg<uint8_t> r2_c = mipp::Reg<uint8_t>(&img_in[i+1][j]);
            mipp::Reg<uint8_t> r2_r = mipp::Reg<uint8_t>(&img_in[i+1][j+1]);

            // Erosion: AND all 9 neighbors
            mipp::Reg<uint8_t> c0 = r0_l & r0_c & r0_r;
            mipp::Reg<uint8_t> c1 = r1_l & r1_c & r1_r;
            mipp::Reg<uint8_t> c2 = r2_l & r2_c & r2_r;
            mipp::Reg<uint8_t> result = c0 & c1 & c2;
            result.store(&img_out[i][j]);
        }
        // Scalar remainder
        for (; j <= j1 - 1; j++) {
            uint8_t c0 = img_in[i - 1][j - 1] & img_in[i - 1][j] & img_in[i - 1][j + 1];
            uint8_t c1 = img_in[i + 0][j - 1] & img_in[i + 0][j] & img_in[i + 0][j + 1];
            uint8_t c2 = img_in[i + 1][j - 1] & img_in[i + 1][j] & img_in[i + 1][j + 1];
            img_out[i][j] = c0 & c1 & c2;
        }
    }
#else
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        for (int j = j0 + 1; j <= j1 - 1; j++) {
            uint8_t c0 = img_in[i - 1][j - 1] & img_in[i - 1][j] & img_in[i - 1][j + 1];
            uint8_t c1 = img_in[i + 0][j - 1] & img_in[i + 0][j] & img_in[i + 0][j + 1];
            uint8_t c2 = img_in[i + 1][j - 1] & img_in[i + 1][j] & img_in[i + 1][j + 1];
            img_out[i][j] = c0 & c1 & c2;
        }
    }
#endif
}

void morpho_compute_dilation3(const uint8_t** img_in, uint8_t** img_out, const int i0, const int i1, const int j0,
                              const int j1) {
    assert(img_in != NULL);
    assert(img_out != NULL);
    assert(img_in != (const uint8_t**)img_out);

    // copy the borders
    for (int i = i0; i <= i1; i++) {
        img_out[i][j0] = img_in[i][j0];
        img_out[i][j1] = img_in[i][j1];
    }
    for (int j = j0; j <= j1; j++) {
        img_out[i0][j] = img_in[i0][j];
        img_out[i1][j] = img_in[i1][j];
    }

#ifdef MOTION_USE_MIPP
    const int vec_size = mipp::N<uint8_t>();
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        int j;
        // Vectorized inner loop - process vec_size pixels at a time
        for (j = j0 + 1; j <= j1 - 1 - vec_size; j += vec_size) {
            // Load 3 rows with left, center, right shifts
            // Row i-1
            mipp::Reg<uint8_t> r0_l = mipp::Reg<uint8_t>(&img_in[i-1][j-1]);
            mipp::Reg<uint8_t> r0_c = mipp::Reg<uint8_t>(&img_in[i-1][j]);
            mipp::Reg<uint8_t> r0_r = mipp::Reg<uint8_t>(&img_in[i-1][j+1]);
            // Row i
            mipp::Reg<uint8_t> r1_l = mipp::Reg<uint8_t>(&img_in[i][j-1]);
            mipp::Reg<uint8_t> r1_c = mipp::Reg<uint8_t>(&img_in[i][j]);
            mipp::Reg<uint8_t> r1_r = mipp::Reg<uint8_t>(&img_in[i][j+1]);
            // Row i+1
            mipp::Reg<uint8_t> r2_l = mipp::Reg<uint8_t>(&img_in[i+1][j-1]);
            mipp::Reg<uint8_t> r2_c = mipp::Reg<uint8_t>(&img_in[i+1][j]);
            mipp::Reg<uint8_t> r2_r = mipp::Reg<uint8_t>(&img_in[i+1][j+1]);

            // Dilation: OR all 9 neighbors
            mipp::Reg<uint8_t> c0 = r0_l | r0_c | r0_r;
            mipp::Reg<uint8_t> c1 = r1_l | r1_c | r1_r;
            mipp::Reg<uint8_t> c2 = r2_l | r2_c | r2_r;
            mipp::Reg<uint8_t> result = c0 | c1 | c2;
            result.store(&img_out[i][j]);
        }
        // Scalar remainder
        for (; j <= j1 - 1; j++) {
            uint8_t c0 = img_in[i - 1][j - 1] | img_in[i - 1][j] | img_in[i - 1][j + 1];
            uint8_t c1 = img_in[i + 0][j - 1] | img_in[i + 0][j] | img_in[i + 0][j + 1];
            uint8_t c2 = img_in[i + 1][j - 1] | img_in[i + 1][j] | img_in[i + 1][j + 1];
            img_out[i][j] = c0 | c1 | c2;
        }
    }
#else
    #pragma omp parallel for schedule(dynamic)
    for (int i = i0 + 1; i <= i1 - 1; i++) {
        for (int j = j0 + 1; j <= j1 - 1; j++) {
            uint8_t c0 = img_in[i - 1][j - 1] | img_in[i - 1][j] | img_in[i - 1][j + 1];
            uint8_t c1 = img_in[i + 0][j - 1] | img_in[i + 0][j] | img_in[i + 0][j + 1];
            uint8_t c2 = img_in[i + 1][j - 1] | img_in[i + 1][j] | img_in[i + 1][j + 1];
            img_out[i][j] = c0 | c1 | c2;
        }
    }
#endif
}

void morpho_compute_opening3(morpho_data_t* morpho_data, const uint8_t** img_in, uint8_t** img_out, const int i0,
                             const int i1, const int j0, const int j1) {
    assert(img_in != NULL);
    assert(img_out != NULL);
    // Separable opening: erosion_h -> erosion_v -> dilation_h -> dilation_v
    morpho_compute_erosion_h3 (img_in, morpho_data->IB, i0, i1, j0, j1);
    morpho_compute_erosion_v3 ((const uint8_t**)morpho_data->IB, morpho_data->IB2, i0, i1, j0, j1);
    morpho_compute_dilation_h3((const uint8_t**)morpho_data->IB2, morpho_data->IB, i0, i1, j0, j1);
    morpho_compute_dilation_v3((const uint8_t**)morpho_data->IB, img_out, i0, i1, j0, j1);
}

void morpho_compute_closing3(morpho_data_t* morpho_data, const uint8_t** img_in, uint8_t** img_out, const int i0,
                             const int i1, const int j0, const int j1) {
    assert(img_in != NULL);
    assert(img_out != NULL);
    // Separable closing: dilation_h -> dilation_v -> erosion_h -> erosion_v
    morpho_compute_dilation_h3(img_in, morpho_data->IB, i0, i1, j0, j1);
    morpho_compute_dilation_v3((const uint8_t**)morpho_data->IB, morpho_data->IB2, i0, i1, j0, j1);
    morpho_compute_erosion_h3 ((const uint8_t**)morpho_data->IB2, morpho_data->IB, i0, i1, j0, j1);
    morpho_compute_erosion_v3 ((const uint8_t**)morpho_data->IB, img_out, i0, i1, j0, j1);
}

// ============================================================================
// BIT-PACKING OPTIMIZATION (Section 2.5.4 of project)
// Store 8 binary pixels per byte instead of 1 pixel per byte
// This reduces memory bandwidth by 8x for morphology operations
// ============================================================================

// Convert from 0/255 format to packed 1-bit format (8 pixels per byte)
// Input: img_in[i][j] = 0 or 255
// Output: img_packed[i][j/8] = 8 bits representing 8 consecutive pixels
void morpho_pack_binary(const uint8_t** img_in, uint8_t** img_packed,
                        const int i0, const int i1, const int j0, const int j1) {
    const int packed_j1 = (j1 - j0 + 1) / 8 + j0;
    
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        for (int jp = j0; jp < packed_j1; jp++) {
            int j = j0 + (jp - j0) * 8;
            uint8_t packed = 0;
            // Pack 8 pixels into one byte (MSB first)
            packed |= (img_in[i][j + 0] ? 0x80 : 0);
            packed |= (img_in[i][j + 1] ? 0x40 : 0);
            packed |= (img_in[i][j + 2] ? 0x20 : 0);
            packed |= (img_in[i][j + 3] ? 0x10 : 0);
            packed |= (img_in[i][j + 4] ? 0x08 : 0);
            packed |= (img_in[i][j + 5] ? 0x04 : 0);
            packed |= (img_in[i][j + 6] ? 0x02 : 0);
            packed |= (img_in[i][j + 7] ? 0x01 : 0);
            img_packed[i][jp] = packed;
        }
    }
}

// Convert from packed 1-bit format back to 0/255 format
void morpho_unpack_binary(const uint8_t** img_packed, uint8_t** img_out,
                          const int i0, const int i1, const int j0, const int j1) {
    const int packed_j1 = (j1 - j0 + 1) / 8 + j0;
    
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        for (int jp = j0; jp < packed_j1; jp++) {
            int j = j0 + (jp - j0) * 8;
            uint8_t packed = img_packed[i][jp];
            // Unpack 8 pixels from one byte (MSB first)
            img_out[i][j + 0] = (packed & 0x80) ? 255 : 0;
            img_out[i][j + 1] = (packed & 0x40) ? 255 : 0;
            img_out[i][j + 2] = (packed & 0x20) ? 255 : 0;
            img_out[i][j + 3] = (packed & 0x10) ? 255 : 0;
            img_out[i][j + 4] = (packed & 0x08) ? 255 : 0;
            img_out[i][j + 5] = (packed & 0x04) ? 255 : 0;
            img_out[i][j + 6] = (packed & 0x02) ? 255 : 0;
            img_out[i][j + 7] = (packed & 0x01) ? 255 : 0;
        }
    }
}

// Packed horizontal erosion (1x3) - operates on bit-packed data
// Each byte contains 8 pixels, so we need to handle bit shifts across byte boundaries
static void morpho_compute_erosion_h3_packed(const uint8_t** img_in, uint8_t** img_out,
                                              const int i0, const int i1,
                                              const int jp0, const int jp1) {
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        // Handle borders
        img_out[i][jp0] = img_in[i][jp0];
        img_out[i][jp1] = img_in[i][jp1];
        
        for (int jp = jp0 + 1; jp < jp1; jp++) {
            // Get current byte and neighbors for bit shifting
            uint8_t prev = img_in[i][jp - 1];
            uint8_t curr = img_in[i][jp];
            uint8_t next = img_in[i][jp + 1];
            
            // Shift left by 1 bit (need MSB from next byte)
            uint8_t left = (curr << 1) | (next >> 7);
            // Shift right by 1 bit (need LSB from prev byte)
            uint8_t right = (curr >> 1) | (prev << 7);
            
            // Erosion: AND center with left and right shifted versions
            img_out[i][jp] = curr & left & right;
        }
    }
}

// Packed vertical erosion (3x1) - operates on bit-packed data
static void morpho_compute_erosion_v3_packed(const uint8_t** img_in, uint8_t** img_out,
                                              const int i0, const int i1,
                                              const int jp0, const int jp1) {
    // Handle border rows
    for (int jp = jp0; jp <= jp1; jp++) {
        img_out[i0][jp] = img_in[i0][jp];
        img_out[i1][jp] = img_in[i1][jp];
    }
    
    #pragma omp parallel for schedule(static)
    for (int i = i0 + 1; i < i1; i++) {
        for (int jp = jp0; jp <= jp1; jp++) {
            // Erosion: AND with row above and row below
            img_out[i][jp] = img_in[i-1][jp] & img_in[i][jp] & img_in[i+1][jp];
        }
    }
}

// Packed horizontal dilation (1x3)
static void morpho_compute_dilation_h3_packed(const uint8_t** img_in, uint8_t** img_out,
                                               const int i0, const int i1,
                                               const int jp0, const int jp1) {
    #pragma omp parallel for schedule(static)
    for (int i = i0; i <= i1; i++) {
        // Handle borders
        img_out[i][jp0] = img_in[i][jp0];
        img_out[i][jp1] = img_in[i][jp1];
        
        for (int jp = jp0 + 1; jp < jp1; jp++) {
            uint8_t prev = img_in[i][jp - 1];
            uint8_t curr = img_in[i][jp];
            uint8_t next = img_in[i][jp + 1];
            
            // Shift left by 1 bit
            uint8_t left = (curr << 1) | (next >> 7);
            // Shift right by 1 bit
            uint8_t right = (curr >> 1) | (prev << 7);
            
            // Dilation: OR center with left and right shifted versions
            img_out[i][jp] = curr | left | right;
        }
    }
}

// Packed vertical dilation (3x1)
static void morpho_compute_dilation_v3_packed(const uint8_t** img_in, uint8_t** img_out,
                                               const int i0, const int i1,
                                               const int jp0, const int jp1) {
    // Handle border rows
    for (int jp = jp0; jp <= jp1; jp++) {
        img_out[i0][jp] = img_in[i0][jp];
        img_out[i1][jp] = img_in[i1][jp];
    }
    
    #pragma omp parallel for schedule(static)
    for (int i = i0 + 1; i < i1; i++) {
        for (int jp = jp0; jp <= jp1; jp++) {
            // Dilation: OR with row above and row below
            img_out[i][jp] = img_in[i-1][jp] | img_in[i][jp] | img_in[i+1][jp];
        }
    }
}

// Packed opening: erosion then dilation (all in packed format)
void morpho_compute_opening3_packed(uint8_t** img_packed, uint8_t** tmp1, uint8_t** tmp2,
                                    const int i0, const int i1, const int jp0, const int jp1) {
    // Separable opening in packed format: erosion_h -> erosion_v -> dilation_h -> dilation_v
    morpho_compute_erosion_h3_packed((const uint8_t**)img_packed, tmp1, i0, i1, jp0, jp1);
    morpho_compute_erosion_v3_packed((const uint8_t**)tmp1, tmp2, i0, i1, jp0, jp1);
    morpho_compute_dilation_h3_packed((const uint8_t**)tmp2, tmp1, i0, i1, jp0, jp1);
    morpho_compute_dilation_v3_packed((const uint8_t**)tmp1, img_packed, i0, i1, jp0, jp1);
}

// Packed closing: dilation then erosion (all in packed format)
void morpho_compute_closing3_packed(uint8_t** img_packed, uint8_t** tmp1, uint8_t** tmp2,
                                    const int i0, const int i1, const int jp0, const int jp1) {
    // Separable closing in packed format: dilation_h -> dilation_v -> erosion_h -> erosion_v
    morpho_compute_dilation_h3_packed((const uint8_t**)img_packed, tmp1, i0, i1, jp0, jp1);
    morpho_compute_dilation_v3_packed((const uint8_t**)tmp1, tmp2, i0, i1, jp0, jp1);
    morpho_compute_erosion_h3_packed((const uint8_t**)tmp2, tmp1, i0, i1, jp0, jp1);
    morpho_compute_erosion_v3_packed((const uint8_t**)tmp1, img_packed, i0, i1, jp0, jp1);
}

// ============================================================================
// END OF BIT-PACKING OPTIMIZATION
// ============================================================================
