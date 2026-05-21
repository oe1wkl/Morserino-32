#ifndef M32OLEDLGFX_H_
#define M32OLEDLGFX_H_

/******************************************************************************************************************************
 *  M32OledLGFX — direct LovyanGFX driver for the legacy Morserino-32 OLED (SSD1306, I2C, 128×64)
 *
 *  Stage C of the DisplayWrapper retirement: replaces ThingPulse's
 *  SSD1306Wire library with LovyanGFX's Panel_SSD1306 on the legacy
 *  Heltec V2 / V3 / ESP32-S3-Devkit / minipcb_lora boards. Mirrors
 *  M32PocketLGFX.h on the TFT side — both inherit from lgfx::LGFX_Device
 *  so the rest of MorseOutput.cpp uses the same API.
 *
 *  Font strategy
 *  -------------
 *  The OLED keeps using the *existing* wkl-format DialogInput byte-array
 *  fonts (wklfonts.h / wklfonts.cpp), unchanged. squix.ch's Adafruit-GFX
 *  output is hard-capped at ASCII printable (0x20-0x7E) and offers no UI
 *  to extend the range — so a straight GFXfont swap would silently lose
 *  the German umlauts (ä, ö, ü, ß and uppercase), the copyright sign ©,
 *  and other Latin-1 chars the SSD1306Wire version handled.
 *
 *  Instead, this class implements a small wkl-format glyph renderer that
 *  reads the existing byte-array fonts and paints pixels via LovyanGFX
 *  primitives, with an inline UTF-8 → Latin-1 decode so the firmware's
 *  UTF-8 source strings still resolve to the correct codepoints (e.g.
 *  the "\xc2\xa9" in morsedefs.h's COPYRIGHT string lands on 0xA9).
 *
 *  wkl font format (per glyph headers in wklfonts.h):
 *    byte 0:  max glyph width
 *    byte 1:  glyph height (pixels)
 *    byte 2:  first codepoint (e.g. 0x20)
 *    byte 3:  number of codepoints (e.g. 0xE0 = 224 → covers up to 0xFF)
 *    jump table at offset 4, 4 bytes / codepoint:
 *      byte 0: jumpAddr MSB    (0xFFFF marker = no bitmap, advance only)
 *      byte 1: jumpAddr LSB
 *      byte 2: glyph size in bytes (0 for the marker case)
 *      byte 3: glyph pixel width  (the inter-character advance)
 *    bitmap data at offset 4 + numChars*4. Per glyph, columns are stored
 *    left-to-right; within a column, (height+7)/8 bytes encode 8 vertical
 *    pixels each, LSB = top pixel.
 *
 *  Pin/timing comes from OLED_SDA / OLED_SCL / OLED_RST build flags.
 *****************************************************************************************************************************/

#include <Arduino.h>
#include <LovyanGFX.hpp>

// ---- DisplayWrapper-/OLEDDisplay-compatible enums (so callers compile unchanged) ----
enum OLEDDISPLAY_TEXT_ALIGNMENT {
    TEXT_ALIGN_LEFT        = 0,
    TEXT_ALIGN_RIGHT       = 1,
    TEXT_ALIGN_CENTER      = 2,
    TEXT_ALIGN_CENTER_BOTH = 3
};

