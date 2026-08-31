/******************************************************************************************************************************
    morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module **
    Copyright (C) 2018-2025 ff.  Willi Kraml, OE1WKL                                                                        **

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this program.
    If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

/// This module contains functions for output on the display, on the USB serial output, on the speaker and on line out
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////// New scrolling display


#ifndef CONFIG_TFT
/// circular buffer: 14 chars by NoOfLines lines (bottom NoOfVisibleLines are visible)
#define NoOfCharsPerLine 14
#define LINE_HEIGHT 16
#define DESCENDER_LENGTH 0
#define C_WIDTH 9
#else
/// circular buffer: LCD uses 4 visible lines; 24 chars/line is well above
/// the longest static printOnScroll() text (~14 chars at the largest fonts).
/// Earlier value 512 reserved ~17 KB persistent RAM with no callers writing
/// anywhere near that much; longer dynamic strings are still gracefully
/// wrapped by the existing screenPos+l > NoOfCharsPerLine check.
#define NoOfCharsPerLine 24
#define LINE_HEIGHT (display.getStringHeight("j"))
#define C_WIDTH display.getStringWidth("A")
#endif

#include <Arduino.h>
#include "MorseOutput.h"
#include "morsedefs.h"
#include "MorsePreferences.h"

#define SCROLL_TOP scrollTop

#ifndef CONFIG_TFT
#include "wklfonts.h"            // legacy DialogInput byte-array fonts
#include "M32OledLGFX.h"
LGFX display;
#else
// TFT path — using DisplayWrapper (the V8.0 path). The in-tree LGFX
// wrapper introduced in #157 left the TFT dark on USB-only-powered
// Pockets (root cause not yet pinned down); the DisplayWrapper library
// path is the only one that keeps the panel alive.
#include "DisplayWrapper.h"
#include "m32logo_aa.h"          // pre-rendered anti-aliased boot-splash logo (white-on-black)
#ifdef CONFIG_SCROLL_FONT_SIZE
#include "IntelOneMono12ptAscii.h"   // ASCII-range small scroll font (see that file for why)
#endif
DisplayWrapper display;
#endif

/// Measure a string in the *currently selected* font, in pixels.
///
/// Always use this instead of display.getStringWidth() for a string whose length
/// is not a fixed one or two characters: DisplayWrapper::getStringWidth() is
/// declared uint8_t but returns LovyanGFX's int32_t textWidth(), so it wraps
/// modulo 256. The wrapper maps DialogInput_* to IntelOneMono 12pt/15pt on the
/// TFT - 15 px and 18 px per character, both monospace - so the wrap starts at
/// 18 characters in the status-line font and at 15 in the scroll-area font, well
/// inside what a 320 px line holds. An 18-character status line measured 270 px
/// and reported 14. Measuring through getLGFX() keeps the full width; the wrapper
/// hands out the same lcd object it draws with, so the font state is shared and
/// this needs no setFont() of its own.
///
/// The OLED build has no such defect (M32OledLGFX::getStringWidth returns
/// uint16_t, and 128 px never overflows anyway), so it just forwards.
static inline uint16_t stringWidth(const String& s) {
#ifdef CONFIG_TFT
  return (uint16_t) DisplayWrapper::getLGFX()->textWidth(s.c_str());
#else
  return display.getStringWidth(s);
#endif
}

/// Select the font for the scroll area's *default* size (i.e. printOnScroll()
/// callers that don't pass small=true): the Font Size preference's Normal/
/// Small choice on the M32 Pocket (non-Accessibility), or always the normal
/// 15pt font everywhere else. Small uses the ASCII-range
/// IntelOneMono12ptAscii.h variant rather than the IntelOneMono 12pt used for
/// a caller-forced small=true (e.g. a narrow IP-address line) - see that
/// header for why. forceNormal overrides the preference (used by the boot
/// splash, whose (c) glyph falls outside the small ASCII font's declared
/// range). Everywhere else (OLED, and the Accessibility Edition) this is
/// always the normal (large) font; posScrollFont doesn't exist in those
/// builds. Shared by setDefaultScrollFont() and printOnScroll() so the two
/// font-selection rules can't drift apart.
static inline void scrollFont(boolean bold, boolean forceNormal) {
#ifdef CONFIG_SCROLL_FONT_SIZE
  if (forceNormal || !MorsePreferences::pliste[posScrollFont].value)
    display.setFont(bold ? DialogInput_bold_15 : DialogInput_plain_15);
  else
    display.setFont(bold ? &IntelOneMono_Bold12pt8b_Ascii : &IntelOneMono_Regular12pt8b_Ascii);
#else
  display.setFont(bold ? DialogInput_bold_15 : DialogInput_plain_15);
#endif
}

/// Plain-weight, non-forced-small shorthand for scrollFont(). printOnScroll()
/// may leave a different font active (bold, or a caller-forced small size) -
/// callers that need to measure against the *default* scroll font (the
/// wrap-width checks in printToScroll_internal() and wordNeedsWrap()) must
/// call this first rather than relying on whatever font happens to be active.
static inline void setDefaultScrollFont() {
  scrollFont(false, false);
}

#ifdef CONFIG_SCROLL_FONT_SIZE
uint8_t NoOfVisibleLines = NoOfVisibleLinesNormal;
#endif

// Row-to-row Y step used in place of LINE_HEIGHT wherever a scroll row's
// position (not its glyph/clear-rect size, which stays LINE_HEIGHT) is
// computed, plus a matching top margin (scrollTopPad) added once before the
// first row - together they spread NoOfVisibleLines rows evenly across the
// whole available height, split into NoOfVisibleLines+1 equal gaps (before
// the first row, between each pair, after the last), instead of packing
// tight at the honest per-glyph pitch and leaving every unused pixel as one
// lump below the last row (or, with only the inter-row step enlarged and no
// top margin, still visibly hugging the top). Left untouched (both fall back
// to plain SCROLL_TOP/LINE_HEIGHT via SCROLL_ROW_TOP/LINE_STEP below)
// everywhere NoOfVisibleLines is a fixed compile-time constant (OLED, and
// TFT without the Font Size preference) - there the existing layout is
// already tuned to fill the area with nothing left over. Recomputed only in
// applyScrollFontGeometry(), where NoOfVisibleLines itself is (re)computed,
// not on every draw - LINE_HEIGHT is a live macro that reads whatever font
// happens to be active *right now*, so reading it there (right after
// explicitly selecting the default scroll font) is the one place guaranteed
// to reflect the font this spacing is meant for.
static uint8_t scrollLineStep = 0;
static uint8_t scrollTopPad = 0;

#ifdef CONFIG_SCROLL_FONT_SIZE
#define LINE_STEP scrollLineStep
#define SCROLL_ROW_TOP (SCROLL_TOP + scrollTopPad)
#else
#define LINE_STEP LINE_HEIGHT
#define SCROLL_ROW_TOP SCROLL_TOP
#endif

#ifdef CONFIG_SOUND_I2S
#include "I2S_Sidetone.hpp"
I2S_Sidetone sidetone;
#endif

#ifdef CONFIG_WM8960
#include <SparkFun_WM8960_Arduino_Library.h>
WM8960 codec;
#endif

#ifdef CONFIG_TLV320AIC3100
#include "tlv320aic31xx_codec.h"
TLV320AIC31xx codec(&Wire);
#endif

using namespace MorseOutput;

extern uint16_t volt;
extern double voltage_raw;

char textBuffer[NoOfLines][2 * NoOfCharsPerLine + 1]; /// we need extra room for style markers (FONT_ATTRIB stored as characters to toggle on/off the style within a line)
/// and 0 terminator


uint8_t linePointer = 0;    /// defines the current bottom line
uint8_t bottomLine = 0;
static uint8_t scrollScreenPos = 0;   /// current column on the scroll line; used by printToScroll_internal() and by wordNeedsWrap()

int8_t MorseOutput::maxPos = NoOfLines - NoOfVisibleLines;
int8_t MorseOutput::relPos = MorseOutput::maxPos;

#ifndef CONFIG_TFT

#define lora_width 6        /// a simple logo that shows when we operate with loRa, stored in XBM format
#define lora_height 11
static unsigned char lora_bits[] = {
  0x0f, 0x18, 0x33, 0x24, 0x29, 0x2b, 0x29, 0x24, 0x33, 0x18, 0x0f
};

#define wifi_width 6        /// a simple logo that shows when we operate with WiFi, stored in XBM format
#define wifi_height 11
static unsigned char wifi_bits[] = {
  0xc0,0xc8,0xd0,0xe4,0xe8,0xeb,0xe8,0xe4,0xd0,0xc8,0xc0 };

#define ble_width 6         /// shown while a BLE Serial client holds a session, stored in XBM format
#define ble_height 11
// The Bluetooth rune, reduced to stem + the two right-hand triangles. The real
// rune's crossing diagonals need a left arm that degenerates into two isolated
// pixels at six columns wide, which reads as dirt rather than as a symbol.
static unsigned char ble_bits[] = {
  0x04,0x0c,0x0c,0x14,0x0c,0x04,0x0c,0x14,0x0c,0x0c,0x04 };

#else

//fg = 0x6345;
//bg = 0xfff3;


#define lora_width 13        /// a simple logo that shows when we operate with loRa, stored in XBM format
#define lora_height 24
static unsigned char lora_bits[] = {
  0x88,0xe0,0x11,0xe1,0x22,0xe2,0x44,0xe2,0x44,0xe4,0x8b,0xe4,
 0x93,0xe8,0x13,0xe9,0x27,0xe9,0x27,0xf2,0x4b,0xf2,0x5f,0xf2,
 0x5f,0xf2,0x44,0xf2,0x24,0xf2,0x22,0xe9,0x11,0xe9,0x90,0xe8,
 0x88,0xe4,0x44,0xe4,0x44,0xe2,0x22,0xe2,0x11,0xe1,0x88,0xe0
};

#define wifi_width 13        /// a simple logo that shows when we operate with WiFi, stored in XBM format
#define wifi_height 24
static unsigned char wifi_bits[] = {
  0x00,0xe0,0x00,0xe0,0x00,0xe1,0x00,0xe2,0x40,0xe4,0x80,0xe4,
 0x90,0xe8,0x10,0xe9,0x24,0xe9,0x24,0xf2,0x48,0xf2,0x4b,0xf2,
 0x48,0xf2,0x24,0xf2,0x24,0xf2,0x10,0xe9,0x10,0xe9,0x80,0xe8,
 0x80,0xe4,0x40,0xe4,0x00,0xe2,0x00,0xe1,0x00,0xe0,0x00,0xe0
};

#define ble_width 13         /// shown while a BLE Serial client holds a session, stored in XBM format
#define ble_height 24
// The full Bluetooth rune: stem, plus two strokes that each run from a stem end
// out to the right and then diagonally across to the opposite side. There is
// room for the crossings at this size, unlike on the OLED.
static unsigned char ble_bits[] = {
  0x40,0x00,0xc0,0x00,0xc0,0x00,0x40,0x01,0x40,0x02,0x40,0x02,
 0x44,0x04,0x48,0x02,0x48,0x02,0x50,0x01,0xe0,0x00,0x40,0x00,
 0x40,0x00,0xe0,0x00,0x50,0x01,0x48,0x02,0x48,0x02,0x44,0x04,
 0x40,0x02,0x40,0x02,0x40,0x01,0xc0,0x00,0xc0,0x00,0x40,0x00
};

/// logo for start-up screen

#define M32c_width 318
#define M32c_height 136
static unsigned char M32c_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
  0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xf0, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
  0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x07, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xe0, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x0f, 0x07,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xfc, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff,
  0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xfc, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff,
  0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0x07, 0xc0,
  0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
  0xff, 0xff, 0x03, 0x00, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0xc7, 0xff, 0xff, 0x00, 0x00, 0xfc, 0x0f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xcf, 0xff, 0xff, 0x00, 0x00,
  0xfc, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xcf,
  0xff, 0xff, 0x03, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xe0, 0xff, 0xff, 0xcf, 0xff, 0xff, 0x07, 0x00, 0xf8, 0x1f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x7f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xcf, 0xff, 0xff, 0x0f, 0x00,
  0xf8, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe,
  0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xcf,
  0xff, 0xff, 0x1f, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xf0, 0xff, 0xff, 0xcf, 0xff, 0xff, 0x1f, 0x00, 0xf8, 0x1f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x9f, 0x83, 0xff, 0x3f, 0x00,
  0xfc, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x1f,
  0x00, 0xfc, 0x3f, 0x00, 0xfc, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfc, 0xff, 0xff, 0x1f, 0x00, 0xf0, 0x3f, 0x00, 0xfe, 0x0f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x1f, 0x00, 0xf0, 0x3f, 0x00,
  0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x1f,
  0x00, 0xe0, 0x7f, 0x00, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfe, 0xff, 0xff, 0x3f, 0x00, 0xe0, 0x7f, 0x80, 0xff, 0x03, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x07, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x3f, 0x00, 0xe0, 0x3f, 0xc0,
  0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x3f,
  0x00, 0xf0, 0x3f, 0xf0, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0x3f, 0x00, 0xf8, 0x3f, 0xf8, 0x7f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x3f, 0x00, 0xfc, 0x3f, 0xfc,
  0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff,
  0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  0x80, 0xff, 0x1f, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0xc0,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0x1f, 0x00,
  0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff,
  0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xe0,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x7f, 0x00,
  0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe1, 0xff,
  0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff,
  0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x7f, 0xe0, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf0,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xe0, 0xff, 0xff, 0xff, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x00,
  0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xe0, 0xff,
  0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff,
  0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff,
  0x3f, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0xfc,
  0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x01,
  0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff,
  0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0xfe,
  0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x07,
  0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff,
  0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x0f,
  0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0xc0, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f,
  0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
  0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0xf0, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xc0, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0x01, 0x00, 0xf8, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff,
  0x01, 0x00, 0xfc, 0xff, 0xff, 0xf3, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff,
  0x1f, 0xff, 0xff, 0xff, 0x03, 0x00, 0xfc, 0xff, 0xff, 0xf3, 0xff, 0xff,
  0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xc0, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x03, 0x00, 0xfe, 0xff,
  0xff, 0xf1, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0x1f, 0xfe, 0xff, 0xff,
  0x07, 0x00, 0xfe, 0xff, 0xff, 0xf1, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff,
  0x1f, 0xfe, 0xff, 0xff, 0x07, 0x00, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff,
  0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xe0, 0xff, 0xff, 0x0f, 0xfc, 0xff, 0xff, 0x07, 0x00, 0xff, 0xff,
  0xff, 0xe0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x0f, 0xfc, 0xff, 0xff,
  0x0f, 0x80, 0xff, 0xff, 0x7f, 0xe0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff,
  0x0f, 0xfc, 0xff, 0xff, 0x0f, 0x80, 0xff, 0xff, 0x7f, 0xe0, 0xff, 0xff,
  0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf0, 0xff, 0xff, 0x07, 0xf8, 0xff, 0xff, 0x1f, 0x80, 0xff, 0xff,
  0x3f, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x07, 0xf8, 0xff, 0xff,
  0x1f, 0xc0, 0xff, 0xff, 0x3f, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff,
  0x07, 0xf0, 0xff, 0xff, 0x3f, 0xc0, 0xff, 0xff, 0x3f, 0xc0, 0xff, 0xff,
  0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf0, 0xff, 0xff, 0x07, 0xf0, 0xff, 0xff, 0x3f, 0xe0, 0xff, 0xff,
  0x1f, 0xc0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff,
  0x7f, 0xe0, 0xff, 0xff, 0x1f, 0xc0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff,
  0x03, 0xe0, 0xff, 0xff, 0x7f, 0xf0, 0xff, 0xff, 0x0f, 0xc0, 0xff, 0xff,
  0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf8, 0xff, 0xff, 0x03, 0xc0, 0xff, 0xff, 0x7f, 0xf0, 0xff, 0xff,
  0x0f, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x03, 0xc0, 0xff, 0xff,
  0xff, 0xf8, 0xff, 0xff, 0x07, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff,
  0x03, 0x80, 0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0x07, 0x80, 0xff, 0xff,
  0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf8, 0xff, 0xff, 0x01, 0x80, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff,
  0x03, 0x80, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x01, 0x80, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x03, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff,
  0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x80, 0xff, 0xff,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xfc, 0xff, 0xff, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x01, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x01, 0x00, 0xfe, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff,
  0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xfe, 0xff, 0xff, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x00, 0x00, 0xfc, 0xff,
  0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff,
  0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0xff, 0xff,
  0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xfe, 0xff, 0x7f, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0x3f,
  0x00, 0x00, 0xfe, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x00, 0xf8, 0xff,
  0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f,
  0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xfe, 0xff,
  0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0xff, 0x7f, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x1f,
  0x00, 0x00, 0xfe, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0xfe, 0xff, 0xff, 0x03, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x3f,
  0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0xfc, 0xff,
  0xff, 0x03, 0x00, 0xe0, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0xff, 0x0f,
  0x00, 0x00, 0xfc, 0xff, 0xff, 0x03, 0x00, 0xe0, 0xff, 0xff, 0x07, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xc0, 0xff,
  0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x07, 0x00, 0xe0,
  0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x3f,
  0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0x07, 0x00, 0x00, 0xfc, 0xff,
  0xff, 0x07, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01,
  0x80, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0xff, 0x07,
  0x00, 0x00, 0xfc, 0xff, 0xff, 0x07, 0x00, 0xe0, 0xff, 0xff, 0x7f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x01, 0xc0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x80, 0xff,
  0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x07, 0x00, 0xe0,
  0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01, 0xc0, 0xff, 0xff, 0x1f,
  0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0xf8, 0xff,
  0xff, 0x07, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01,
  0xc0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01,
  0x00, 0x00, 0xf8, 0xff, 0xff, 0x0f, 0x00, 0xe0, 0x1f, 0x80, 0xff, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x01, 0xc0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xfe,
  0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x0f, 0x00, 0xe0,
  0x1f, 0x00, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01, 0xc0, 0xff, 0xff, 0x1f,
  0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xf8, 0xff,
  0xff, 0x0f, 0x00, 0xe0, 0x1f, 0x00, 0xfe, 0x01, 0x00, 0x0f, 0x00, 0x00,
  0x80, 0x03, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x00, 0xfc, 0x01,
  0xe0, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x00,
  0x00, 0x00, 0xf8, 0xff, 0xff, 0x0f, 0x00, 0xe0, 0x1f, 0x00, 0xfe, 0x01,
  0xf0, 0xff, 0x00, 0x00, 0xf8, 0x7f, 0x00, 0xfe, 0x00, 0xfe, 0x01, 0xfc,
  0x1f, 0xc0, 0xff, 0x1f, 0xe0, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xfc,
  0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x1f, 0x00, 0xe0,
  0x1f, 0x00, 0xfc, 0x01, 0xfc, 0xff, 0x03, 0x00, 0xfe, 0xff, 0x01, 0xfe,
  0x00, 0xff, 0x01, 0xff, 0x7f, 0xc0, 0xff, 0x1f, 0xe0, 0xff, 0xff, 0x0f,
  0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0xf0, 0xff,
  0xff, 0x1f, 0x00, 0xe0, 0x1f, 0x00, 0xfc, 0x01, 0xfe, 0xff, 0x07, 0x00,
  0xff, 0xff, 0x03, 0xfe, 0x80, 0xff, 0x80, 0xff, 0xff, 0xc0, 0xff, 0x1f,
  0xe0, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x7f, 0x00,
  0x00, 0x00, 0xf0, 0xff, 0xff, 0x1f, 0x00, 0xe0, 0x1f, 0x00, 0xfe, 0x01,
  0xff, 0xff, 0x0f, 0x80, 0xff, 0xff, 0x07, 0xfe, 0xc0, 0x7f, 0xc0, 0xff,
  0xff, 0xc1, 0xff, 0x1f, 0xf0, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xf8,
  0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x1f, 0x00, 0xe0,
  0x1f, 0x00, 0xfe, 0x81, 0xff, 0xff, 0x1f, 0xc0, 0xff, 0xff, 0x07, 0xfe,
  0xe0, 0x3f, 0xe0, 0xff, 0xff, 0xc3, 0xff, 0x1f, 0xf0, 0xff, 0xff, 0x07,
  0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0xf0, 0xff,
  0xff, 0x1f, 0x00, 0xe0, 0x1f, 0x00, 0xfe, 0x81, 0xff, 0xf0, 0x3f, 0xe0,
  0x7f, 0xf8, 0x0f, 0xfe, 0xf0, 0x1f, 0xe0, 0x3f, 0xfc, 0x07, 0xfc, 0x01,
  0xf0, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x3f, 0x00,
  0x00, 0x00, 0xf0, 0xff, 0xff, 0x1f, 0x00, 0xe0, 0x1f, 0x00, 0xff, 0xc1,
  0x3f, 0xc0, 0x3f, 0xe0, 0x1f, 0xf0, 0x0f, 0xfe, 0xf8, 0x0f, 0xf0, 0x0f,
  0xf8, 0x07, 0xfc, 0x01, 0xf0, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0xe0,
  0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0xe0,
  0x1f, 0xe0, 0xff, 0xe0, 0x1f, 0x80, 0x7f, 0xe0, 0x0f, 0xe0, 0x0f, 0xfe,
  0xf8, 0x07, 0xf0, 0x07, 0xf0, 0x0f, 0xfc, 0x01, 0xf8, 0xff, 0xff, 0x07,
  0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0x3f, 0x00, 0xe0, 0xff, 0xff, 0xff, 0xe0, 0x1f, 0x80, 0x7f, 0xf0,
  0x0f, 0xe0, 0x0f, 0xfe, 0xfc, 0x03, 0xf8, 0x07, 0xe0, 0x0f, 0xfc, 0x01,
  0xf8, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x0f, 0x00,
  0x00, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0xe0, 0xff, 0xff, 0x7f, 0xe0,
  0x0f, 0x00, 0x7f, 0xf0, 0x0f, 0x00, 0x00, 0xfe, 0xfe, 0x01, 0xf8, 0x03,
  0xe0, 0x0f, 0xfc, 0x01, 0xf8, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0xc0,
  0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0x00, 0xe0,
  0xff, 0xff, 0x7f, 0xe0, 0x0f, 0x00, 0xff, 0xf0, 0x07, 0x00, 0x00, 0xfe,
  0xff, 0x00, 0xf8, 0x03, 0xe0, 0x0f, 0xfc, 0x01, 0xf8, 0xff, 0xff, 0x03,
  0x00, 0x00, 0x00, 0xc0, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff,
  0xff, 0x3f, 0x00, 0xe0, 0xff, 0xff, 0x3f, 0xf0, 0x0f, 0x00, 0xff, 0xf0,
  0x07, 0x00, 0x00, 0xfe, 0xff, 0x01, 0xf8, 0xff, 0xff, 0x0f, 0xfc, 0x01,
  0xfc, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x80, 0xff, 0x03, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0xe0, 0xff, 0xff, 0x0f, 0xf0,
  0x0f, 0x00, 0xfe, 0xf0, 0x07, 0x00, 0x00, 0xfe, 0xff, 0x01, 0xf8, 0xff,
  0xff, 0x1f, 0xfc, 0x01, 0xfc, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x80,
  0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0xe0,
  0xff, 0xff, 0x01, 0xf0, 0x0f, 0x00, 0xfe, 0xf0, 0x07, 0x00, 0x00, 0xfe,
  0xff, 0x03, 0xf8, 0xff, 0xff, 0x1f, 0xfc, 0x01, 0xfc, 0xff, 0xff, 0x03,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff,
  0xff, 0x7f, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xfe, 0xf0,
  0x07, 0x00, 0x00, 0xfe, 0xff, 0x03, 0xf8, 0xff, 0xff, 0x1f, 0xfc, 0x01,
  0xfc, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xf0,
  0x0f, 0x00, 0xff, 0xf0, 0x07, 0x00, 0x00, 0xfe, 0xff, 0x07, 0xf8, 0xff,
  0xff, 0x0f, 0xfc, 0x01, 0xfc, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0x7f, 0x00, 0xe0,
  0x1f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xff, 0xf0, 0x07, 0x00, 0x00, 0xfe,
  0xfb, 0x0f, 0xf8, 0x03, 0x00, 0x00, 0xfc, 0x01, 0xfe, 0xff, 0xff, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff,
  0xff, 0xff, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xe0, 0x0f, 0x00, 0xff, 0xf0,
  0x07, 0xc0, 0x0f, 0xfe, 0xf1, 0x0f, 0xf8, 0x03, 0x00, 0x00, 0xfc, 0x01,
  0xfe, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xe0,
  0x0f, 0x00, 0x7f, 0xf0, 0x0f, 0xe0, 0x0f, 0xfe, 0xf0, 0x1f, 0xf8, 0x07,
  0x00, 0x00, 0xfc, 0x01, 0xfe, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x7f, 0x00, 0xe0,
  0x1f, 0x00, 0x00, 0xe0, 0x1f, 0x80, 0x7f, 0xf0, 0x0f, 0xe0, 0x0f, 0xfe,
  0xe0, 0x1f, 0xf0, 0x07, 0xe0, 0x0f, 0xfc, 0x01, 0xfe, 0xff, 0xff, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff,
  0xff, 0x1f, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xc0, 0x3f, 0xc0, 0x7f, 0xe0,
  0x1f, 0xf0, 0x0f, 0xfe, 0xc0, 0x3f, 0xf0, 0x0f, 0xf0, 0x0f, 0xfc, 0x01,
  0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0xff, 0xff, 0x03, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xc0,
  0x7f, 0xe0, 0x3f, 0xe0, 0x3f, 0xf8, 0x07, 0xfe, 0xc0, 0x3f, 0xf0, 0x1f,
  0xf8, 0x07, 0xfc, 0x01, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x7f, 0x00, 0x00, 0xe0,
  0x1f, 0x00, 0x00, 0x80, 0xff, 0xff, 0x3f, 0xc0, 0xff, 0xff, 0x07, 0xfe,
  0x80, 0x7f, 0xe0, 0xff, 0xff, 0x07, 0xfc, 0x1f, 0x00, 0x3c, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff,
  0x0f, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0x00, 0xff, 0xff, 0x1f, 0xc0,
  0xff, 0xff, 0x03, 0xfe, 0x80, 0xff, 0xc0, 0xff, 0xff, 0x03, 0xfc, 0x1f,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xff, 0x03, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0x00,
  0xff, 0xff, 0x0f, 0x80, 0xff, 0xff, 0x01, 0xfe, 0x00, 0xff, 0x80, 0xff,
  0xff, 0x01, 0xfc, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0xe0,
  0x1f, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x07, 0x00, 0xff, 0xff, 0x00, 0xfe,
  0x00, 0xff, 0x01, 0xff, 0xff, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f,
  0x00, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x01, 0x00,
  0xfc, 0x7f, 0x00, 0xfe, 0x00, 0xfe, 0x03, 0xfc, 0x3f, 0x00, 0xe0, 0x1f,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xc0, 0x3f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
  0x0f, 0x00, 0x00, 0x00 };

#endif

volatile uint64_t MorseOutput::TOTcounter;                       // holds millis for Time-Out Timer

char printToScroll_buf[16] = "";       // replaces String printToScroll_buffer
int  printToScroll_bufLen = 0;         // tracks current length
// IMPROVEMENT: the old printToScroll_buffer was a global String that did
// a heap realloc on every "+= text" call (multiple times per CW element).
// New code: zero heap operations during accumulation. One String construction
// at flush time (every ~10 chars), which is ~10× fewer allocations.

/// The most printToScroll() hands to printToScroll_internal() in one go.
/// Bounded by the buffer itself, and by one scroll line: printToScroll_internal()
/// *wraps* a chunk that does not fit the rest of the current line, but it never
/// *splits* one - it memcpy's the whole chunk into textBuffer[bottomLine] and
/// draws it with a single printOnScroll(). A chunk wider than a line would
/// therefore be drawn past the right edge of the display (and a much wider one
/// would run over the row in textBuffer[], which holds 2*NoOfCharsPerLine+1
/// bytes). 14 on the OLED, 15 on the LCD - both compile-time constants, so this
/// costs nothing in a path that runs once per CW element.
static const int printToScroll_bufMax =
        ((int) sizeof(printToScroll_buf) - 1 < NoOfCharsPerLine)
        ? (int) sizeof(printToScroll_buf) - 1
        : NoOfCharsPerLine;

FONT_ATTRIB printToScroll_lastStyle = REGULAR;

/////////////////////// parameters for LF tone generation and  HF (= vol ctrl) PWM
int toneFreq = 500 ;
int toneChannel = 2;      // this PWM channel is used for LF generation, duty cycle is 0 (silent) or 50% (tone)
int lineOutChannel = 3;   // this PWM channel is used for line-out LF generation, duty cycle is 0 (silent) or 50% (tone)
int volChannel = 8;       // this PWM channel is used for HF generation, duty cycle between 1% (almost silent) and 100% (loud)
int pwmResolution = 10;
unsigned int volFreq = 32000; // this is the HF frequency we are using

const int  dutyCycleFiftyPercent =  511;                                                                             ;
const int  dutyCycleTwentyPercent = 250;
const int  dutyCycleZero = 0;


////// Display functions

// Idempotency cache for the very common "clearStatusLine() followed by
// printOnStatusLine(true, 0, str)" pattern used by the main menu and the
// preferences menu on every encoder click. Without this, the white flash
// between the clear and the reprint is visible as a flicker. We defer the
// actual clear until the next printOnStatusLine call: if it would
// reproduce the currently-visible text at xpos=0, we skip both ops.
namespace {
    bool   statusClearPending = false;   // clearStatusLine was called, not yet flushed
    String statusLineCache;              // text last drawn via printOnStatusLine at xpos=0
    bool   statusLineStrong  = false;    // strong flag for that cached text
    // Pixel width of the widest xpos==0 (full-line) text still potentially visible on
    // screen. Unlike statusLineCache (invalidated by the very next narrow write, since the
    // visible text is then no longer a single known string), this is deliberately NOT reset
    // by a narrow write - only by something that actually wipes the line/screen. A narrow
    // write (WPM digits, keyer symbol, ...) must clear at least this far right, or it leaves
    // fragments of whatever wider full-line message (e.g. "Continue with paddle") is still
    // sitting past its own narrow width - see printOnStatusLine().
    uint16_t statusLineCacheWidth = 0;

    // Paint the full-width white background, respecting the battery icon's
    // right-side reserved area. Extracted so clearStatusLine() and the
    // deferred-clear path inside printOnStatusLine() stay in sync.
    void paintStatusBackground() {
        display.setFont(DialogInput_plain_12);
        display.setColor(WHITE);
#ifdef CONFIG_MCP73871
        if (MorseOutput::batteryIconVisible)
            display.fillRect(0, 0, display.getWidth() - 34, SCROLL_TOP);
        else
#endif
            display.fillRect(0, 0, display.getWidth(), SCROLL_TOP);
        display.setColor(BLACK);
    }
}

void MorseOutput::initDisplay()
{
#ifdef OLED_RST
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);
#endif
  display.init();
#ifdef CONFIG_TFT
  if (MorsePreferences::leftHanded)
#else
  if (!MorsePreferences::leftHanded)
#endif
    display.flipScreenVertically();
  display.clear();
  // Screen was just wiped (e.g. by MorseGameMode::exit reinitialising the
  // panel after a game). Drop any cached status-line state so the next
  // printOnStatusLine actually repaints rather than short-circuiting.
  statusClearPending = true;
  statusLineCache    = "";
  statusLineCacheWidth = 0;
}



#ifdef CONFIG_TFT
// M6: the active theme's CW-transcription colour, the OK/ERR result colours, and
// its background, captured by setTheme() and applied by printOnScroll() when
// drawing MORSE_* / OK_RESULT / ERR_RESULT styled text.
static uint16_t currentMorseColor = 0xFFFF;
static uint16_t currentOkColor    = 0x07E0;
static uint16_t currentErrColor   = 0xF800;
static uint16_t currentThemeBg    = 0x0000;

void MorseOutput::setTheme (uint8_t theme) {
  //DEBUG("Theme: " + String(theme));
  display.setTheme(MorsePreferences::themeList[theme].foreground,
                   MorsePreferences::themeList[theme].background);
  currentMorseColor = MorsePreferences::themeList[theme].morse;
  currentOkColor    = MorsePreferences::themeList[theme].ok;
  currentErrColor   = MorsePreferences::themeList[theme].err;
  currentThemeBg    = MorsePreferences::themeList[theme].background;
}

#endif

void MorseOutput::clearDisplay() {
    display.clear();
    display.display();
    // Whole screen was wiped; the status-line cache no longer reflects
    // what's on screen. Force the next printOnStatusLine to repaint its
    // full background by treating it as if clearStatusLine was just called.
    statusClearPending = true;
    statusLineCache    = "";
    statusLineCacheWidth = 0;
#ifdef CONFIG_MCP73871
    batteryDisplayDirty = true;
    batteryIconVisible = false;
#endif
}

void MorseOutput::refreshDisplay()
{
  display.display();
}

uint8_t MorseOutput::getScrollTop() {
#ifndef CONFIG_TFT
  return 15;
#else
  display.setFont(DialogInput_plain_12);
  uint8_t top =  display.getStringHeight("j");
  // display.setFont(DialogInput_plain_15);
  return top ;
#endif
}


void MorseOutput::sleep()
{
  display.displayOff(); // OLED sleep
}

void MorseOutput::decreaseBrightness() {
    switch (MorsePreferences::oledBrightness) {
      case 255:
                MorsePreferences::oledBrightness = 127;
                break;
      case 127:
                MorsePreferences::oledBrightness = 63;
                break;
      case 63:
                MorsePreferences::oledBrightness = 28;
                break;
      case 28:
                MorsePreferences::oledBrightness = 9;
                break;
      default:
                MorsePreferences::oledBrightness = 255;
                break;
    }
    display.setBrightness(MorsePreferences::oledBrightness);
    MorsePreferences::writeBrightnessPreference(MorsePreferences::oledBrightness);

}

void MorseOutput::setBrightness(uint8_t brightness) {
  display.setBrightness(brightness);
}

#ifdef CONFIG_SCROLL_FONT_SIZE
/// (Re)derive the visible-line count and scrollback depth from the current
/// Font Size preference. Needed in two places: right after the preferences
/// menu's encoder-adjust changes it, and once at boot - NVS may have loaded
/// posScrollFont's stored value as Small from an earlier session, but
/// NoOfVisibleLines is statically initialised to the normal (4-line) default
/// and nothing else re-derives it, so a device that boots with the small font
/// already saved would render small text but still only show 4 lines until
/// the user re-visited the preference.
void MorseOutput::applyScrollFontGeometry() {
  // relPos == maxPos is the "glued to the live bottom" state that
  // printToScroll_internal() checks to decide whether to live-print each new
  // character as it arrives (relPos == maxPos) or leave the display alone
  // because the user is looking at scrolled-back history (relPos != maxPos).
  // maxPos itself is about to change below, so capture whether we were glued
  // *before* recomputing it - otherwise a plain constrain() would silently
  // un-glue an actively-watched live view (e.g. mid CW Generator output)
  // merely because Small->Normal made maxPos larger: the old relPos value is
  // still in [0, new maxPos], so constrain() would leave it unchanged, and it
  // would then equal the new maxPos only by coincidence. When it doesn't
  // (this direction), every future character stops appearing at all, since
  // the live-print fast path only fires while glued.
  boolean wasAtBottom = (relPos == maxPos);
  NoOfVisibleLines = MorsePreferences::pliste[posScrollFont].value ? NoOfVisibleLinesSmall : NoOfVisibleLinesNormal;
  maxPos = NoOfLines - NoOfVisibleLines;
  // Glued view stays glued (jumps to the new bottom); a scrolled-back view
  // keeps its numeric position, just clamped to the new (possibly smaller)
  // range - close enough for the rare case of a snapshot recall or protocol
  // change landing while someone happens to be reviewing history.
  relPos = wasAtBottom ? maxPos : constrain(relPos, 0, maxPos);
  // Give the rows a small amount of extra breathing room between them (up to
  // +2px over the honest per-glyph pitch), then centre the resulting block -
  // rows-plus-gaps together - top and bottom margin exactly equal, by
  // construction (both computed from the same actual content height, not
  // estimated separately - a first attempt at this used the raw gap size
  // as the top margin directly, which isn't the same number and left the
  // block still visibly closer to the top). setDefaultScrollFont() first so
  // LINE_HEIGHT (a live macro reading whatever font is *currently* active)
  // reflects the font this geometry is for, not whatever was last set by an
  // unrelated caller (e.g. the status line).
  setDefaultScrollFont();
  int available = display.getHeight() - SCROLL_TOP;
  int leftover = available - NoOfVisibleLines * LINE_HEIGHT;
  int gap = leftover > 0 ? min(2, leftover / (NoOfVisibleLines + 1)) : 0;
  scrollLineStep = LINE_HEIGHT + gap;
  int contentHeight = NoOfVisibleLines * scrollLineStep - gap;   // no trailing gap after the last row
  scrollTopPad = (available - contentHeight) / 2;
}
#endif


/// Accumulate text for the scroll area, handing it to printToScroll_internal()
/// a bufferful at a time. The buffer is purely an optimisation: generateCW()
/// calls this once per CW element, and batching ~10 characters before building
/// the String that printToScroll_internal() takes saves ~10x the heap traffic.
/// It is not a limit on what a caller may print - text that does not fit is
/// consumed in successive chunks, each flushed in turn, until all of it has
/// been handed on.
///
/// It used to be clamped instead: whatever did not fit the free space in the
/// buffer was silently dropped, so every string over 15 characters lost its
/// tail. That cost the QSO Bot the closing bracket and the line break of
/// "[QSO complete (no RST)]" (and the line break of "(click to exit)\n"), and
/// truncated whole-word display in the CW Generator for words longer than 15
/// characters - English Words at unlimited length reaches 17, and a file-player
/// token may be up to 127.
void MorseOutput::printToScroll(FONT_ATTRIB style, const String& text, boolean autoflush, boolean scroll) {
    boolean styleChanged = (style != printToScroll_lastStyle);
    int textLen = text.length();
    boolean lengthExceeded = (printToScroll_bufLen + textLen) > 10;

    if (styleChanged || lengthExceeded) {
        MorseOutput::flushScroll(scroll);              // still carries the previous style
    }

    printToScroll_lastStyle = style;                   // the flushes below use the new one

    // Append text to the buffer, emptying it as often as it takes. Note the
    // loop body always makes progress: the buffer is never full on entry
    // (either it already had room, or the flush just cleared it), so copyLen
    // is at least 1. An empty string appends nothing - clearBuffer() relies on
    // that to flush without adding anything.
    const char* src = text.c_str();
    int consumed = 0;
    while (consumed < textLen) {
        if (printToScroll_bufLen >= printToScroll_bufMax) {
            MorseOutput::flushScroll(scroll);          // full: empty it and carry on
            printToScroll_lastStyle = style;           // which resets the style - put it back,
        }                                              // or every chunk but the first goes REGULAR
        int copyLen = textLen - consumed;
        if (copyLen > printToScroll_bufMax - printToScroll_bufLen)
            copyLen = printToScroll_bufMax - printToScroll_bufLen;
        memcpy(&printToScroll_buf[printToScroll_bufLen], src + consumed, copyLen);
        printToScroll_bufLen += copyLen;
        printToScroll_buf[printToScroll_bufLen] = '\0';
        consumed += copyLen;
    }

    if (autoflush || (textLen > 0 && src[textLen - 1] == '\n')) {
        MorseOutput::flushScroll(scroll);
    }
}

void MorseOutput::clearBuffer() {
  MorseOutput::printToScroll(REGULAR, "", false, false);                     // clear the buffer first
}


void MorseOutput::clearScrollBuffer() {
    printToScroll_buf[0] = '\0';
    printToScroll_bufLen = 0;
    printToScroll_lastStyle = REGULAR;
}




void MorseOutput::flushScroll(boolean scroll) {
    if (printToScroll_bufLen != 0) {
        // printToScroll_internal still takes String — we construct one here.
        // This is only called when the buffer is flushed (every ~1-10 chars),
        // so one String construction per flush is acceptable.
        MorseOutput::printToScroll_internal(printToScroll_lastStyle,
                                            String(printToScroll_buf), scroll);
        clearScrollBuffer();
    }
}
 
/// store text in textBuffer, if it fits the screen line; otherwise scroll up, clear bottom buffer, store in new buffer, print on new line

void MorseOutput::printToScroll_internal(FONT_ATTRIB style, const String& text, boolean scroll) {

  // for DEBUG
  //char c;
  //unsigned char ch;
  //
  static uint8_t pos = 0;
  static FONT_ATTRIB lastStyle = REGULAR;
  uint8_t l = text.length();
  if (l == 0) {                               // an empty string signals we should clear the buffer
    for (int i = 0; i < NoOfLines; ++i) {
      textBuffer[i][0] = (char) 0;                    /// empty this line
    }
    refreshScrollArea((NoOfLines + bottomLine - (NoOfVisibleLines - 1)) % NoOfLines);
    pos = scrollScreenPos = 0;                                // reset the position pointers
    return;
  }

  int linebreak = text.endsWith("\n");
  String stripped;
  if (linebreak) {
    stripped = text.substring(0, l - 1);
    l = stripped.length();
  } else {
    stripped = text;   // stripped always holds the working copy
  }

#ifdef CONFIG_TFT
  // measure against the *default* scroll font, not whatever printOnScroll()
  // (or unrelated code, e.g. the status line) last left active
  setDefaultScrollFont();
  int textTooLong = (scrollScreenPos + l > display.getWidth()/display.getStringWidth("A"));
#else
  int textTooLong = (scrollScreenPos + l > NoOfCharsPerLine);
#endif

  if (textTooLong) {                 // we need to scroll up and start a new line
    MorseOutput::newLine(scroll);
    pos = 0;  scrollScreenPos = 0; lastStyle = REGULAR;
  }

  // After a wrap to a new line, discard a leading space: the separator blank
  // between two words belongs to the end of the previous line, not to the
  // start of the new one, where it would push the first word one column to
  // the right. Was LCD-only until the word-boundary wrap made the case
  // systematic on the OLED too - there, a line that happens to end exactly on
  // the last column is always followed by the separator blank, so every
  // second or third line started with an indent.
  if (scrollScreenPos == 0 && l > 0 && stripped[0] == ' ') {
    stripped.remove(0, 1);
    l = stripped.length();
  }

  const String& t = stripped;   // always use stripped from here on

  /// store text in buffer
  if (style == REGULAR) {
    memcpy(&textBuffer[bottomLine][pos], t.c_str(), l);  // copy the string of characters
    pos += l;
    textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
  } else {
    if (style == lastStyle)  {                                // not regular, but we have no change in style
      // The previous emphasized chunk in this same style ended with a
      // trailing style marker. To MERGE this new chunk with that one
      // (so the renderer sees a single emphasized region rather than
      // toggling style off-then-on between them) we overwrite that
      // trailing marker with our new text and emit a fresh trailing
      // marker after it. Without this, callers that write same-styled
      // text in many small chunks — e.g. the QSO Bot's char-by-char
      // display — end up with markers between every chunk, which the
      // renderer interprets as alternating style toggles (every other
      // char flips back to REGULAR).
      if (pos > 0 && textBuffer[bottomLine][pos - 1] == (char) style) {
        --pos;                                                // erase previous closing marker
      } else {
        // Defensive fallback: REGULAR text was inserted after the
        // previous emphasized chunk (which doesn't update lastStyle),
        // so the buffer doesn't end with our marker. Open a fresh
        // emphasized region.
        textBuffer[bottomLine][pos++] = (char) style;
      }
      memcpy(&textBuffer[bottomLine][pos], t.c_str(), l);
      pos += l;
      textBuffer[bottomLine][pos++] = (char) style;           // new closing marker
      textBuffer[bottomLine][pos] = (char) 0;
    } else {
      //DEBUG("NOTlastStyle :" + t);
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker at the beginning
      memcpy(&textBuffer[bottomLine][pos], t.c_str(), l);  // copy the string of characters
      pos += l;
      textBuffer[bottomLine][pos++] = (char) style;           // add the style marker at the end
      textBuffer[bottomLine][pos] = (char) 0;                 // add 0 character
      lastStyle = style;                                      // remember new style flag
    }
  }
///// for debugging: show contents of text buffer
///DEBUG("Buffer:");
///for (int i  = 0; (c = textBuffer[bottomLine][i]); ++i) {
///  DEBUG(String( ch = c ) + " <");
/// }
/////

  if (relPos == maxPos) {                                     // we show the bottom lines on the screen, therefore we add the new stuff  immediately
    /// and send string to screen, avoiding refresh of complete line
    //DEBUG("relPos: " + String(relPos));
    MorseOutput::printOnScroll(NoOfVisibleLines - 1, style, scrollScreenPos, t);               // these characters are 9 pixels wide,
  }
  setDefaultScrollFont();
  scrollScreenPos += (stringWidth(t) / C_WIDTH);
  if (linebreak) {
    MorseOutput::newLine(scroll);
    pos = 0;  scrollScreenPos = 0; lastStyle = REGULAR;
  }
}

/// Query only, no side effects: would displaying `wordLen` more columns of
/// text overflow the current scroll line? Used by the CW Generator to
/// decide, before it starts revealing a word character-by-character,
/// whether to wrap at the word boundary instead of relying on
/// printToScroll_internal's per-character wrap (which only sees one token
/// at a time and so cannot avoid splitting a word). Only makes sense for a
/// generator, which already knows the whole word up front (in clearText)
/// before showing its first character; live keyed/decoded input has no such
/// look-ahead, so it still relies on the plain per-character wrap.
/// Returns false (no point wrapping) if the line is already empty, and also
/// if the word is wider than a whole line: that one has to be hard-wrapped
/// somewhere no matter what, so it is left to the per-character wrap - the
/// pre-emptive break would otherwise fire again for every single character
/// (each time the rest of the word still doesn't fit), stranding them one
/// per line.  English Words with "unlimited" length reaches 17 characters,
/// which is longer than the OLED's 14-column line.
boolean MorseOutput::wordNeedsWrap(uint16_t wordLen) {
#ifdef CONFIG_TFT
  setDefaultScrollFont();        // measure in the font the scroll area is drawn in
  uint16_t width = display.getWidth() / display.getStringWidth("A");
#else
  uint16_t width = NoOfCharsPerLine;
#endif
  if (scrollScreenPos == 0 || wordLen > width)
    return false;
  return (scrollScreenPos + wordLen > width);
}


void MorseOutput::newLine(boolean scroll) {
  //DEBUG("Newline!");
  linePointer = (linePointer + 1) % NoOfLines;
  if (relPos && relPos != maxPos)
    --relPos;
  bottomLine = linePointer;
  textBuffer[bottomLine][0] = (char) 0;               /// and empty the bottom line

  if (relPos == 0 || relPos == maxPos)
    refreshScrollArea(relPos);
  if (scroll)
    MorseOutput::displayScrollBar(true);

}

/// refresh all visible lines from buffer in scroll area;

void MorseOutput::refreshScrollArea(int relPos) {
  for (int i = 0; i < NoOfVisibleLines; ++i)
    refreshScrollLine((bottomLine + relPos + 1 + i) % NoOfLines, i);
  display.display();
}

/// print a line to the screen

void MorseOutput::refreshScrollLine(int bufferLine, int displayLine) {
  String temp;
  temp.reserve(16);
  temp = "";
  char c;
  unsigned char ch;
  boolean irFlag = false;
  FONT_ATTRIB style = REGULAR;
  int pos = 0;
  uint16_t charsPrinted;

  display.setColor(BLACK);
  #ifdef CONFIG_TFT
  display.fillRect(0, SCROLL_ROW_TOP + displayLine * LINE_STEP , display.getWidth()-1, LINE_HEIGHT); // black out the line on screen
  #else
  display.fillRect(0, SCROLL_TOP + displayLine * LINE_HEIGHT , display.getWidth()-1, LINE_HEIGHT+1); // black out the line on screen

  #endif
  for (int i = 0; (c = textBuffer[bufferLine][i]) ; ++i) {
    // if (c == ' ') DEBUG("Blank!");
    if (c <= ERR_RESULT)   {         /// a style marker (1..ERR_RESULT)
      if (irFlag)         /// at the end of an emphasized string
      {
            //DEBUG("irFl>>" + temp + "<<");
        // split across two statements: C_WIDTH must be evaluated *after* printOnScroll()
        // has picked the font it just drew with (evaluation order of / operands is
        // otherwise unspecified, and printOnScroll() sets the font as a side effect)
        uint16_t printedWidth = MorseOutput::printOnScroll(displayLine, style, pos, temp);
        charsPrinted = printedWidth / C_WIDTH;
        style = REGULAR;
        pos += charsPrinted;
        temp = "";
        irFlag = false;
      }
      else                /// at the beginning of an emphasized string
      {
        if (temp.length()) {
              //DEBUG("noFl>>" + temp + "<<");

          // see the matching comment above: force printOnScroll() to run (and pick its
          // font) before C_WIDTH is evaluated against it
          uint16_t printedWidth = MorseOutput::printOnScroll(displayLine, style, pos, temp);
          charsPrinted = printedWidth / C_WIDTH;
          style = REGULAR;
          pos += charsPrinted;
          temp = "";
        }
        style = (FONT_ATTRIB) c;
        irFlag = true;
      }
    }
    else {                /// normal character - add it to temp
      temp += c;
      // DEBUG("temp >>" + temp + "<<");
    }
  }

  if (temp.length())
    MorseOutput::printOnScroll(displayLine, style, pos, temp);
}


/// place a string onto the scroll area; line = 0 .. NoOfVisibleLines-1

uint16_t MorseOutput::printOnScroll(uint8_t line, FONT_ATTRIB how, uint8_t xpos, const String& mystring, boolean small, boolean forceNormal) {
  uint16_t w;
  int x, y;

  boolean inverse = (how == INVERSE_REGULAR || how == INVERSE_BOLD);
  boolean bold    = (how & BOLD) || how == OK_RESULT || how == ERR_RESULT;

  if (inverse)
    display.setColor(WHITE);
  else
    display.setColor(BLACK);
  // an explicit small=true always wins (callers use it to force a narrow fit,
  // e.g. an IP address line) and always keeps the original (full-range)
  // IntelOneMono 12pt regardless of the Font Size preference; only the
  // *default* size (small==false) follows it, and only on the M32 Pocket
  // (non-Accessibility) - see scrollFont().
  if (small) {
    display.setFont(bold ? DialogInput_bold_12 : DialogInput_plain_12);
  } else {
    scrollFont(bold, forceNormal);
  }

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // convert the array characters into a String object
  w = stringWidth(mystring);

  x = xpos * C_WIDTH;
  // The centred layout (scrollTopPad + scrollLineStep) is computed for the
  // *default* scroll font, so its step is only guaranteed to clear that font's
  // glyph box. A row drawn in a different font - a caller-forced small size, or
  // forceNormal - has a taller box than the step allows, and consecutive such
  // rows overlap: the boot splash's two forceNormal lines sat 27px apart with a
  // 35px box, so the copyright line's background fill ate 8px off the bottom of
  // the version line. Those callers get the plain packed layout matching the
  // font they actually draw in, which is what they had before centring. (In
  // builds without the Font Size preference both branches are identical -
  // SCROLL_ROW_TOP/LINE_STEP fall back to SCROLL_TOP/LINE_HEIGHT.)
  if (small || forceNormal)
    y = SCROLL_TOP + line * LINE_HEIGHT;
  else
    y = SCROLL_ROW_TOP + line * LINE_STEP;

  // clear the print area
  #ifdef CONFIG_TFT
  display.fillRect(x,  y, w, LINE_HEIGHT);
  #else
  display.fillRect(x,  y, w, LINE_HEIGHT+1);
  #endif

  if (inverse)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);
  #ifdef CONFIG_TFT
  // M6: CW transcription (MORSE_*) draws in the theme's Morse colour. Weight was
  // already chosen above via (how & BOLD); here we override only the text colour,
  // leaving the just-filled theme background intact. drawString() honours the
  // last setTextColor(), so this sticks for exactly this string.
  if (how == MORSE_REGULAR || how == MORSE_BOLD)
    DisplayWrapper::getLGFX()->setTextColor(currentMorseColor, currentThemeBg);
  else if (how == OK_RESULT)
    DisplayWrapper::getLGFX()->setTextColor(currentOkColor, currentThemeBg);
  else if (how == ERR_RESULT)
    DisplayWrapper::getLGFX()->setTextColor(currentErrColor, currentThemeBg);
  #endif

  display.drawString(x, y, mystring);
  display.display();
  resetTOT();
  return w;         // we return the actual width of the output, in case of converted UTF8 characters
}




void MorseOutput::clearScroll() {
  MorseOutput::printToScroll_internal(REGULAR, "", false);
  clearScrollBuffer();
  clearScrollLines();
}



void MorseOutput::drawVolumeCtrl (boolean inverse, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t volume) { // volume = 0-19
  int i = (width - 4) * volume / 19;
// DEBUG("volCtrl: " + String(i));
  if (inverse)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);

  display.fillRect(x, y, width, height);

  if (!inverse)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);

  display.fillRect(x + 2, y + 4, i, height - 8);
  display.drawHorizontalLine(x + 2, y + height / 2, width - 4);
  display.display();
  resetTOT();
}


void MorseOutput::displayScrollBar(boolean visible) {          /// display a scroll bar on the right edge of the display
  #ifdef TFT_WIDTH
    const int v_start = TFT_WIDTH/5-2;
    const int bar_total = TFT_WIDTH - v_start;
  #else
    const int bar_total = 49 ;      // for the old Heltec display
    const int v_start = 15;
  #endif
  const int l_bar = NoOfVisibleLines * bar_total / NoOfLines;

  if (visible) {
    display.setColor(WHITE);
    display.drawVerticalLine(display.getWidth()-1, v_start, bar_total);
    display.setColor(BLACK);
    display.drawVerticalLine(display.getWidth()-1, v_start + (relPos * (bar_total - l_bar) / maxPos), l_bar);
  } else {
    display.setColor(BLACK);
    display.drawVerticalLine(display.getWidth()-1, v_start, bar_total);
  }
  display.display();
  resetTOT();
}


///// display battery status as text and icon, parameter v: Voltage in mV
/*
void MorseOutput::displayBatteryStatus(int v) {    /// v in millivolts!
  int a, b, c; String s; double d;
  s.reserve(20);
  d = v / 50;
  c = round(d) * 50;
  // DEBUG("v: " + String (v) + " c: " + String(c));
  a = c / 1000;
  b = (c - 1000 * a) / 100;
  if (v > 1000)
    s = "U: " + String(a) + "." + String(b) + " V";
  else
    s = "Unknown   ?";
#ifdef CONFIG_MCP73871
  uint8_t powerpath_state = (digitalRead(CONFIG_MCP_STAT1_PIN)<<2) + ( digitalRead(CONFIG_MCP_STAT2_PIN) << 1) + digitalRead(CONFIG_MCP_PG_PIN);
  switch (powerpath_state) {
      case 0:
        s = "Fault!";
        break;
      case 2:
        s = "Charge";
        break;
      case 3:
        //s = "Low!";
        break;
      case 4:
        s = "Full";
        break;
      case 6:
        s = "No Bat!";
        break;
      case 7:
        // Serial.println("Shutdown No Input Power Present");
        break;
      default:
        //Serial.print("Unknown State: ");
        break;
    }
#ifdef CONFIG_BATMEAS_PIN
// pocketwroom with voltage reading
//  s+= " " + String(v/1000.0);
#endif
#endif
  printOnScroll(2, REGULAR, 0, s);
#ifdef CONFIG_TFT
    #define batt_x 220
    #define batt_width 70
    #define batt_h (LINE_HEIGHT - 4)
    #define butt_width 12
    #define butt_h (LINE_HEIGHT - 16)
#else
    #define batt_x 75
    #define batt_width 35
    #define batt_h (LINE_HEIGHT - 4)
    #define butt_width 4
    #define butt_h (LINE_HEIGHT - 8)
#endif 
#define butt_x (batt_x + batt_width )

//#ifndef CONFIG_MCP73871
  int w = constrain(v, 3300, 4200);
  w = map(w, 3300, 4200, 0, batt_width - 4);
  display.drawRect(batt_x, SCROLL_TOP + 2 * LINE_HEIGHT + 3, batt_width, batt_h);
  display.drawRect(butt_x, SCROLL_TOP + 2 * LINE_HEIGHT + (3 + (batt_h - butt_h)/2), butt_width, butt_h);
  if (v > 1000)
    display.fillRect(batt_x+2, SCROLL_TOP + 2 * LINE_HEIGHT + 5 , w, LINE_HEIGHT - 8);
//#endif
  display.display();
} */

