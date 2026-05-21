#ifndef M32POCKETLGFX_H_
#define M32POCKETLGFX_H_

/******************************************************************************************************************************
 *  M32PocketLGFX — direct LovyanGFX driver for the M32 Pocket TFT (ST7789)
 *
 *  Replaces the DisplayWrapper library for the TFT path. The class inherits
 *  from lgfx::LGFX_Device so all standard LovyanGFX methods (drawString,
 *  fillRect, setRotation, sprites, ...) are available unchanged, AND adds
 *  the few DisplayWrapper-compatible helpers that MorseOutput already calls
 *  (setColor enum, setTheme, setFont index, clear, drawXbm, etc.) so we do
 *  not have to rewrite every call site.
 *
 *  Pin/timing configuration is taken from the TFT_* build flags defined in
 *  platformio.ini (M32_Pocket_TFT_build_flags). Originally vendored from
 *  the DisplayWrapper repo's ST7789.h.
 *
 *  Fonts: the M32 Pocket UI uses IntelOneMono in 12 pt/15 pt regular/bold.
 *  The DialogInput_* names below are kept for historical compatibility with
 *  MorseOutput.cpp; they evaluate to the IntelOneMono font pointers (which
 *  is what the TFT path was already using under the hood — DisplayWrapper
 *  mapped index 0..3 to IntelOneMono on the TFT, DialogInput on the OLED).
 *****************************************************************************************************************************/

#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "IntelOneMono_Regular12pt8b.h"
#include "IntelOneMono_Bold12pt8b.h"
#include "IntelOneMono_Regular15pt8b.h"
#include "IntelOneMono_Bold15pt8b.h"

// ---- DisplayWrapper-compatible enums (so callers compile unchanged) ----
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

// ---- Font index macros (historical names; resolve to real font pointers) ----
#define DialogInput_plain_12 (&IntelOneMono_Regular12pt8b)
#define DialogInput_bold_12  (&IntelOneMono_Bold12pt8b)
#define DialogInput_plain_15 (&IntelOneMono_Regular15pt8b)
#define DialogInput_bold_15  (&IntelOneMono_Bold15pt8b)

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

    // DW-compat state
    bool      _useTheme = false;
    uint16_t  _themeFg  = TFT_WHITE;
    uint16_t  _themeBg  = TFT_BLACK;
    const lgfx::GFXfont *_currentFont = nullptr;

public:
    LGFX() {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 40000000;
            cfg.spi_3wire   = true;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = TFT_SCLK;
            cfg.pin_mosi    = TFT_MOSI;
            cfg.pin_miso    = TFT_MISO;
            cfg.pin_dc      = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = TFT_CS;
            cfg.pin_rst          = TFT_RST;
            cfg.pin_busy         = -1;
            cfg.panel_width      = TFT_WIDTH;
            cfg.panel_height     = TFT_HEIGHT;
#ifdef TFT_OFFSET_X
            cfg.offset_x         = TFT_OFFSET_X;
#else
            cfg.offset_x         = 0;
#endif
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = TFT_BL;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }

    // ---- DisplayWrapper-compatible convenience methods ----

    bool init() {
#ifdef PIN_VTFT_CTRL
        pinMode(PIN_VTFT_CTRL, OUTPUT);
        digitalWrite(PIN_VTFT_CTRL, LOW);
#endif
        begin();
        setRotation(3);
        return true;
    }

    void display() {}                        // LovyanGFX auto-flushes; no-op
    void setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT) {}  // historical no-op

    void displayOff() {
        setBrightness(0);
        sleep();
    }

    void flipScreenVertically() { setRotation(1); }

    void setTheme(uint16_t fg, uint16_t bg) {
        _useTheme = true;
        _themeFg  = fg;
        _themeBg  = bg;
    }

    void clear() {
        fillScreen(_useTheme ? _themeBg : TFT_BLACK);
    }

    void setColor(OLEDDISPLAY_COLOR color) {
        if (!_useTheme) {
            if (color == WHITE) {
                lgfx::LGFX_Device::setColor(TFT_WHITE);
                setTextColor(TFT_WHITE, TFT_BLACK);
            } else {
                lgfx::LGFX_Device::setColor(TFT_BLACK);
                setTextColor(TFT_BLACK, TFT_WHITE);
            }
        } else {
            if (color == WHITE) {
                lgfx::LGFX_Device::setColor(_themeFg);
                setTextColor(_themeFg, _themeBg);
            } else {
                lgfx::LGFX_Device::setColor(_themeBg);
                setTextColor(_themeBg, _themeFg);
            }
        }
    }

    // DisplayWrapper had setFont(int) — but the firmware actually passes the
    // font *pointer* (via the DialogInput_* macros above), so only the
    // pointer overload is needed. LGFX_Device already provides
    // setFont(const lgfx::GFXfont*).
    void setFont(const lgfx::GFXfont *fontData) {
        _currentFont = fontData;
        lgfx::LGFX_Device::setFont(fontData);
    }

    // DisplayWrapper signature: drawXbm(x,y,w,h,xbm). LovyanGFX:
    // drawXBitmap(x,y,bitmap,w,h,fg,bg). Adapter keeps callers unchanged.
    void drawXbm(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *xbm) {
        drawXBitmap(x, y, xbm, w, h,
                    _useTheme ? _themeFg : TFT_WHITE,
                    _useTheme ? _themeBg : TFT_BLACK);
    }

    void drawHorizontalLine(int16_t x, int16_t y, int16_t length) {
        drawFastHLine(x, y, length);
    }
    void drawVerticalLine(int16_t x, int16_t y, int16_t length) {
        drawFastVLine(x, y, length);
    }

    // Size helpers — DisplayWrapper returned uint8_t; LGFX returns int.
    uint16_t getWidth()  { return width();  }
    uint16_t getHeight() { return height(); }
    uint16_t getStringWidth(const String& s) { return textWidth(s.c_str()); }
    uint16_t getStringHeight(const String&)  { return fontHeight(_currentFont); }
    uint16_t drawString(int16_t x, int16_t y, const String& s) {
        setTextDatum(lgfx::top_left);
        lgfx::LGFX_Device::drawString(s.c_str(), x, y);
        return textWidth(s.c_str());
    }
};

// The global display instance lives in MorseOutput.cpp; consumers (games,
// MorseGameMode) include this header to get the LGFX type and the extern.
extern LGFX display;

#endif  // M32POCKETLGFX_H_
