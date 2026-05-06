/******************************************************************************************************************************
 *  MorseGameMode implementation. See MorseGameMode.h for design notes.
 *
 *  Sprite buffer strategy
 *  ----------------------
 *  The sprite pixel buffer (~108 KB at 170×320 / 320×170, 16-bit) is a
 *  static array in BSS, declared at compile time and never touched by the
 *  heap allocator. Both portrait and landscape game modes share the same
 *  buffer — they have identical pixel counts on the M32 Pocket panel; only
 *  the W×H interpretation handed to LGFX differs, and `fillSprite(BLACK)`
 *  at game entry resets layout state.
 *
 *  This is what fixes the Radio-Cave-after-Invaders OOM. Heap-based
 *  allocation strategies (whether kept-alive or freed-on-exit) all failed
 *  in practice: a separate persistent ~48 KB allocation made during the
 *  first game session permanently fragmented the largest contiguous free
 *  block, leaving it ~2 KB short of what the next 108,800-byte sprite
 *  needed. Putting the buffer in BSS sidesteps the heap entirely.
 *
 *  We use `LGFX_Sprite::setBuffer()` instead of `createSprite()` to hand
 *  LGFX our static buffer. setBuffer() marks the SpriteBuffer's source as
 *  Preallocated, which makes its release() skip the heap_free() — so
 *  ~LGFX_Sprite() is safe and the static buffer is undisturbed across
 *  game sessions.
 *
 *  Trade-off: ~108 KB of internal SRAM (~21% of the chip's 512 KB)
 *  permanently reserved.
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

  // Static sprite buffer. Lives in BSS — allocated at compile time, never
  // freed at runtime, immune to heap fragmentation. Sized for the panel's
  // pixel count at 16-bit colour. Portrait and landscape modes share it;
  // both have the same total pixel count.
  alignas(4) uint16_t spriteBuffer[TFT_WIDTH * TFT_HEIGHT];

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

    // Defensive: dispose of any previous sprite wrapper. The static buffer
    // is shared and remains intact; only the small LGFX_Sprite object is
    // freed.
    if (sprite) {
      delete sprite;
      sprite = nullptr;
    }

    sprite = new LGFX_Sprite(lcd);
    if (!sprite) {
      restoreMenuDisplay(leftHanded);
      return nullptr;
    }
    sprite->setColorDepth(16);
    sprite->setBuffer(spriteBuffer, lcd->width(), lcd->height(), 16);
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
  // Dispose of the small LGFX_Sprite wrapper. The underlying buffer is
  // Preallocated (static), so SpriteBuffer::release() inside ~LGFX_Sprite()
  // skips the heap_free(), leaving the static buffer untouched and ready
  // for the next game session.
  if (sprite) {
    delete sprite;
    sprite = nullptr;
  }
  restoreMenuDisplay(lastLeftHanded);
}

LGFX_Sprite *MorseGameMode::getSprite() {
  return sprite;
}

#endif // CONFIG_DISPLAYWRAPPER