void MorseOutput::displayBatteryStatus(int v) {
  int a, b, c; String s; double d;
  s.reserve(20);
  d = v / 50;
  c = round(d) * 50;
  a = c / 1000;
  b = (int) (((c - 1000 * a) / 100) + 0.5);  // add 0.5 to round to nearest 100mV
  if (v > 1000)
    s = "U: " + String(a) + "." + String(b) + " V";
  else
    s = "";
 
  printOnScroll(NoOfVisibleLines - 1, REGULAR, 0, s);
 
#ifndef CONFIG_MCP73871
  
  // Non-MCP builds: show voltage-based battery icon on line 2
  #define BATT_X       75
  #define BATT_W       35
  #define BATT_H       (LINE_HEIGHT - 4)
  #define BATT_NUB_W   4
  #define BATT_NUB_H   (LINE_HEIGHT - 8)
  #define BATT_PAD     2
  #define BATT_NUB_X   (BATT_X + BATT_W)
 
  int batt_y = SCROLL_ROW_TOP + (NoOfVisibleLines - 1) * LINE_STEP + 3;
  int nub_y  = batt_y + (BATT_H - BATT_NUB_H) / 2;
  int fill_x = BATT_X + BATT_PAD;
  int fill_y = batt_y + BATT_PAD;
  int fill_max_w = BATT_W - 2 * BATT_PAD;
  int fill_h = BATT_H - 2 * BATT_PAD;
 
  display.setColor(BLACK);
  display.fillRect(BATT_X, batt_y, BATT_W + BATT_NUB_W + 1, BATT_H);
  display.setColor(WHITE);
  display.drawRect(BATT_X, batt_y, BATT_W, BATT_H);
  display.drawRect(BATT_NUB_X, nub_y, BATT_NUB_W, BATT_NUB_H);
 
  if (v > 1000) {
    int w = constrain(v, 3300, 4200);
    w = map(w, 3300, 4200, 0, fill_max_w);
    display.fillRect(fill_x, fill_y, w, fill_h);
  }
#endif

// 
  display.display();
}
   
