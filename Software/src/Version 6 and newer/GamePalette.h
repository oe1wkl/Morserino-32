/******************************************************************************************************************************
 *  GamePalette — shared 8-bpp colour palette for the M32 Pocket game sprite.
 *
 *  Games that use an 8-bpp (palette) game sprite refer to colours by the
 *  indices in GamePaletteIndex below. MorseGameMode populates the sprite's
 *  palette from the matching RGB565 table (kGamePalette[] in
 *  MorseGameMode.cpp) — the enum order and that table MUST stay in sync.
 *
 *  Why indexed colour
 *  ------------------
 *  An 8-bpp sprite is half the size of the 16-bpp one (~50 KB vs ~99 KB at
 *  the current trim), which removes the heap pressure that previously made
 *  the sprite and WiFi/ESP-NOW compete for the same large contiguous block.
 *
 *  LovyanGFX's 8-bpp mode is *pure* indexed colour: the colour argument to
 *  any drawing call is masked to its low byte and used directly as a
 *  palette index (there is no nearest-colour matching). So drawing calls on
 *  an 8-bpp sprite must pass these PAL_* indices, never RGB565 values. The
 *  palette entries hold the exact RGB565 values the games used before, so
 *  the on-screen result is pixel-identical to the old 16-bpp sprite.
 *****************************************************************************************************************************/

#pragma once

#ifdef CONFIG_TFT

#include <cstdint>

enum GamePaletteIndex : uint8_t {
  PAL_BLACK = 0,    // 0x0000  true black (sprite clear / LCD letterbox match)
  PAL_BG,           // 0x0841  dark near-black background
  PAL_WHITE,        // 0xFFFF  body / HUD text
  PAL_RED,          // 0xF800  red — warnings, lives, danger, wrong letter
  PAL_CYAN,         // 0x07FF  cyan — accents, headings, score
  PAL_YELLOW,       // 0xFFE0  yellow — highlights, titles, level-up
  PAL_GREEN,        // 0x07E0  green — success / OK / correct
  PAL_GREY,         // 0x7BEF  mid grey — dim text, miss
  PAL_LIGHTGREY,    // 0xBDF7  light grey — secondary text / hints
  PAL_MIDBLUE,      // 0x528A  mid-blue — frames, revealed-wrong slot
  PAL_BLUE,         // 0x001F  blue — input text
  PAL_MAGENTA,      // 0xF81F  magenta — attack prompt
  PAL_DARKGREY,     // 0x2104  dark grey — status/footer band

  // Morse Invaders character-category colours: each invader is filled in a
  // category colour and outlined in a dimmed version of it. The border
  // values are the exact result of the old runtime brightening
  // (color565(R5*4, G6*2, B5*4)) precomputed, since palette indices can't
  // be brightened arithmetically.
  PAL_INV_LETTERS,    // 0x0400  dark green   (fill)
  PAL_INV_NUMBERS,    // 0x8400  dark yellow  (fill)
  PAL_INV_PUNCT,      // 0x8200  dark orange  (fill)
  PAL_INV_PROSIGN,    // 0x8010  dark magenta (fill)
  PAL_INV_LETTERS_B,  // 0x0200  letters border
  PAL_INV_NUMBERS_B,  // 0x4200  numbers border
  PAL_INV_PUNCT_B,    // 0x4100  punct border
  PAL_INV_PROSIGN_B,  // 0x4008  prosign border

  PAL_COUNT         // number of palette entries in use
};

#endif // CONFIG_TFT
