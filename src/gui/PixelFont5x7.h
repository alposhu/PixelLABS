#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace PixelLabs {

struct PixelCanvas {
    uint32_t* pixels = nullptr;
    int width = 0;
    int height = 0;

    inline void setPixel(int x, int y, uint32_t color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            pixels[y * width + x] = color;
        }
    }

    inline void blendPixel(int x, int y, uint32_t color, uint8_t alpha) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        uint32_t dst = pixels[y * width + x];
        uint32_t inv = 255 - alpha;
        uint32_t r = (((color >> 16) & 0xFF) * alpha + ((dst >> 16) & 0xFF) * inv) >> 8;
        uint32_t g = (((color >> 8)  & 0xFF) * alpha + ((dst >> 8)  & 0xFF) * inv) >> 8;
        uint32_t b = (((color)       & 0xFF) * alpha + ((dst)       & 0xFF) * inv) >> 8;
        pixels[y * width + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }

    void fillRect(int x, int y, int w, int h, uint32_t color) {
        int x0 = std::max(0, x);
        int y0 = std::max(0, y);
        int x1 = std::min(width, x + w);
        int y1 = std::min(height, y + h);
        for (int py = y0; py < y1; ++py) {
            uint32_t* row = pixels + py * width;
            for (int px = x0; px < x1; ++px) {
                row[px] = color;
            }
        }
    }

    // Pixel-artchamfered box (1-pixel or 2-pixel notched corners)
    void fillPixelBox(int x, int y, int w, int h, uint32_t fillCol, uint32_t borderCol, int cut = 2) {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                int cx = std::min(dx, w - 1 - dx);
                int cy = std::min(dy, h - 1 - dy);
                if (cx + cy < cut) continue;
                bool isBorder = (dx == 0 || dx == w - 1 || dy == 0 || dy == h - 1 || (cx + cy == cut));
                setPixel(x + dx, y + dy, isBorder ? borderCol : fillCol);
            }
        }
    }

    void drawRectBorder(int x, int y, int w, int h, uint32_t color, int thickness = 1) {
        fillRect(x, y, w, thickness, color);
        fillRect(x, y + h - thickness, w, thickness, color);
        fillRect(x, y, thickness, h, color);
        fillRect(x + w - thickness, y, thickness, h, color);
    }

    void drawLine(int x0, int y0, int x1, int y1, uint32_t color, int thickness = 1) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            if (thickness <= 1) {
                setPixel(x0, y0, color);
            } else {
                fillRect(x0 - thickness / 2, y0 - thickness / 2, thickness, thickness, color);
            }
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Crisp pixel-snapped circle/ring
    void drawPixelCircle(int cx, int cy, int r, uint32_t color, int thickness = 2, int pixelSnap = 2) {
        int rOuter = r + thickness / 2;
        int rInner = std::max(0, r - (thickness - thickness / 2));
        int rOut2 = rOuter * rOuter;
        int rIn2  = rInner * rInner;
        for (int dy = -rOuter; dy <= rOuter; dy += pixelSnap) {
            for (int dx = -rOuter; dx <= rOuter; dx += pixelSnap) {
                int d2 = dx * dx + dy * dy;
                if (d2 >= rIn2 && d2 <= rOut2) {
                    fillRect(cx + dx, cy + dy, pixelSnap, pixelSnap, color);
                }
            }
        }
    }

    void fillCircle(int cx, int cy, int r, uint32_t color, int pixelSnap = 2) {
        int r2 = r * r;
        for (int dy = -r; dy <= r; dy += pixelSnap) {
            for (int dx = -r; dx <= r; dx += pixelSnap) {
                if (dx * dx + dy * dy <= r2) {
                    fillRect(cx + dx, cy + dy, pixelSnap, pixelSnap, color);
                }
            }
        }
    }
};