#ifdef CONFIG_MCP73871
 
 
// ---- State variables ----
uint8_t  MorseOutput::ppCurrentState = 255;
uint8_t  MorseOutput::ppPreviousState = 255;
bool     MorseOutput::batteryDisplayDirty = true;
bool     MorseOutput::batteryIconVisible = false;
 
static uint8_t lastDrawnPPS = 255;
static uint8_t lastDrawnBars = 255;
 
// ---- Read raw powerpath state from pins ----
uint8_t MorseOutput::getPowerpathState() {
    return (digitalRead(CONFIG_MCP_STAT1_PIN) << 2)
         | (digitalRead(CONFIG_MCP_STAT2_PIN) << 1)
         |  digitalRead(CONFIG_MCP_PG_PIN);
}
 
// ---- Map voltage to 0-4 bars ----
static uint8_t voltageToBars(int v) {
    if (v < 3300) return 0;
    if (v < 3500) return 1;
    if (v < 3700) return 2;
    if (v < 3860) return 3;
    return 4;
}
 
// ---- Fast state check — called in ALL three loops ----
// Checks the ISR flag. If set, reads pins and compares with previous.
// If state changed, sets batteryDisplayDirty.
// If transition was charging→full, recalibrates vAdjust. (this has been scrapped)
// Takes microseconds (no ADC, no display).
/* void MorseOutput::checkPowerpathState() {
    if (!powerpath_event)
        return;
    powerpath_event = false;
 
    uint8_t state = getPowerpathState();
    if (state != ppCurrentState) {
        if (state == 6 && ppCurrentState != 6)  { // transition from battery to connected
          delay(1);
          //state = getPowerpathState();
        }
        ppPreviousState = ppCurrentState;
        ppCurrentState = state;
        batteryDisplayDirty = true;
        DEBUG ("Powerpath state changed: " + String(ppPreviousState) + " -> " + String(ppCurrentState));
    }
} */
 void MorseOutput::checkPowerpathState() {
    static unsigned long lastMeasurement = 0;
    if (millis() - lastMeasurement > 2400) { // check
      lastMeasurement = millis();
      uint8_t state = getPowerpathState();

      //DEBUG ("Powerpath state: " + String(state));
      //DEBUG ("Volt: " + String(batteryVoltage()));

     if (state == 4 && volt > 4290) // we seem to run on USB without a battery connected
        state = 6;
      if (state != ppCurrentState) {
        ppPreviousState = ppCurrentState;
        ppCurrentState = state;
        batteryDisplayDirty = true;
      }
    } else return;
}

