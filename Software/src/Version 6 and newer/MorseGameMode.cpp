/******************************************************************************************************************************
 *  MorseGameMode implementation. See MorseGameMode.h for design notes.
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

    // Defensive: if a previous game session somehow left a sprite alive, free
    // it before allocating again. The normal flow always calls exit() first,
    // so this should be a no-op in practice.
    if (sprite) {
      sprite->deleteSprite();
      delete sprite;
      sprite = nullptr;
    }

    sprite = new LGFX_Sprite(lcd);
    if (!sprite) {
      restoreMenuDisplay(leftHanded);
      return nullptr;
    }
    sprite->setPsram(false);
    sprite->setColorDepth(16);
    if (!sprite->createSprite(lcd->width(), lcd->height())) {
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
  // Free the sprite immediately so the heap doesn't carry the ~106 KB
  // allocation through menu time. This is the change that prevents
  // fragmentation-driven OOM when starting another game later.
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
