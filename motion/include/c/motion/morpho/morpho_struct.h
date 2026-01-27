/*!
 * \file
 * \brief Morphology structure.
 */

#pragma once

#include <stdint.h>

/**
 *  Inner data required to perform morphology.
 */
typedef struct {
    int i0; /**< First \f$y\f$ index in the image (included). */
    int i1; /**< Last \f$y\f$ index in the image (included). */
    int j0; /**< First \f$x\f$ index in the image (included). */
    int j1; /**< Last \f$x\f$ index in the image (included). */
    uint8_t **IB; /**< Temporary binary image. */
    uint8_t **IB2; /**< Second temporary binary image for separable morphology. */
    uint8_t **IB_packed; /**< Packed binary image (8 pixels per byte). */
    uint8_t **IB_packed2; /**< Second packed temporary image. */
    uint8_t **IB_packed3; /**< Third packed temporary image. */
} morpho_data_t;