// ---- Slow display update — called in menu and preferences loops ONLY ----
// Measures battery voltage (~50ms), redraws icon if needed.
// Also does periodic re-measurement every 60 seconds.
void MorseOutput::updateBatteryDisplay() {
    static unsigned long lastMeasurement = 0;

    if ((millis() - lastMeasurement > 20000) || batteryDisplayDirty) {  // periodic update every 10s, or if flagged dirty by state change
        lastMeasurement = millis();
        volt = batteryVoltage();
    }

    if (ppCurrentState == 255)
        ppCurrentState = getPowerpathState();

    uint8_t pps = ppCurrentState;
    uint8_t bars = voltageToBars(volt);

    if (pps == lastDrawnPPS && bars == lastDrawnBars && !batteryDisplayDirty)
        return;

    lastDrawnPPS = pps;
    lastDrawnBars = bars;
    batteryDisplayDirty = false;

    drawBatteryIcon(pps, bars);
    batteryIconVisible = true;
}
 
// ---- Erase the battery icon ----
void MorseOutput::clearBatteryIcon() {
    if (!batteryIconVisible) return;

    lgfx::LGFX_Device* lcd = DisplayWrapper::getLGFX();

    const int bodyW = 26, iconH = 16, nubW = 4, margin = 2;
    int ix = lcd->width() - bodyW - nubW - margin;
    int iy = (SCROLL_TOP - iconH) / 2;

    lcd->startWrite();
    lcd->fillRect(ix - 1, iy - 1, bodyW + nubW + 3, iconH + 2, TFT_WHITE);
    lcd->endWrite();

    batteryIconVisible = false;
    lastDrawnPPS = 255;
    lastDrawnBars = 255;
}
 
