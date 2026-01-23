// OpenCL Kernels for Motion Detection
// GPU acceleration for Sigma-Delta and Morphology operations

// ============================================================================
// SIGMA-DELTA KERNEL
// Performs all 4 steps of Sigma-Delta in a single kernel to minimize
// GPU memory transfers and kernel launch overhead
// ============================================================================

__kernel void sigma_delta_compute(
    __global const uchar* img_in,      // Input grayscale image
    __global uchar* img_out,           // Output binary image (0 or 255)
    __global uchar* M,                 // Mean image (read/write)
    __global uchar* O,                 // Difference image (write)
    __global uchar* V,                 // Variance image (read/write)
    const int width,                   // Image width
    const int height,                  // Image height
    const uchar N,                     // Sigma-Delta N parameter (typically 2)
    const uchar vmin,                  // Minimum variance
    const uchar vmax                   // Maximum variance
) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height)
        return;
    
    int idx = y * width + x;
    
    // Load values
    uchar m = M[idx];
    uchar i = img_in[idx];
    uchar v = V[idx];
    
    // Step 1: Mean update (M converges toward I by ±1)
    if (m < i)
        m = m + 1;
    else if (m > i)
        m = m - 1;
    M[idx] = m;
    
    // Step 2: Compute absolute difference
    uchar o = (m > i) ? (m - i) : (i - m);
    O[idx] = o;
    
    // Step 3: Variance update with clamping
    uchar no = (uchar)min((int)o * N, 255);  // N * O with overflow protection
    if (v < no)
        v = v + 1;
    else if (v > no)
        v = v - 1;
    v = max(min(v, vmax), vmin);  // Clamp to [vmin, vmax]
    V[idx] = v;
    
    // Step 4: Binary output
    img_out[idx] = (o < v) ? 0 : 255;
}

// ============================================================================
// MORPHOLOGY KERNELS
// Erosion and Dilation with 3x3 structuring element
// ============================================================================

// 3x3 Erosion: output = AND of all 9 neighbors
__kernel void morpho_erosion3(
    __global const uchar* img_in,
    __global uchar* img_out,
    const int width,
    const int height
) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    // Handle borders: copy input to output
    if (x == 0 || x >= width - 1 || y == 0 || y >= height - 1) {
        if (x < width && y < height)
            img_out[y * width + x] = img_in[y * width + x];
        return;
    }
    
    // Load 3x3 neighborhood
    uchar r0 = img_in[(y-1) * width + (x-1)] & img_in[(y-1) * width + x] & img_in[(y-1) * width + (x+1)];
    uchar r1 = img_in[y * width + (x-1)]     & img_in[y * width + x]     & img_in[y * width + (x+1)];
    uchar r2 = img_in[(y+1) * width + (x-1)] & img_in[(y+1) * width + x] & img_in[(y+1) * width + (x+1)];
    
    // Erosion: AND all rows
    img_out[y * width + x] = r0 & r1 & r2;
}

// 3x3 Dilation: output = OR of all 9 neighbors
__kernel void morpho_dilation3(
    __global const uchar* img_in,
    __global uchar* img_out,
    const int width,
    const int height
) {
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    // Handle borders: copy input to output
    if (x == 0 || x >= width - 1 || y == 0 || y >= height - 1) {
        if (x < width && y < height)
            img_out[y * width + x] = img_in[y * width + x];
        return;
    }
    
    // Load 3x3 neighborhood
    uchar r0 = img_in[(y-1) * width + (x-1)] | img_in[(y-1) * width + x] | img_in[(y-1) * width + (x+1)];
    uchar r1 = img_in[y * width + (x-1)]     | img_in[y * width + x]     | img_in[y * width + (x+1)];
    uchar r2 = img_in[(y+1) * width + (x-1)] | img_in[(y+1) * width + x] | img_in[(y+1) * width + (x+1)];
    
    // Dilation: OR all rows
    img_out[y * width + x] = r0 | r1 | r2;
}

// ============================================================================
// OPTIMIZED MORPHOLOGY WITH LOCAL MEMORY
// Uses local memory (shared memory) to reduce global memory accesses
// Each workgroup loads a tile + halo into local memory
// ============================================================================

#define TILE_SIZE 16
#define HALO 1