enum OLEDDISPLAY_COLOR {
    BLACK = 0, WHITE = 1, INVERSE = 2,
    RED   = 3, GREEN = 4, BLUE    = 5
};

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_SSD1306 _panel_instance;
    lgfx::Bus_I2C       _bus_instance;

    // wkl font + current colours (LovyanGFX's own setTextColor state is
    // also kept in sync so any built-in GFXfont path still works).
    //
    // NOTE: colours are stored as uint16_t (RGB565). If passed as int /
    // uint32_t to drawPixel(), LovyanGFX's color_conv_t treats the value
    // as RGB888 (24-bit) — TFT_WHITE (0xFFFF) would then be interpreted
    // as cyan (R=0, G=0xFF, B=0xFF), to_gray() ~= 174, and the 1-bit
    // OLED's Bayer dithering produces a stuck checkerboard pattern. The
    // explicit uint16_t type makes the converter pick the RGB565 path.
    const uint8_t *_wklFont   = nullptr;
    uint16_t       _fgColor   = TFT_WHITE;
    uint16_t       _bgColor   = TFT_BLACK;

    // Decode a single Latin-1 codepoint from a UTF-8 string. Returns the
    // codepoint as 0..255 and advances `p`; non-Latin-1 codepoints
    // (3- or 4-byte sequences) are silently skipped. Returns -1 at EOS.
    static int nextLatin1(const char *&p) {
        while (true) {
            uint8_t c = (uint8_t)*p;
            if (c == 0) return -1;
            ++p;
            if (c < 0x80) return c;                       // ASCII
            if ((c & 0xE0) == 0xC0) {                     // 2-byte UTF-8
                uint8_t c2 = (uint8_t)*p;
                if ((c2 & 0xC0) != 0x80) return c;        // malformed → raw byte
                ++p;
                uint16_t cp = ((c & 0x1F) << 6) | (c2 & 0x3F);
                return (cp < 256) ? (int)cp : 0;          // outside Latin-1 → skip
            }
            if ((c & 0xF0) == 0xE0) {                     // 3-byte → skip
                if (*p) ++p; if (*p) ++p;
                continue;
            }
            if ((c & 0xF8) == 0xF0) {                     // 4-byte → skip
                if (*p) ++p; if (*p) ++p; if (*p) ++p;
                continue;
            }
            return c;                                     // lone continuation → raw byte
        }
    }