// ---- Draw the battery icon ----
// Size: body 20×12 + nub 3×6 = total 23×12 pixels
// Position: top-right corner of screen
// Well below the scroll area (scroll ends at ~114px, icon at ~304px)

void MorseOutput::drawBatteryIcon(uint8_t pps, uint8_t bars) {
    const int bodyW = 26, iconH = 16, nubW = 4, nubH = 8, pad = 2, margin = 2;
    int ix = display.getWidth() - bodyW - nubW - margin;
    int iy = (SCROLL_TOP - iconH) / 2;
    // Paint the FULL status-line-height column behind the icon with the
    // status-line background colour. clearStatusLine skips this 34-px
    // column when the icon is visible (to avoid overdrawing the icon),
    // so we have to fill the full column ourselves — otherwise the
    // strips above and below the icon end up showing whatever was
    // there before (e.g. the blue background painted by display.clear()
    // on game exit), creating a visible gap.
    display.setColor(WHITE);
    display.fillRect(display.getWidth() - 34, 0, 34, SCROLL_TOP);

    // Draw in status line text colour
    display.setColor(BLACK);
    display.drawRect(ix, iy, bodyW, iconH);
    display.fillRect(ix + bodyW, iy + (iconH - nubH) / 2, nubW, nubH);

    int fillX = ix + pad;
    int fillY = iy + pad;
    int fillH = iconH - 2 * pad;
    // DEBUG("PPS: " + String(pps));

    switch (pps) {
        case 2: {                                       // charging
            int cx = ix + bodyW / 2;
            display.fillRect(cx + 1, iy + 2, 3, 2);
            display.fillRect(cx - 1, iy + 4, 3, 2);
            display.fillRect(cx - 3, iy + 6, 7, 2);
            display.fillRect(cx + 1, iy + 8, 3, 2);
            display.fillRect(cx - 1, iy + 10, 3, 2);
            display.fillRect(cx - 2, iy + 12, 3, 2);
            break;
        }
        /* case 4:                                         // full  
            for (uint8_t i = 0; i < 4; i++)
                display.fillRect(fillX + i * 5, fillY, 4, fillH);
            break; */
       // case 0:
        case 6:                                         // no battery connected
            for (int i = 0; i < fillH; i++) {
                display.fillRect(fillX + (i * (bodyW - 2*pad) / fillH), fillY + i, 1, 1);
                display.fillRect(fillX + (bodyW - 2*pad) - 1 - (i * (bodyW - 2*pad) / fillH), fillY + i, 1, 1);
            }
            break;
        default:
            for (uint8_t i = 0; i < bars; i++)
                display.fillRect(fillX + i * 5, fillY, 4, fillH);
            break;
    }

    display.setColor(WHITE);  // restore
}
 