__kernel void morpho_erosion3_local(
    __global const uchar* img_in,
    __global uchar* img_out,
    const int width,
    const int height
) {
    // Local memory for tile + halo (18x18 for 16x16 tile with 1-pixel halo)
    __local uchar tile[TILE_SIZE + 2*HALO][TILE_SIZE + 2*HALO];
    
    int gx = get_global_id(0);
    int gy = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    
    // Load center of tile
    int src_x = gx;
    int src_y = gy;
    if (src_x < width && src_y < height)
        tile[ly + HALO][lx + HALO] = img_in[src_y * width + src_x];
    else
        tile[ly + HALO][lx + HALO] = 0;
    
    // Load halo (edges of tile)
    // Left edge
    if (lx == 0) {
        int hx = gx - HALO;
        tile[ly + HALO][0] = (hx >= 0 && gy < height) ? img_in[gy * width + hx] : 0;
    }
    // Right edge
    if (lx == TILE_SIZE - 1 || gx == width - 1) {
        int hx = gx + HALO;
        tile[ly + HALO][lx + HALO + 1] = (hx < width && gy < height) ? img_in[gy * width + hx] : 0;
    }
    // Top edge
    if (ly == 0) {
        int hy = gy - HALO;
        tile[0][lx + HALO] = (hy >= 0 && gx < width) ? img_in[hy * width + gx] : 0;
    }
    // Bottom edge
    if (ly == TILE_SIZE - 1 || gy == height - 1) {
        int hy = gy + HALO;
        tile[ly + HALO + 1][lx + HALO] = (hy < height && gx < width) ? img_in[hy * width + gx] : 0;
    }
    // Corners
    if (lx == 0 && ly == 0)
        tile[0][0] = (gx > 0 && gy > 0) ? img_in[(gy-1) * width + (gx-1)] : 0;
    if (lx == TILE_SIZE - 1 && ly == 0)
        tile[0][lx + HALO + 1] = (gx < width - 1 && gy > 0) ? img_in[(gy-1) * width + (gx+1)] : 0;
    if (lx == 0 && ly == TILE_SIZE - 1)
        tile[ly + HALO + 1][0] = (gx > 0 && gy < height - 1) ? img_in[(gy+1) * width + (gx-1)] : 0;
    if (lx == TILE_SIZE - 1 && ly == TILE_SIZE - 1)
        tile[ly + HALO + 1][lx + HALO + 1] = (gx < width - 1 && gy < height - 1) ? img_in[(gy+1) * width + (gx+1)] : 0;
    
    // Synchronize to ensure all data is loaded
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // Compute erosion from local memory
    if (gx >= width || gy >= height)
        return;
    
    // Handle borders
    if (gx == 0 || gx >= width - 1 || gy == 0 || gy >= height - 1) {
        img_out[gy * width + gx] = img_in[gy * width + gx];
        return;
    }
    
    int tx = lx + HALO;
    int ty = ly + HALO;
    
    uchar r0 = tile[ty-1][tx-1] & tile[ty-1][tx] & tile[ty-1][tx+1];
    uchar r1 = tile[ty][tx-1]   & tile[ty][tx]   & tile[ty][tx+1];
    uchar r2 = tile[ty+1][tx-1] & tile[ty+1][tx] & tile[ty+1][tx+1];
    
    img_out[gy * width + gx] = r0 & r1 & r2;
}

