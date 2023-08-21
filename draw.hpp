#pragma once

#include <cstdint>

struct DrawingContext {
protected:
    uint32_t* m_pixels = nullptr;
    int m_width = 0;
    int m_height = 0;

public:
    DrawingContext(uint32_t* pixels, int width, int height);

    /** Returns the width of the underlying pixel buffer, in pixels. */
    int width() const { return m_width; }

    /** Returns the height of the underlying pixel buffer, in pixels. */
    int height() const { return m_height; }

    void xline(int x, int y, int width, uint32_t pixel);
    void yline(int x, int y, int height, uint32_t pixel);
    void draw_rect(int x, int y, int width, int height, uint32_t pixel);
    void fill_rect(int x, int y, int width, int height, uint32_t pixel);
};
