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
  LGFX_Sprite *enterPortrait(bool leftHanded);

  // Allocate a fresh landscape-orientation game sprite. Same semantics as
  // enterPortrait — reboots on allocation failure.
  LGFX_Sprite *enterLandscape(bool leftHanded);

  // Push the sprite to the panel.
  void pushFrame();

  // Free the sprite and restore the Morserino menu's display state:
  // portrait rotation, MorseOutput::initDisplay(), theme, and rotary-encoder
  // pin handoff. Call this once at the end of every game's run().
  void exit();

  // Currently-allocated sprite, or nullptr if not in game mode.
  LGFX_Sprite *getSprite();

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