__kernel void morpho_dilation3_local(
    __global const uchar* img_in,
    __global uchar* img_out,
    const int width,
    const int height
) {
    __local uchar tile[TILE_SIZE + 2*HALO][TILE_SIZE + 2*HALO];
    
    int gx = get_global_id(0);
    int gy = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    
    // Load center of tile
    if (gx < width && gy < height)
        tile[ly + HALO][lx + HALO] = img_in[gy * width + gx];
    else
        tile[ly + HALO][lx + HALO] = 0;
    
    // Load halo (same as erosion)
    if (lx == 0) {
        int hx = gx - HALO;
        tile[ly + HALO][0] = (hx >= 0 && gy < height) ? img_in[gy * width + hx] : 0;
    }
    if (lx == TILE_SIZE - 1 || gx == width - 1) {
        int hx = gx + HALO;
        tile[ly + HALO][lx + HALO + 1] = (hx < width && gy < height) ? img_in[gy * width + hx] : 0;
    }
    if (ly == 0) {
        int hy = gy - HALO;
        tile[0][lx + HALO] = (hy >= 0 && gx < width) ? img_in[hy * width + gx] : 0;
    }
    if (ly == TILE_SIZE - 1 || gy == height - 1) {
        int hy = gy + HALO;
        tile[ly + HALO + 1][lx + HALO] = (hy < height && gx < width) ? img_in[hy * width + gx] : 0;
    }
    if (lx == 0 && ly == 0)
        tile[0][0] = (gx > 0 && gy > 0) ? img_in[(gy-1) * width + (gx-1)] : 0;
    if (lx == TILE_SIZE - 1 && ly == 0)
        tile[0][lx + HALO + 1] = (gx < width - 1 && gy > 0) ? img_in[(gy-1) * width + (gx+1)] : 0;
    if (lx == 0 && ly == TILE_SIZE - 1)
        tile[ly + HALO + 1][0] = (gx > 0 && gy < height - 1) ? img_in[(gy+1) * width + (gx-1)] : 0;
    if (lx == TILE_SIZE - 1 && ly == TILE_SIZE - 1)
        tile[ly + HALO + 1][lx + HALO + 1] = (gx < width - 1 && gy < height - 1) ? img_in[(gy+1) * width + (gx+1)] : 0;
    
    barrier(CLK_LOCAL_MEM_FENCE);
    
    if (gx >= width || gy >= height)
        return;
    
    if (gx == 0 || gx >= width - 1 || gy == 0 || gy >= height - 1) {
        img_out[gy * width + gx] = img_in[gy * width + gx];
        return;
    }
    
    int tx = lx + HALO;
    int ty = ly + HALO;
    
    uchar r0 = tile[ty-1][tx-1] | tile[ty-1][tx] | tile[ty-1][tx+1];
    uchar r1 = tile[ty][tx-1]   | tile[ty][tx]   | tile[ty][tx+1];
    uchar r2 = tile[ty+1][tx-1] | tile[ty+1][tx] | tile[ty+1][tx+1];
    
    img_out[gy * width + gx] = r0 | r1 | r2;
}

// ============================================================================
// FUSED SIGMA-DELTA + EROSION KERNEL
// Reduces memory transfers by computing Sigma-Delta output and immediately
// feeding it to the first morphology pass
// ============================================================================

__kernel void sigma_delta_erosion_fused(
    __global const uchar* img_in,      // Input grayscale image
    __global uchar* img_out,           // Output after erosion
    __global uchar* M,                 // Mean image
    __global uchar* O,                 // Difference image
    __global uchar* V,                 // Variance image
    const int width,
    const int height,
    const uchar N,
    const uchar vmin,
    const uchar vmax
) {
    __local uchar binary_tile[TILE_SIZE + 2*HALO][TILE_SIZE + 2*HALO];
    
    int gx = get_global_id(0);
    int gy = get_global_id(1);
    int lx = get_local_id(0);
    int ly = get_local_id(1);
    
    // Step 1: Compute Sigma-Delta for this pixel
    uchar binary_result = 0;
    if (gx < width && gy < height) {
        int idx = gy * width + gx;
        
        uchar m = M[idx];
        uchar i = img_in[idx];
        uchar v = V[idx];
        
        if (m < i) m++;
        else if (m > i) m--;
        M[idx] = m;
        
        uchar o = (m > i) ? (m - i) : (i - m);
        O[idx] = o;
        
        uchar no = (uchar)min((int)o * N, 255);
        if (v < no) v++;
        else if (v > no) v--;
        v = max(min(v, vmax), vmin);
        V[idx] = v;
        
        binary_result = (o < v) ? 0 : 255;
    }
    
    // Store to local memory for erosion
    tile[ly + HALO][lx + HALO] = binary_result;
    
    // Load halo from neighbors (need to recompute Sigma-Delta for halo pixels)
    // For simplicity, we synchronize and use the computed values
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // For a complete implementation, halo loading would require additional logic
    // This is a simplified version that shows the concept
    
    if (gx >= width || gy >= height)
        return;
    
    // For now, just output the binary result
    // Full fusion would require more complex halo handling
    img_out[gy * width + gx] = binary_result;
}
