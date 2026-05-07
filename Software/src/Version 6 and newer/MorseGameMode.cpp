/******************************************************************************************************************************
 *  MorseGameMode implementation. See MorseGameMode.h for design notes.
 *
 *  Sprite buffer strategy
 *  ----------------------
 *  Allocated dynamically from the heap on game enter and freed on game
 *  exit. (An earlier version of this module reserved the buffer in BSS,
 *  which fixed an OOM at the cost of permanently consuming ~108 KB of
 *  internal SRAM. That broke ESP-NOW and BT, which need the same
 *  internal-SRAM pool — those are time-multiplexed with games, so dynamic
 *  allocation is the right policy: heap full when WiFi/BT/ESP-NOW need
 *  it, sprite-sized only while a game is actually running.)
 *
 *  Sprite size
 *  -----------
 *  We allocate slightly less than the full panel — the long side is
 *  reduced by SPRITE_TRIM pixels — so the sprite always fits inside the
 *  largest contiguous heap block we observe after a game session has
 *  fragmented the heap (~106 KB on the M32 Pocket). The trimmed strip at
 *  the bottom (portrait) or right (landscape) of the panel stays the
 *  black `lcd.fillScreen` painted at game enter; games are responsible
 *  for laying out their UI within the sprite area.
 *****************************************************************************************************************************/

#include "MorseGameMode.h"

#ifdef CONFIG_DISPLAYWRAPPER

#include <Arduino.h>
#include <ESP32Encoder.h>

#include "DisplayWrapper.h"
#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "morsedefs.h"

extern ESP32Encoder rotaryEncoder;
extern const int    PinCLK;
extern const int    PinDT;

// Pixels removed from the long side of the panel when sizing the sprite.
// At 170×320, trimming 16 from the long side gives a 170×304 / 304×170
// sprite (= 103,360 bytes), comfortably under the ~106 KB largest free
// block we observe after one game session. Decrease this only after
// re-running the heap diagnostics on the heap-diagnostics branch and
// confirming there's enough headroom.
#ifndef MORSE_GAMEMODE_SPRITE_TRIM
#define MORSE_GAMEMODE_SPRITE_TRIM 16
#endif

namespace {

  LGFX_Sprite *sprite         = nullptr;
  bool         lastLeftHanded = false;

  // Restore the Morserino menu's display state. Used both by exit() and by
  // the failure path of allocate() so that callers see a coherent menu
  // regardless of whether enter*() succeeded.
  void restoreMenuDisplay(bool leftHanded) {
    DisplayWrapper::getLGFX()->setRotation(leftHanded ? 0 : 2);
    MorseOutput::initDisplay();
    MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
    pinMode(PinCLK, INPUT_PULLUP);
    pinMode(PinDT,  INPUT_PULLUP);
    rotaryEncoder.attachHalfQuad(PinDT, PinCLK);
    rotaryEncoder.setCount(0);
  }

  LGFX_Sprite *allocate(int rotation, bool leftHanded) {
    lastLeftHanded = leftHanded;

    auto *lcd = DisplayWrapper::getLGFX();
    lcd->setRotation(rotation);
    lcd->fillScreen(TFT_BLACK);

    // Defensive: free any prior sprite buffer. Normal flow always calls
    // exit() first, so this should be a no-op in practice.
    if (sprite) {
      sprite->deleteSprite();
      delete sprite;
      sprite = nullptr;
    }

    // Trim the long side; keep the short side at the panel's full width.
    int spriteW = lcd->width();
    int spriteH = lcd->height();
    if (spriteW >= spriteH) {
      spriteW -= MORSE_GAMEMODE_SPRITE_TRIM;
    } else {
      spriteH -= MORSE_GAMEMODE_SPRITE_TRIM;
    }

    sprite = new LGFX_Sprite(lcd);
    if (!sprite) {
      restoreMenuDisplay(leftHanded);
      return nullptr;
    }
    sprite->setPsram(false);
    sprite->setColorDepth(16);
    if (!sprite->createSprite(spriteW, spriteH)) {
      delete sprite;
      sprite = nullptr;
      restoreMenuDisplay(leftHanded);
      return nullptr;
    }
    sprite->fillSprite(TFT_BLACK);
    return sprite;
  }

} // namespace

LGFX_Sprite *MorseGameMode::enterPortrait(bool leftHanded) {
  return allocate(leftHanded ? 0 : 2, leftHanded);
}

LGFX_Sprite *MorseGameMode::enterLandscape(bool leftHanded) {
  return allocate(leftHanded ? 1 : 3, leftHanded);
}

void MorseGameMode::pushFrame() {
  if (!sprite) return;
  auto *lcd = DisplayWrapper::getLGFX();
  lcd->startWrite();
  sprite->pushSprite(lcd, 0, 0);
  lcd->endWrite();
}

void MorseGameMode::exit() {
  // Free the sprite immediately so the heap is released back to the
  // pool the moment the game exits — important so that re-entering the
  // menu (or starting WiFi/BT) sees a heap unencumbered by the sprite.
  if (sprite) {
    sprite->deleteSprite();
    delete sprite;
    sprite = nullptr;
  }
  restoreMenuDisplay(lastLeftHanded);
}

LGFX_Sprite *MorseGameMode::getSprite() {
  return sprite;
}

#endif // CONFIG_DISPLAYWRAPPER