void MorseOutput::resetPowerpathDisplay() {
    batteryDisplayDirty = true;
    lastDrawnPPS = 255;
    lastDrawnBars = 255;
}
  
#endif  // CONFIG_MCP73871
 
void MorseOutput::displayEmptyBattery(void (*f)()) {                                /// display a warning and go to (return to) deep sleep
  display.clear();
  display.drawRect(10, 11, 95, 50);
  display.drawRect(105, 26, 15, 20);
  if (f != 0) {
    printOnScroll(1, INVERSE_BOLD, 4,  "EMPTY");
    delay(4000);
    (*f)();
  }
  else {
    printOnScroll(1, INVERSE_BOLD, 4,  "LOW BATTERY");
    display.display();
    delay(4000);
  }
}

#ifdef CONFIG_TFT
#define leftBoundary 56
#define logoWidth 14
#else
#define leftBoundary 35
#define logoWidth 7
#endif
#define meterWidth (leftBoundary - logoWidth)

/// display volume as a progress bar: vol = 1 - 20
void MorseOutput::displayVolume (boolean speedsetting, uint8_t volume) {
  //DEBUG("Volume: " + String(volume));
  drawVolumeCtrl(speedsetting ? false : true, display.getWidth()-leftBoundary, 0, meterWidth, SCROLL_TOP, volume);
  display.display();
}


////// S Meter for Trx mode

void MorseOutput::updateSMeter(int rssi) {

  static boolean wasZero = false;

  if (rssi == 0)
    if (wasZero)
      return;
    else {
      drawVolumeCtrl( false, display.getWidth()-leftBoundary, 0, meterWidth, SCROLL_TOP, 0);
      wasZero = true;
    }
  else {
    //DEBUG ("RSSI: " + String(rssi));
    uint8_t s_v = constrain(map(rssi, -150, -20, 0, 20), 0, 20);
    //DEBUG ("Value: " + String(s_v));
    drawVolumeCtrl( false, display.getWidth()-leftBoundary, 0, meterWidth, SCROLL_TOP, s_v);
    wasZero = false;
  }
  display.display();
}

/// for morse decoder: show a suqare on status line when we detected a signal

void MorseOutput::drawInputStatus( boolean on) {
  if (on)
    display.setColor(BLACK);
  else
    display.setColor(WHITE);
  display.fillRect(1, 1, 13, 13);
  display.display();
}


void MorseOutput::dispLoraLogo() {                     /// display a small logo in the top right corner to indicate we operate with LoRa
  display.setColor(BLACK);
  display.drawXbm(display.getWidth()-logoWidth, 2, lora_width, lora_height, lora_bits);
  display.setColor(WHITE);
  display.display();
}

void MorseOutput::dispWifiLogo() {     // display a small logo in the top right corner to indicate we operate with WiFi
  display.setColor(BLACK);
  display.drawXbm(display.getWidth()-logoWidth, 2, wifi_width, wifi_height, wifi_bits);
  display.setColor(WHITE);
  display.display();
}

#ifdef CONFIG_BLE_SERIAL
// Where the session glyph lives. On builds with a charge controller the battery
// icon owns the right-hand end of the STATUS line, so the glyph steps left of the
// strip paintStatusBackground() reserves. Deliberately NOT conditional on
// batteryIconVisible: menu_() calls clearDisplay() on entry, which clears that
// flag, so a glyph drawn during that window would land in the battery's place
// and be painted over the moment the icon is redrawn -- which looked exactly
// like the indicator failing to appear at all after leaving a mode.
static int bleLogoX() {
#ifdef CONFIG_MCP73871
  return display.getWidth() - 34 - ble_width - 4;      // 34 = that reserved width
#else
  return display.getWidth() - logoWidth;               // classic: battery is in the scroll area
#endif
}

void MorseOutput::dispBleLogo() {      // display a small logo in the top right corner while a BLE Serial client is connected
  display.setColor(BLACK);
  display.drawXbm(bleLogoX(), 2, ble_width, ble_height, ble_bits);
  display.setColor(WHITE);
  display.display();
}

void MorseOutput::clearBleLogo() {     // erase it when the session ends
  // Its own operation, because printOnStatusLine() short-circuits when asked to
  // draw text that is already on screen: coming back to the menu with the same
  // "Select Mode:" line repaints no background whatsoever, so a glyph left over
  // from a finished session would sit there forever.
  display.setColor(WHITE);
  display.fillRect(bleLogoX(), 2, ble_width, ble_height);
  display.setColor(WHITE);
  display.display();
}
#endif

#ifdef CONFIG_TFT
// Boot-splash tuning (tweakable):
#ifndef M32_LOGO_STEPS
#define M32_LOGO_STEPS     40              // animation frames — more = smoother (1 = none)
#endif
#ifndef M32_LOGO_STEP_MS
#define M32_LOGO_STEP_MS   16              // ms paced between frames — higher = calmer
#endif

void MorseOutput::dispM32Logo() {
  // Theme-independent boot splash: a pre-rendered anti-aliased "M32 Pocket" logo
  // (white-on-black 8-bit grayscale, m32logo_aa.h) grows smoothly out of the centre
  // and eases to rest at its native size (z = 1.0, drawn 1:1, so the resting logo is
  // pixel-crisp).
  //
  // Pre-rendering the AA offline sidesteps this board's two limits: there is no read
  // line (TFT_MISO=-1, so a runtime-AA push would fringe black) and no PSRAM (so a
  // large AA buffer won't allocate this late in boot). The asset already carries its
  // anti-aliasing and is drawn opaque on a black screen — no read-back, small buffer.
  // Growth is monotonic (easeOutCubic) so opaque frames never leave ghost edges.
  auto *lcd = DisplayWrapper::getLGFX();
  lcd->fillScreen(TFT_BLACK);
  LGFX_Sprite logo(lcd);
  logo.setColorDepth(16);
  if (logo.createSprite(M32_AA_W, M32_AA_H)) {
    // expand the 8-bit grayscale asset into the 16bpp sprite (grey → RGB565; grey is
    // colour-order agnostic, so no byte-swap concerns)
    for (int y = 0; y < M32_AA_H; ++y)
      for (int x = 0; x < M32_AA_W; ++x) {
        uint8_t v = M32logo_aa[y * M32_AA_W + x];
        logo.drawPixel(x, y, logo.color565(v, v, v));
      }
    logo.setPivot(M32_AA_W / 2.0f, M32_AA_H / 2.0f);
    const float cx = lcd->width()  / 2.0f;
    const float cy = lcd->height() / 2.0f;
    for (int i = 1; i <= M32_LOGO_STEPS; ++i) {
      float t   = (float)i / M32_LOGO_STEPS;
      float inv = 1.0f - t;
      float e   = 1.0f - inv * inv * inv;   // easeOutCubic: grows, then settles gently
      float z   = e;                        // ends at 1.0 → asset drawn 1:1 (crisp)
      if (z < 0.02f) z = 0.02f;
      logo.pushRotateZoom(lcd, cx, cy, 0.0f, z, z);
      delay(M32_LOGO_STEP_MS);
    }
    logo.deleteSprite();
  } else {
    // fallback: the original 1-bit logo at native size, white on black
    lcd->drawXBitmap((lcd->width() - M32c_width) / 2, (lcd->height() - M32c_height) / 2,
                     M32c_bits, M32c_width, M32c_height, TFT_WHITE, TFT_BLACK);
  }
  display.display();
}
#endif

//////// Display the status line in CW Keyer Mode
//////// Layout of top line:
//////// Tch ul 15 WpM
//////// 0    5    0


void MorseOutput::printOnStatusLine(boolean strong, uint8_t xpos, const String& string) {    // place a string onto the status line; chars are 7px wide = 18 chars per line
  // Fast path: deferred clear + identical full-line text → already on
  // screen, skip both operations entirely.
  if (xpos == 0 && statusClearPending &&
      statusLineCache == string && statusLineStrong == strong) {
    statusClearPending = false;
    resetTOT();
    return;
  }

  // Honour any pending clearStatusLine before drawing.
  if (statusClearPending) {
    paintStatusBackground();
    statusClearPending = false;
    statusLineCacheWidth = 0;   // background is genuinely blank now
  }

  if (strong)
    display.setFont(DialogInput_bold_12);
  else
    display.setFont(DialogInput_plain_12);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  uint16_t w = stringWidth(string);
  uint16_t x = xpos * display.getStringWidth("A");
  // Clear at least as far right as the last full-line write reached (statusLineCacheWidth):
  // a narrower write here (e.g. WPM digits) must not leave fragments of a wider message
  // (e.g. "Continue with paddle") sticking out past its own width. Only extends the clear
  // when that old content actually reaches into or past this xpos - never shrinks it.
  uint16_t clearW = w;
  if (statusLineCacheWidth > x) {
      uint16_t neededW = statusLineCacheWidth - x;
      if (neededW > clearW) clearW = neededW;
  }
  display.setColor(WHITE);
  display.fillRect(x, 0 , clearW, SCROLL_TOP);
  display.setColor(BLACK);
  display.drawString(x, 0, string);
  display.setColor(WHITE);
  display.display();

  // Only full-line writes (xpos==0) become the cached value; partial updates (WPM digits,
  // keyer mode, ...) invalidate statusLineCache since the visible text is no longer a single
  // known string - but deliberately leave statusLineCacheWidth alone: it keeps guarding
  // narrow writes against the wider text until a new full-line write replaces it, or
  // clearStatusLine()/clearDisplay() actually wipes the line/screen.
  if (xpos == 0) {
    statusLineCache  = string;
    statusLineStrong = strong;
    statusLineCacheWidth = w;
  } else {
    statusLineCache = "";
  }

  resetTOT();
  #ifdef CONFIG_MCP73871
  //  batteryDisplayDirty = true;    // redraw icon on next updateBatteryDisplay
  #endif
}



void MorseOutput::clearStatusLine() {
    // Defer the actual paint to the next printOnStatusLine. If that call
    // reprints the same text, both ops collapse to a no-op (no flicker).
    // All in-tree clearStatusLine callers immediately follow with a
    // printOnStatusLine, so the deferral is always honoured.
    statusClearPending = true;
}

/// clear all visible lines of the scroll area

void MorseOutput::clearScrollLines() {
  // A single rect over the whole scroll area, not N calls to clearLine() -
  // those only cover each row's own tight LINE_HEIGHT band, leaving the
  // gaps *between* rows (scrollTopPad before the first row, the small
  // per-row step above LINE_HEIGHT introduced by applyScrollFontGeometry()
  // to spread rows evenly) untouched. Switching Font Size changes those gaps
  // too, so a leftover fragment of the previous font's text could sit
  // exactly in a gap that used to not exist and never get cleared.
  #ifdef CONFIG_TFT
  display.setColor(BLACK);
  display.fillRect(0, SCROLL_TOP, display.getWidth() - 1, display.getHeight() - SCROLL_TOP);
  display.setColor(WHITE);
  #else
  // must match whatever font printOnScroll() will use to redraw these lines
  // right after - on TFT, LINE_HEIGHT depends on the active font, so clearing
  // at the wrong size leaves stray pixels or clips the freshly-drawn text.
  setDefaultScrollFont();
  for (int i = 0; i < NoOfVisibleLines; ++i) {
    MorseOutput::clearLine(i);
  }
  #endif
}

void MorseOutput::clearLine(uint8_t line) {                                              /// clear a line - display is done somewhere else!
  int y, l;
  y = SCROLL_ROW_TOP + line * LINE_STEP;
  l = display.getWidth()-1;
  display.setColor(BLACK);
  #ifdef CONFIG_TFT
  display.fillRect(0, y, l, LINE_HEIGHT);
  #else
  display.fillRect(0, y, l, LINE_HEIGHT+1);
  #endif
  display.setColor(WHITE);
}


void MorseOutput::showVolumeScope(uint16_t mini, uint16_t maxi)
{
  uint16_t a, b, c;
  a = map(mini, 0, 4096, 0, 125);
  b = map(maxi, 0, 4000, 0, 125);
  c = b - a;
  MorseOutput::clearLine(NoOfVisibleLines - 1);
  display.drawRect(5, SCROLL_ROW_TOP + (NoOfVisibleLines - 1) * LINE_STEP + 5, 102, LINE_HEIGHT - 8);
  display.drawRect(30, SCROLL_ROW_TOP + (NoOfVisibleLines - 1) * LINE_STEP + 5, 52, LINE_HEIGHT - 8);
  display.fillRect(a, SCROLL_ROW_TOP + (NoOfVisibleLines - 1) * LINE_STEP + 7, c, LINE_HEIGHT - 11);
  display.display();
}


