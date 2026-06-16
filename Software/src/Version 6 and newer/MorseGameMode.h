/******************************************************************************************************************************
 *  MorseGameMode — owns the off-screen sprite and rotation state used by the M32 Pocket games.
 *
 *  Previously these were owned by DisplayWrapper, which kept both a portrait and a landscape sprite
 *  alive for the device's lifetime — ~106 KB each — leading to OOM when starting Radio Cave after
 *  Morse Invaders or Fight the Pileup. This module allocates the sprite fresh on game entry and
 *  frees it on exit, so the heap stays clean during menu time.
 *
 *  Only built when CONFIG_TFT is defined (M32 Pocket / TFT boards).
 *****************************************************************************************************************************/

#pragma once

#ifdef CONFIG_TFT

#include <LovyanGFX.hpp>

namespace MorseGameMode {

  // Call once at boot, after MorseOutput::initDisplay(). Pushes a
  // panel-sized throwaway sprite so LovyanGFX does its first-time
  // SPI/DMA allocation NOW, while the heap is empty, instead of inside
  // the first game session. Without this, the lazy alloc lands adjacent
  // to the game sprite and the freed sprite region can't merge back —
  // causing fragmentation that builds up across game cycles.
  void warmup();

  // Allocate a fresh portrait-orientation game sprite (panel-sized minus
  // a small trim — see MORSE_GAMEMODE_SPRITE_TRIM in the implementation).
  // Sets the panel rotation and clears the screen, then returns the sprite
  // to draw into.
  //
  // Never returns nullptr in practice: on allocation failure, instead of
  // failing back to the caller, this triggers a memory-clearing reboot
  // (saves the user's current menu pointer to RTC and calls ESP.restart()).
  // setup() then auto-resumes into the same menu item with a defragmented
  // heap. Callers may still keep a defensive `if (!canvas) return;` for
  // robustness, but it should never fire.
  //
  // colorDepth selects the sprite's bits-per-pixel: 16 (RGB565, the
  // default) or 8 (indexed palette — see GamePalette.h). An 8-bpp sprite
  // is half the size; games that opt in must draw using PAL_* palette
  // indices instead of RGB565 values.
  LGFX_Sprite *enterPortrait(bool leftHanded, uint8_t colorDepth = 16);

  // Allocate a fresh landscape-orientation game sprite. Same semantics as
  // enterPortrait — reboots on allocation failure.
  LGFX_Sprite *enterLandscape(bool leftHanded, uint8_t colorDepth = 16);

  // Load the shared 8-bpp game palette (GamePalette.h) into a
  // caller-owned sprite. Used by games that allocate their own secondary
  // 8-bpp sprite (e.g. Invaders' rotated-text tile) and need it to share
  // the main sprite's palette so blits between the two stay colour-correct.
  void applyGamePalette(LGFX_Sprite *s);

  // Push the sprite to the panel.
  void pushFrame();

  // Free the sprite and restore the Morserino menu's display state:
  // portrait rotation, MorseOutput::initDisplay(), theme, and rotary-encoder
  // pin handoff. Call this once at the end of every game's run().
  void exit();

  // Currently-allocated sprite, or nullptr if not in game mode.
  LGFX_Sprite *getSprite();

  // Centre a single line of text horizontally at (centreX, y) on a game
  // sprite, with the given fg/bg colours and optional font. Shared helper —
  // was duplicated verbatim as drawCentredText() in MorseGame.cpp and
  // MorsePileup.cpp; callers pass their own centre-x and background so it
  // stays game-agnostic.
  void drawCentred(LGFX_Sprite *s, int centreX, int y, const char *text,
                   uint16_t color, uint16_t bg, const lgfx::IFont *font = nullptr);

  // Force a memory-clearing reboot. Saves MorsePreferences::menuPtr to RTC,
  // shows a brief "Clearing memory..." overlay, then ESP.restart()s. After
  // the reset, setup() detects the RTC magic and auto-resumes directly
  // into the saved menu item. Used by:
  //   - this module's allocate() on createSprite failure;
  //   - MorseMenu's WiFi entry path on the 2nd-and-subsequent WiFi Trx
  //     entry in one boot session (QuickEspNow can't survive multiple
  //     WiFi.mode(STA)/WIFI_OFF cycles cleanly).
  // Does not return.
  [[noreturn]] void triggerMemoryClearingReboot();

} // namespace MorseGameMode

#endif // CONFIG_TFT
