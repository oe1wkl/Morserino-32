/******************************************************************************************************************************
 *  MorseGameMode — owns the off-screen sprite and rotation state used by the M32 Pocket games.
 *
 *  Previously these were owned by DisplayWrapper, which kept both a portrait and a landscape sprite
 *  alive for the device's lifetime — ~106 KB each — leading to OOM when starting Radio Cave after
 *  Morse Invaders or Fight the Pileup. This module allocates the sprite fresh on game entry and
 *  frees it on exit, so the heap stays clean during menu time.
 *
 *  Only built when CONFIG_DISPLAYWRAPPER is defined (M32 Pocket / TFT boards).
 *****************************************************************************************************************************/

#pragma once

#ifdef CONFIG_DISPLAYWRAPPER

#include <LovyanGFX.hpp>

namespace MorseGameMode {

  // Allocate a fresh portrait-orientation game sprite (panel-sized).
  // Sets the panel rotation and clears the screen. Returns the sprite to draw
  // into, or nullptr on allocation failure (the menu's display state is
  // automatically restored before nullptr is returned).
  LGFX_Sprite *enterPortrait(bool leftHanded);

  // Allocate a fresh landscape-orientation game sprite.
  LGFX_Sprite *enterLandscape(bool leftHanded);

  // Push the sprite to the panel.
  void pushFrame();

  // Free the sprite and restore the Morserino menu's display state:
  // portrait rotation, MorseOutput::initDisplay(), theme, and rotary-encoder
  // pin handoff. Call this once at the end of every game's run().
  void exit();

  // Currently-allocated sprite, or nullptr if not in game mode.
  LGFX_Sprite *getSprite();

} // namespace MorseGameMode

#endif // CONFIG_DISPLAYWRAPPER