void MorseOutput::resetTOT() {       //// reset the Time Out Timer - we do this whenever there is a screen update
    MorseOutput::TOTcounter = millis();
}


/////// functions for audio output

#ifdef CONFIG_TLV320AIC3100



// Level fed into the shared post-mixer VolumeStream (sidetone AND voice clips ride it).
// 0.79 is the ceiling: the sine generator is full scale and the codec DAC adds +2 dB, so
// 0.79 * 1.259 = 0.995 -- the last of the digital headroom. Voice clips stay clear because
// generate_audio.sh caps every clip's DECODED peak at 0.95 (0.95 * 0.79 * 1.259 = 0.945).
static const float SIDETONE_LEVEL = 0.79f;
// Speaker path: one gain step up (18 dB) with a -3 dB trim on the volume control = +3 dB
// net over the PR #208 setting, and 3 dB below the plain 18 dB that was rejected as too
// loud at full Tone Volume. Why the speaker needs it and the headphone does not: the old
// sidetone was a CLIPPED square wave, and its harmonics (1.8/3/4.2 kHz) sat exactly where
// this micro-speaker is efficient and the ear is most sensitive. De-clipping removed them,
// so a clean 600 Hz sine reads as quieter on the speaker even at higher RMS -- while on
// headphones, which reproduce 600 Hz perfectly well, CW is already far louder than before.
static const float SPEAKER_TRIM_DB = 3.0f;
// Headphone driver gain (valid 0..9 dB). PR #208 put this at the 9 dB maximum to close a
// ~21 dB speaker/headphone gap; with the speaker since raised by SPEAKER_TRIM_DB, phones
// came out louder than the speaker on the bench, so back off -- first to 6 dB, then to 3
// after a second listen ("at max volume the headphones get really very loud"). Take it off the GAIN
// and not off the volume control: the volume control is an attenuator ahead of the driver,
// so trimming there leaves the driver's own noise untouched and the noise floor becomes
// audible once Tone Volume is turned up to compensate (that is exactly what a -6 dB volume
// offset did during the PR #208 bench work). Lowering the driver gain scales signal and
// upstream noise together, so the floor rides down with it.
static const float HEADPHONE_GAIN_DB = 3.0f;
// Line-out drives someone else's input, so its level must be defined and must not depend
// on whether headphones happened to be plugged in earlier in the session. soundEnableLineOut()
// never wrote the driver gain at all, so it inherited whatever soundEnableHeadphone() last
// set -- 0 dB (the reset default) on a device that had not seen headphones, 9 dB after
// PR #208 on one that had. Before PR #208 the headphone path also used 0 dB, so line-out
// was accidentally deterministic; raising the headphone gain turned that into a silent
// 9 dB swing. Write it explicitly, and keep it at the pre-PR-#208 value.
static const float LINEOUT_GAIN_DB = 0.0f;

// Speaker volume curve, deliberately NOT the same as calcVolume().
//
// calcVolume()'s 3 dB steps span 48 dB (plus -54/-60 at the bottom). That range is right
// for headphones, where the quietest settings are genuinely useful, but far too wide for
// the built-in speaker: below roughly -30 dB it simply stops producing audible output, so
// the bottom third of the encoder's travel did nothing. That got worse when PR #208
// de-clipped the sidetone -- a clean sine has none of the 1-3 kHz harmonic content the
// small speaker actually radiates well, so its usable range shrank further.
//
// So the speaker gets 2 dB steps over 36 dB. The maximum is unchanged (the top of the
// range was never the problem); the minimum comes up by 24 dB, which is what turns the
// bottom settings back into something you can hear. Headphones keep calcVolume() and its
// near-silent lowest step.
float calcSpeakerVolume(uint8_t v) { // v = 0 - 19
  if (v == 0) return -78.0f;                                   // off (also explicitly muted)
  return (float) ((19 - v) * -2.0) - SPEAKER_TRIM_DB;
}

float calcVolume(uint8_t v) { // v = 0 - 19
  // we map the volume levels 0 - 19 to dB values for the codec
  // 
  switch (v) {
    case 0:
      return -78.0f; // off
    case 1:
      return -60.0f;
    case 2:
      return -54.0f;
    default:
      return (float) ((19 - v ) * -3.0); // calculate  -3dB steps 
  }
}

void soundEnableHeadphone(void) {
    codec.enableHeadphoneAmp();
    // codec.setHeadphoneVolume(-30.0f,-30.0f); // volume regulated by encoder
    codec.setHeadphoneVolume(calcVolume(MorsePreferences::sidetoneVolume)-3.0,calcVolume(MorsePreferences::sidetoneVolume)-3.0);
    codec.setHeadphoneGain(HEADPHONE_GAIN_DB,HEADPHONE_GAIN_DB); // valid db: 0 - 9; see HEADPHONE_GAIN_DB
    // Undo line-out mode. soundEnableLineOut() turns it on and nothing ever turned it off,
    // so once a line-out setting had been tried, the driver stayed configured for a light
    // load for the rest of the session -- including while actually driving 32 ohm phones.
    // Same class of bug as the gain above: these two functions must SET the driver state,
    // never inherit it from whichever one happened to run last.
    codec.setHeadphoneLineMode(false);
    codec.setHeadphoneMute(false); // unmute hp
    codec.setSpeakerMute(true); // mute class d speaker amp
}

void soundEnableSpeaker(void) {
    codec.enableSpeakerAmp();
    codec.setSpeakerGain(18.0f); // valid db: 6, 12, 18, 24; paired with SPEAKER_TRIM_DB below
    // codec.setSpeakerVolume(-13.0f); // volume set by encoder
    codec.setSpeakerVolume(calcSpeakerVolume(MorsePreferences::sidetoneVolume));
    codec.setSpeakerMute(false); // unmute class d speaker amp
    codec.setHeadphoneMute(true); // mute hp
}

void soundEnableLineOut(bool muted = false, bool variable = false) {
  // this has now to parameters: whether we want to mute the speaker and whether we want to use variable gain on the line-out
    codec.enableHeadphoneAmp();
    //codec.setHeadphoneVolume(-10.0f,-10.0f); // unmute
    //codec.setHeadphoneGain(-12.0f,-12.0f);
    codec.setHeadphoneMute(false); // unmute hp
    codec.setHeadphoneLineMode(true); // enable line-out mode
    codec.setHeadphoneGain(LINEOUT_GAIN_DB,LINEOUT_GAIN_DB); // define it; do not inherit it

    if (!variable) {
      //DEBUG("Fixed gain!");
      codec.setHeadphoneVolume(0.0, 0.0); // volume 0db on line-out
    }
    else
      codec.setHeadphoneVolume(calcVolume(MorsePreferences::sidetoneVolume)-3.0,calcVolume(MorsePreferences::sidetoneVolume)-3.0);

    if (muted)
          codec.setSpeakerMute(true); // mute class d speaker amp
      else
      {
        //DEBUG ("Not muted!");
        codec.enableSpeakerAmp();
        codec.setSpeakerGain(18.0f); // valid db: 6, 12, 18, 24; paired with SPEAKER_TRIM_DB below
        // codec.setSpeakerVolume(-10.0f); // volume set by encoder
        codec.setSpeakerVolume(calcSpeakerVolume(MorsePreferences::sidetoneVolume));
        codec.setSpeakerMute(false); // unmute class d speaker amp
      }
}


void MorseOutput::soundSetVolume(uint8_t v) { // v = 0 - 19
  if (codec.isHeadsetDetected()) {
    switch (MorsePreferences::pliste[posLineOut].value)
    {
      case 0: // headphones, speaker muted 
        if (v==0)
            codec.setHeadphoneMute(true); // mute hp
        else  
            codec.setHeadphoneMute(false);
        codec.setHeadphoneVolume(calcVolume(v)-3.0, calcVolume(v)-3.0); // small offset -- 0dB was a touch too loud, -6dB was too quiet
        break;
      case 1: // line-out, speaker not muted, fixed l-o gain
        if (v==0)
            codec.setSpeakerMute(true); // mute class d speaker amp
        else  
            codec.setSpeakerMute(false);
        codec.setSpeakerVolume(calcSpeakerVolume(v));
        break;
      case 2: // line-out, speaker not muted, variable l-o gain
        if (v==0) {
            codec.setSpeakerMute(true); // mute class d speaker amp
            codec.setHeadphoneMute(true); // mute hp
        }
        else  
        {
            codec.setSpeakerMute(false);
            codec.setHeadphoneMute(false);
        }
        codec.setSpeakerVolume(calcSpeakerVolume(v));
        codec.setHeadphoneVolume(calcVolume(v)-3.0, calcVolume(v)-3.0); // small offset -- 0dB was a touch too loud, -6dB was too quiet
        break;
      case 3: // line-out, speaker muted, fixed l-o gain
        break;
    }
  }
  /* {
    if (v==0)
      codec.setHeadphoneMute(true); // mute hp
    else  
      codec.setHeadphoneMute(false);
    codec.setHeadphoneVolume(calcVolume(v)-3.0, calcVolume(v)-3.0); // small offset -- 0dB was a touch too loud, -6dB was too quiet
  } */ 
  else {
    if (v==0)
      codec.setSpeakerMute(true); // mute class d speaker amp
    else  
      codec.setSpeakerMute(false);
    codec.setSpeakerVolume(calcSpeakerVolume(v));
  }
}



void MorseOutput::soundEventHandler() {
  // interrupt reason needs to be read, otherwise the codec won't send any further interrupts
  if (codec.readRegister(AIC31XX_INTRDACFLAG) & AIC31XX_HSPLUG) { // bit 4 is set on headset related interrupts
    //DEBUG("AIC31XX: Headset plug interrupt triggered");
  }
if (codec.isHeadsetDetected()) { 
    //DEBUG("AIC31XX: Headset interrupt: plugged-in");
    switch (MorsePreferences::pliste[posLineOut].value)
    {
      case 0: // headphones, speaker muted 
        soundEnableHeadphone();
        break;
      case 1: // line-out, speaker not muted, fixed gain
        soundEnableLineOut(false, false);
        break;
      case 2: // line-out, speaker not muted, variable gain
        soundEnableLineOut(false, true);
        break;
      case 3: // line-out, speaker muted, fixed gain
        soundEnableLineOut(true, false);
        break;
    }
} else {
    //DEBUG("AIC31XX: Headset interrupt: unplugged");
    soundEnableSpeaker();
       // enable speaker, mute headphones
  }   
}
#endif