public:
    LGFX() {
        {
            auto cfg = _bus_instance.config();
            cfg.i2c_port    = 0;
            cfg.freq_write  = 700000;
            cfg.freq_read   = 400000;
            cfg.pin_sda     = OLED_SDA;
            cfg.pin_scl     = OLED_SCL;
            cfg.i2c_addr    = 0x3c;
            cfg.prefix_cmd  = 0x00;
            cfg.prefix_data = 0x40;
            cfg.prefix_len  = 1;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = -1;
#ifdef OLED_RST
            cfg.pin_rst          = OLED_RST;
#else
            cfg.pin_rst          = -1;
#endif
            cfg.pin_busy         = -1;
            cfg.panel_width      = 128;
            cfg.panel_height     = 64;
            cfg.memory_width     = 128;
            cfg.memory_height    = 64;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.readable         = false;
            cfg.invert           = false;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }

    // ---- DisplayWrapper-/OLEDDisplay-compatible convenience methods ----

    bool init() {
        begin();
        setRotation(0);
        setColorDepth(lgfx::palette_1bit);
        return true;
    }

    void display() {}                        // LovyanGFX auto-flushes
    void setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT) {}

    void displayOff() {
        setBrightness(0);
        sleep();
    }

    void flipScreenVertically() { setRotation(2); }

    void clear() { fillScreen((uint16_t)TFT_BLACK); }

    void setColor(OLEDDISPLAY_COLOR color) {
        if (color == WHITE) { _fgColor = TFT_WHITE; _bgColor = TFT_BLACK; }
        else                { _fgColor = TFT_BLACK; _bgColor = TFT_WHITE; }
        // Pass as uint16_t so the RGB565 conversion path is selected
        // (see note on _fgColor type above).
        lgfx::LGFX_Device::setColor((uint16_t)_fgColor);
        setTextColor((uint16_t)_fgColor, (uint16_t)_bgColor);
    }

    // wkl-format font (the firmware's existing wklfonts.h byte arrays).
    // Stored verbatim — drawString below reads and renders it directly.
    void setFont(const uint8_t *wklFont) { _wklFont = wklFont; }

    // GFXfont path (kept for any consumer that still wants LovyanGFX's
    // built-in fonts; not used by MorseOutput on the OLED path today).
    void setFont(const lgfx::GFXfont *fontData) {
        _wklFont = nullptr;
        lgfx::LGFX_Device::setFont(fontData);
    }

    void drawXbm(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *xbm) {
        drawXBitmap(x, y, xbm, w, h, (uint16_t)TFT_WHITE, (uint16_t)TFT_BLACK);
    }

    void drawHorizontalLine(int16_t x, int16_t y, int16_t length) { drawFastHLine(x, y, length); }
    void drawVerticalLine  (int16_t x, int16_t y, int16_t length) { drawFastVLine(x, y, length); }

    uint16_t getWidth()  { return width();  }
    uint16_t getHeight() { return height(); }

    uint16_t getStringWidth(const String& s) {
        if (!_wklFont) return textWidth(s.c_str());
        const uint8_t *f = _wklFont;
        uint8_t firstChar = pgm_read_byte(f + 2);
        uint8_t numChars  = pgm_read_byte(f + 3);
        uint16_t w = 0;
        const char *p = s.c_str();
        int cp;
        while ((cp = nextLatin1(p)) >= 0) {
            if (cp == 0) continue;                   // skipped (out of Latin-1)
            if (cp < firstChar || cp >= firstChar + numChars) continue;
            int idx = cp - firstChar;
            w += pgm_read_byte(f + 4 + idx * 4 + 3); // 4-byte JT entry, byte 3 = glyph width
        }
        return w;
    }

    uint16_t getStringHeight(const String&) {
        if (!_wklFont) return fontHeight();
        return pgm_read_byte(_wklFont + 1);
    }

    uint16_t drawString(int16_t x, int16_t y, const String& s) {
        if (!_wklFont) {
            setTextDatum(lgfx::top_left);
            lgfx::LGFX_Device::drawString(s.c_str(), x, y);
            return textWidth(s.c_str());
        }
        const uint8_t *f = _wklFont;
        uint8_t height      = pgm_read_byte(f + 1);
        uint8_t firstChar   = pgm_read_byte(f + 2);
        uint8_t numChars    = pgm_read_byte(f + 3);
        uint8_t bytesPerCol = (height + 7) >> 3;
        const uint8_t *bitmapStart = f + 4 + (uint16_t)numChars * 4;

        startWrite();
        int penX = x;
        const char *p = s.c_str();
        int cp;
        while ((cp = nextLatin1(p)) >= 0) {
            if (cp == 0) continue;
            if (cp < firstChar || cp >= firstChar + numChars) continue;
            int idx = cp - firstChar;
            const uint8_t *jt = f + 4 + idx * 4;
            uint16_t jumpAddr = ((uint16_t)pgm_read_byte(jt)     << 8)
                              |  (uint16_t)pgm_read_byte(jt + 1);
            uint8_t  glyphSize  = pgm_read_byte(jt + 2);
            uint8_t  charWidth  = pgm_read_byte(jt + 3);

            if (jumpAddr != 0xFFFF && glyphSize != 0) {
                // Iterate the actual byte stream (same shape as
                // SSD1306Wire's drawInternal): each byte covers 8 vertical
                // pixels in column-page order. Crucially, the LAST column
                // of a glyph may have fewer than `bytesPerCol` bytes — the
                // wkl/squix encoder drops trailing all-zero pages — so we
                // must drive everything off the byte count rather than
                // assuming a fixed cols × pages grid.
                const uint8_t *glyph = bitmapStart + jumpAddr;
                for (uint8_t i = 0; i < glyphSize; ++i) {
                    uint8_t b = pgm_read_byte(glyph + i);
                    if (!b) continue;
                    uint8_t col  = i / bytesPerCol;
                    uint8_t page = i % bytesPerCol;
                    int  baseRow = page << 3;
                    for (uint8_t bit = 0; bit < 8; ++bit) {
                        if (b & (1 << bit)) {
                            int row = baseRow + bit;
                            if (row < height)
                                drawPixel(penX + col, y + row, _fgColor);
                        }
                    }
                }
            }
            penX += charWidth;
        }
        endWrite();
        return penX - x;
    }
};

// The global display instance is defined in MorseOutput.cpp.
extern LGFX display;

#endif  // M32OLEDLGFX_H_
