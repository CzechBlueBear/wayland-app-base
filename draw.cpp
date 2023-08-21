#include "draw.hpp"
#include <cassert>

DrawingContext::DrawingContext(uint32_t* pixels, int width, int height)
    : m_pixels(pixels), m_width(width), m_height(height)
{
    assert(m_pixels && m_width >= 0 && m_height >= 0);
}

/**
 * Draws a horizontal line from (x, y) to (x+width-1, y),
 * using the given pixel value.
 * Line is automatically clipped against the underlying pixel buffer boundaries.
 * Negative starting coordinates are safe and work as expected.
 */
void DrawingContext::xline(int x, int y, int width, uint32_t pixel) {
    if (y < 0 || y >= m_height || x >= m_width || width <= 0) { return; }
    if (x < 0) { x = 0; }
    if (x + width > m_width) { width = m_width - x; }

    assert(m_pixels);
    uint32_t* addr = m_pixels + y*m_width + x;
    uint32_t* end = addr + width;

    for(; addr < end; addr++) {
        *addr = pixel;
    }
}

void DrawingContext::yline(int x, int y, int height, uint32_t pixel) {
    if (x < 0 || x >= m_width || y >= height || height <= 0) { return; }
    if (y < 0) y = 0;
    if (y + height > m_height) { height = m_height - y; }

    assert(m_pixels);
    uint32_t* addr = m_pixels + y*m_width + x;
    uint32_t* end = m_pixels + (y+height)*m_width + x;

    for(; addr < end; addr += m_width) {
        *addr = pixel;
    }
}

void DrawingContext::fill_rect(int x, int y, int width, int height, uint32_t pixel) {
    for (int i=0; i < height; ++i) {
        xline(x, y+i, width, pixel);
    }
}