void MorseOutput::soundSetup()
{
#ifndef CONFIG_SOUND_I2S
  // set up PWMs for tone generation
  ledcSetup(toneChannel, toneFreq, pwmResolution);
  ledcAttachPin(LF_Pin, toneChannel);

  ledcSetup(lineOutChannel, toneFreq, pwmResolution);
  ledcAttachPin(lineOutPin, lineOutChannel);                                    ////// change this for real version - no line out currntly

  ledcSetup(volChannel, volFreq, pwmResolution);
  ledcAttachPin(HF_Pin, volChannel);

  ledcWrite(toneChannel, 0);
  ledcWrite(lineOutChannel, 0);
#else
#ifdef CONFIG_WM8960
#pragma message ("WM8960 CODEC")
	Wire.begin(CONFIG_WM8960_SDA, CONFIG_WM8960_SCL);
	if (codec.begin() == false) //Begin communication over I2C
	{
		DEBUG("The WM8960 did not respond. Please check wiring.");
	} else {
    DEBUG("WM8960 is connected properly.");
    // General setup needed
    codec.enableVREF();
    codec.enableVMID();

#ifdef CONFIG_DECODER_I2S
    // configure mic input for decoder

    // link ADCLRC with DACLRC
    codec.setALRCGPIO();

    // Setup signal flow to the ADC

    codec.enableLMIC();
    codec.enableRMIC();

    // Connect from INPUT1 to "n" (aka inverting) inputs of PGAs.
    codec.connectLMN1();
    codec.connectRMN1();

    // Disable mutes on PGA inputs (aka INTPUT1)
    codec.disableLINMUTE();
    codec.disableRINMUTE();

    // Set pga volumes
    codec.setLINVOLDB(0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)
    codec.setRINVOLDB(0.00); // Valid options are -17.25dB to +30dB (0.75dB steps)

    // Set input boosts to get inputs 1 to the boost mixers
    codec.setLMICBOOST(WM8960_MIC_BOOST_GAIN_0DB);
    codec.setRMICBOOST(WM8960_MIC_BOOST_GAIN_0DB);

    // Connect from MIC inputs (aka pga output) to boost mixers
    codec.connectLMIC2B();
    codec.connectRMIC2B();

    // Enable boost mixers
    codec.enableAINL();
    codec.enableAINR();

    // Connect LB2LO (booster to output mixer (analog bypass)
    codec.enableLB2LO();
    codec.enableRB2RO();

    // config mic input end
#endif

    // Connect from DAC outputs to output mixer
    codec.enableLD2LO();
    codec.enableRD2RO();

    // Enable output mixers
    codec.enableLOMIX();
    codec.enableROMIX();

    // CLOCK STUFF, These settings will get you 44.1KHz sample rate, and class-d
    // freq at 705.6kHz
    codec.enablePLL(); // Needed for class-d amp clock
    codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
    codec.setSMD(WM8960_PLL_MODE_FRACTIONAL);
    codec.setCLKSEL(WM8960_CLKSEL_PLL);
    codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
    codec.setBCLKDIV(4);
    codec.setDCLKDIV(WM8960_DCLKDIV_16);
    codec.setPLLN(7);
    codec.setPLLK(0x86, 0xC2, 0x26); // PLLK=86C226h
    codec.setWL(WM8960_WL_16BIT);

    codec.enablePeripheralMode();

    codec.enableAdcLeft();
    codec.enableAdcRight();
    codec.enableDacLeft();
    codec.enableDacRight();

    codec.disableDacMute();   // Default is "soft mute" on, so we must disable mute to make channels active
    //codec.enableLoopBack(); // Loopback sends ADC data directly into DAC
    codec.disableLoopBack();

    codec.enableSpeakers();

    codec.setDacLeftDigitalVolume(255);
    codec.setDacRightDigitalVolume(255);

    codec.setSpeakerVolumeDB(0.00);
    codec.setSpeakerDcGain(5);
    codec.setSpeakerAcGain(5);

    codec.enableHeadphones();
    codec.setHeadphoneVolumeDB(0.00);

    // headphone jack detection
    codec.enableHeadphoneJackDetect();
    codec.setHeadphoneJackDetectInput(WM8960_JACKDETECT_LINPUT3);
	}

#endif
#ifdef CONFIG_TLV320AIC3100
#pragma message ("TLV320AIC3100 CODEC")
    Wire.begin(CONFIG_TLV320AIC3100_SDA, CONFIG_TLV320AIC3100_SCL);
    if (!codec.begin()) {
      Serial.println("ERROR: TLV320AIC3100 Codec didn't respond via I2C!");
    } else {
      codec.setWordLength(AIC31XX_WORD_LEN_16BITS);

      // clock configuration for 44,1kHz 16bit stereo, the master clock will be derived via PLL from the i2s BCLK
      codec.setCLKMUX(AIC31XX_PLL_CLKIN_BCLK, AIC31XX_CODEC_CLKIN_PLL);
      codec.setPLL(1, 2, 32, 0); // uint8_t pll_p, uint8_t pll_r, uint8_t pll_j, uint16_t pll_d
      codec.setNDACVal(8);
      codec.setNDACPower(true);
      codec.setMDACVal(2);
      codec.setMDACPower(true);
      codec.setNADCVal(8);
      codec.setNADCPower(true);
      codec.setMADCVal(2);
      codec.setMADCPower(true);
      codec.setPLLPower(1);
      delay(20); // let PLL settle

      codec.setMicPGAEnable(true);
      codec.writeRegister(AIC31XX_MICPGAPI, 0x10); // MIC1RP 10k

      // enable internal clock for timer
      // this is required as we derive master clock from BCLK and hence have no MCLK input
      codec.modifyRegister(AIC31XX_TIMERDIVIDER, AIC31XX_TIMER_SELECT_MASK, 0);
      codec.modifyRegister(AIC31XX_TIMERDIVIDER, 0x3F, 0x10);

      // enable headset detection and trigger interrupt 1 for headset events
      codec.enableHeadsetDetect();
      codec.setHSDetectInt1(true);

      // codec.writeRegister(AIC31XX_MICBIAS,11);
      codec.enableDAC();
      codec.setDACMute(false);
      // Unity gain: the sidetone already arrives close to full scale (see
      // sidetone.setVolume(0.7) below), and actual loudness is set downstream
      // by the analog headphone/speaker stage (setHeadphoneVolume/setSpeakerVolume,
      // driven by the Tone Volume preference). +20 dB of digital gain here was
      // clipping every tone internally, before it ever reached that analog stage
      // -- audible as a harsh, harmonic-heavy tone regardless of Tone Volume.
      // +2 dB: the last bit of headroom before the 0.7-scaled sidetone would
      // start clipping again (20*log10(1/0.7) ~= 3.1 dB ceiling).
      codec.setDACVolume(2.0f,2.0f);
      codec.enableADC();
      codec.setADCGain(-12.0f);
      // Enable the speaker and then run soundEventHandler once to mute/unmute HP/Spk depending on HS plug state
	  soundEnableSpeaker();
    delay(5);
    soundEventHandler();
      // codec.dumpRegisters(); // nifty when debugging codec issues
    }
#endif
  sidetone.begin(44100,16,2,128); //  defaults to 44100, 16, 2, 32
  sidetone.setFrequency(600.0);
#ifdef CONFIG_TLV320AIC3100
  // The library's begin() leaves the shared post-mixer VolumeStream at 0.8, and only
  // pwmTone() drops it to the 0.7 this path assumes -- so anything played BEFORE the
  // first tone ran 1.2 dB hotter than everything after it. That is not hypothetical:
  // the accessibility edition's boot announcement is the first sound of every session.
  // Set it here so MP3 clips and the sidetone share one level from the first sample on.
  sidetone.setVolume(SIDETONE_LEVEL);
#endif
#endif
}

#ifdef CONFIG_SOUND_I2S
void MorseOutput::setSidetoneEnvelope(uint8_t prefValue) {   // prefValue 0..8 maps to 1..9 ms attack/release
  float t = (prefValue + 1) / 1000.0f;
  sidetone.setADSR(t, 0.0f, 1.0f, t);
}
#endif

void MorseOutput::soundSuspend()
{
#ifdef CONFIG_WM8960
  codec.reset();
  delay(10);
  codec.disableVREF();
#endif
#ifdef CONFIG_TLV320AIC3100
  codec.reset();
#endif
}

void MorseOutput::pwmTone(unsigned int frequency, unsigned int volume, boolean lineOut) { // frequency in Hertz, volume in range 0 - 19; we use 10 bit resolution
#ifndef CONFIG_SOUND_I2S
  const uint16_t vol[] =   {0,  1, 2, 4, 6, 9, 14, 21, 31, 45, 70, 100, 140, 200, 280, 390, 512, 680, 840, 1023}; // 20 values
  unsigned int i = constrain(volume, 0, 19);
  unsigned int j = vol[i] >> 8;     // experimental: soften the inital click
  unsigned int jj = vol[i] >> 3;
  //DEBUG(String(vol[i]));
  //DEBUG(String(frequency));
  if (lineOut) {
      ledcWriteTone(lineOutChannel, (double) frequency);
      ledcWrite(lineOutChannel, dutyCycleFiftyPercent);
  }

  ledcWrite(volChannel, 0);

  if (i == 0 ) {
      ledcWrite(toneChannel, dutyCycleZero);
  }
  else  {
  ledcWrite(toneChannel, dutyCycleFiftyPercent);
  ledcWriteTone(toneChannel, frequency);
  }

  //ledcWrite(volChannel, volFreq);
  ledcWrite(volChannel, j);       // experimental: soften the inital click
  delay(3);                       // experimental: soften the inital click
  ledcWrite(volChannel, jj);       // experimental: soften the inital click
  delay(3);
  ledcWrite(volChannel, vol[i]);

#else
  sidetone.setFrequency(frequency);
  //soundSetVolume(MorsePreferences::sidetoneVolume);
#ifdef CONFIG_TLV320AIC3100
  sidetone.setVolume(SIDETONE_LEVEL);
#else
  sidetone.setVolume(float(volume) / 19.0);
#endif
  sidetone.on();
  delay(6);
#endif
}


void MorseOutput::pwmNoTone(unsigned int volume) {      // stop playing a tone by changing duty cycle of the tone to 0
#ifndef CONFIG_SOUND_I2S
  const uint16_t vol[] =   {0,  1, 2, 4, 6, 9, 14, 21, 31, 45, 70, 100, 140, 200, 280, 390, 512, 680, 840, 1023}; // 20 values
  unsigned int i = constrain(volume, 0, 19);
  unsigned int j = vol[i] >> 8;     // experimental: soften the inital click
  unsigned int jj = vol[i] >> 3;

  //ledcWrite(toneChannel, 450);
  //ledcWrite(lineOutChannel, 450);
  ledcWrite(volChannel, jj);         // experimental: soften the click
  delay(3);
  ledcWrite(volChannel, j);         // experimental: soften the click
  delay(3);

  ledcWrite(volChannel, 0);

  ledcWrite(toneChannel, dutyCycleZero);
  ledcWrite(lineOutChannel, dutyCycleZero);

#else
  delay(6);
  sidetone.off();
#endif
}


void MorseOutput::pwmClick(unsigned int volume) {                        /// generate a click on the speaker
    
    if (!MorsePreferences::pliste[posClicks].value)
      return;
    #ifdef CONFIG_SOUND_I2S
      uint8_t v;
      if (MorsePreferences::sidetoneVolume > 14)
        v = MorsePreferences::sidetoneVolume -4;
      else if (MorsePreferences::sidetoneVolume > 5)
        v = MorsePreferences::sidetoneVolume -2;
      else
        v = MorsePreferences::sidetoneVolume +2;
#ifdef CONFIG_TLV320AIC3100
      soundSetVolume(v);
#endif
      pwmTone(572,v,false);
      delay(3);
      pwmNoTone(v);
      delay(2);
      pwmTone(1144,v,false);
      delay(6);
      pwmNoTone(v);
#ifdef CONFIG_TLV320AIC3100
      soundSetVolume(MorsePreferences::sidetoneVolume);
#endif
    #else
    pwmTone(572,volume > 4 ? volume-4 : volume, false);
    delay(3);
    //pwmTone(286,volume, false);
    pwmTone(1144,volume > 3 ? volume-3 : volume, false);

    delay(6);
    //pwmTone(143,volume-4, false);
    //delay(7);           
    pwmNoTone(volume);
    #endif
}

// Switches the tone generator to the rich (3rd/5th/7th harmonic) timbre for the OK/ERR
// signals and puts it back to a pure sine on the way out, however the block is left. The
// CW sidetone must always be a clean sine -- never leave this on.
#ifdef CONFIG_SOUND_I2S
struct RichTone {
    RichTone()  { sidetone.setRichTimbre(true);  }
    ~RichTone() { sidetone.setRichTimbre(false); }
};
#endif

void MorseOutput::soundSignalOK() {
#ifndef CONFIG_SOUND_I2S
    pwmTone(440, MorsePreferences::sidetoneVolume, false);
    delay(97);
    pwmNoTone(MorsePreferences::sidetoneVolume);
    pwmTone(587, MorsePreferences::sidetoneVolume, false);
    delay(193);
    pwmNoTone(MorsePreferences::sidetoneVolume);
#else
    if (! sidetone.playSPIFFSFile("/sounds/success.mp3")) {
        // Rising, a fifth above the classic pair (440/587 -> 660/880). See soundSignalERR().
        RichTone rich;
        pwmTone(660, MorsePreferences::sidetoneVolume, false);
        delay(97);
        pwmNoTone(MorsePreferences::sidetoneVolume);
        delay(20);
        pwmTone(880, MorsePreferences::sidetoneVolume, false);
        delay(193);
        pwmNoTone(MorsePreferences::sidetoneVolume);
    }
#endif
}


void MorseOutput::soundSignalERR() {
#ifndef CONFIG_SOUND_I2S
  pwmTone(366, MorsePreferences::sidetoneVolume, false);
  delay(97);
  pwmNoTone(MorsePreferences::sidetoneVolume);
  pwmTone(330, MorsePreferences::sidetoneVolume, false);
  delay(193);
  pwmNoTone(MorsePreferences::sidetoneVolume);
#else
  if (! sidetone.playSPIFFSFile("/sounds/error.mp3")) {
     // FALLING, like the classic M32 above -- this branch used to rise (311 -> 330), the
     // same direction as the OK signal, which is exactly backwards for an error. Restored
     // to the classic's interval (366 -> 330, a whole tone down, short then long), a fifth
     // up: 549 -> 495. The fifth and the rich timbre are both about audibility, not taste:
     // as a pure sine at 311 Hz this signal measured ~10 dB below the CW sidetone on the
     // Pocket's speaker, because de-clipping the codec (PR #208) removed the harmonics it
     // had been relying on. Together they win back ~7 dB without raising the pitch further.
     RichTone rich;
     pwmTone(549, MorsePreferences::sidetoneVolume, false);
     delay(97);
     pwmNoTone(MorsePreferences::sidetoneVolume);
     delay(20);
     pwmTone(495, MorsePreferences::sidetoneVolume, false);
     delay(193);
     pwmNoTone(MorsePreferences::sidetoneVolume);
  }
#endif
}

// V9.0 audio accessibility: thin wrappers over the sidetone library's async clip API.
// Non-blocking; the firmware polls voiceService() from its loops (see MorseVoice). All
// pipeline work (file open/close, mixer routing, per-clip decoder reset) happens inside
// the library's audio task -- the firmware never touches the decode pipeline directly.
bool MorseOutput::voiceStart(const char* path) {
#ifdef CONFIG_SOUND_I2S
    return sidetone.startClip(path);
#else
    (void)path; return false;
#endif
}
bool MorseOutput::voiceService() {
#ifdef CONFIG_SOUND_I2S
    return sidetone.serviceClip();
#else
    return false;
#endif
}
void MorseOutput::voiceStop() {
#ifdef CONFIG_SOUND_I2S
    sidetone.stopClip();
#endif
}