// 5x7 Pixel Art Font (5 columns x 7 rows per glyph, MSB at top or row-wise)
inline const uint8_t* getGlyph5x7(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    static const uint8_t kSpace[7] = {0,0,0,0,0,0,0};
    switch (c) {
        case 'A': { static const uint8_t g[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return g; }
        case 'B': { static const uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; return g; }
        case 'C': { static const uint8_t g[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; return g; }
        case 'D': { static const uint8_t g[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; return g; }
        case 'E': { static const uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; return g; }
        case 'F': { static const uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; return g; }
        case 'G': { static const uint8_t g[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}; return g; }
        case 'H': { static const uint8_t g[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; return g; }
        case 'I': { static const uint8_t g[7]={0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}; return g; }
        case 'J': { static const uint8_t g[7]={0x07,0x02,0x02,0x02,0x12,0x12,0x0C}; return g; }
        case 'K': { static const uint8_t g[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11}; return g; }
        case 'L': { static const uint8_t g[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return g; }
        case 'M': { static const uint8_t g[7]={0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; return g; }
        case 'N': { static const uint8_t g[7]={0x11,0x19,0x15,0x13,0x11,0x11,0x11}; return g; }
        case 'O': { static const uint8_t g[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return g; }
        case 'P': { static const uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return g; }
        case 'Q': { static const uint8_t g[7]={0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; return g; }
        case 'R': { static const uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return g; }
        case 'S': { static const uint8_t g[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; return g; }
        case 'T': { static const uint8_t g[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return g; }
        case 'U': { static const uint8_t g[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return g; }
        case 'V': { static const uint8_t g[7]={0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; return g; }
        case 'W': { static const uint8_t g[7]={0x11,0x11,0x11,0x15,0x15,0x1B,0x11}; return g; }
        case 'X': { static const uint8_t g[7]={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; return g; }
        case 'Y': { static const uint8_t g[7]={0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; return g; }
        case 'Z': { static const uint8_t g[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; return g; }
        case '0': { static const uint8_t g[7]={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; return g; }
        case '1': { static const uint8_t g[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; return g; }
        case '2': { static const uint8_t g[7]={0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}; return g; }
        case '3': { static const uint8_t g[7]={0x1F,0x02,0x04,0x06,0x01,0x11,0x0E}; return g; }
        case '4': { static const uint8_t g[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; return g; }
        case '5': { static const uint8_t g[7]={0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}; return g; }
        case '6': { static const uint8_t g[7]={0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}; return g; }
        case '7': { static const uint8_t g[7]={0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; return g; }
        case '8': { static const uint8_t g[7]={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; return g; }
        case '9': { static const uint8_t g[7]={0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}; return g; }
        case '.': { static const uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}; return g; }
        case ':': { static const uint8_t g[7]={0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}; return g; }
        case '-': { static const uint8_t g[7]={0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; return g; }
        case '+': { static const uint8_t g[7]={0x00,0x04,0x04,0x1F,0x04,0x04,0x00}; return g; }
        case '%': { static const uint8_t g[7]={0x19,0x19,0x02,0x04,0x08,0x13,0x13}; return g; }
        case '/': { static const uint8_t g[7]={0x01,0x02,0x02,0x04,0x08,0x08,0x10}; return g; }
        case '<': { static const uint8_t g[7]={0x02,0x04,0x08,0x10,0x08,0x04,0x02}; return g; }
        case '>': { static const uint8_t g[7]={0x08,0x04,0x02,0x01,0x02,0x04,0x08}; return g; }
        case '(': { static const uint8_t g[7]={0x02,0x04,0x08,0x08,0x08,0x04,0x02}; return g; }
        case ')': { static const uint8_t g[7]={0x08,0x04,0x02,0x02,0x02,0x04,0x08}; return g; }
        case '#': { static const uint8_t g[7]={0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00}; return g; }
        case '!': { static const uint8_t g[7]={0x04,0x04,0x04,0x04,0x04,0x00,0x04}; return g; }
        case '&': { static const uint8_t g[7]={0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}; return g; }
        case '\'': { static const uint8_t g[7]={0x04,0x04,0x08,0x00,0x00,0x00,0x00}; return g; }
        case ',': { static const uint8_t g[7]={0x00,0x00,0x00,0x00,0x0C,0x04,0x08}; return g; }
        case '=': { static const uint8_t g[7]={0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}; return g; }
        case '[': { static const uint8_t g[7]={0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; return g; }
        case ']': { static const uint8_t g[7]={0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; return g; }
        case '_': { static const uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x00,0x1F}; return g; }
        case '*': { static const uint8_t g[7]={0x00,0x04,0x15,0x0E,0x15,0x04,0x00}; return g; }
        case '?': { static const uint8_t g[7]={0x0E,0x11,0x01,0x02,0x04,0x00,0x04}; return g; }
        case '^': { static const uint8_t g[7]={0x04,0x0A,0x11,0x00,0x00,0x00,0x00}; return g; }
        default: return kSpace;
    }
}

inline int measurePixelText(const char* text, int scale = 2, int tracking = 1) {
    if (!text) return 0;
    int len = static_cast<int>(std::strlen(text));
    if (len == 0) return 0;
    return len * (5 * scale) + (len - 1) * (tracking * scale);
}

inline void drawPixelText(PixelCanvas& c, int x, int y, const char* text, uint32_t color, int scale = 2, int tracking = 1) {
    if (!text) return;
    int curX = x;
    for (const char* p = text; *p; ++p) {
        const uint8_t* g = getGlyph5x7(*p);
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = g[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    c.fillRect(curX + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        curX += (5 + tracking) * scale;
    }
}

inline void drawPixelTextCentered(PixelCanvas& c, int centerX, int y, const char* text, uint32_t color, int scale = 2, int tracking = 1) {
    int w = measurePixelText(text, scale, tracking);
    drawPixelText(c, centerX - w / 2, y, text, color, scale, tracking);
}

} // namespace PixelLabs
