#pragma once

#include <cstdint>

struct DrawingContext {
    uint32_t* m_pixels; /**< Pointer to the start of the pixel array */
    int m_width;      /**< Width of the buffer, in pixels */
    int m_height;     /**< Height of the buffer, in pixels */
    int m_stride;     /**< Distance, in bytes, between two consecutive lines */

    void clear(uint32_t pixel);
    void xline(int x, int y, int width, uint32_t pixel);
    void yline(int x, int y, int height, uint32_t pixel);
    void draw_rect(int x, int y, int width, int height, uint32_t pixel);
    void fill_rect(int x, int y, int width, int height, uint32_t pixel);
};
