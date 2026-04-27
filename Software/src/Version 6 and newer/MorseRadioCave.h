#ifndef MORSERADIOCAVE_H_
#define MORSERADIOCAVE_H_

/******************************************************************************************************************************
 *  Radio Cave — Text adventure game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  A text adventure inspired by Colossal Cave Adventure. The player explores
 *  an abandoned Cold War radio station hidden in a mountain cave, repairs
 *  the equipment, and completes a QSO with a remote station (IR7) that has
 *  been calling for 50+ years. All commands are keyed as Morse code.
 *****************************************************************************************************************************/

#include <Arduino.h>

#ifdef CONFIG_CW_GAME

// ---- Screen layout (320x170 landscape) ----
// Radio Cave runs in landscape because text adventures need wide lines more
// than tall columns. The layout is: top bar (icon + room name, 30 px), main
// text area (120 px, scrollable), bottom bar (exits + steps + inventory, 20 px).
#define RC_SCREEN_W       320
#define RC_SCREEN_H       170

#define RC_TOPBAR_H        30          // icon + room name
#define RC_BOTBAR_H        20          // exits + steps + inventory
#define RC_MAIN_TOP       (RC_TOPBAR_H + 2)
#define RC_MAIN_BOTTOM    (RC_SCREEN_H - RC_BOTBAR_H - 2)
#define RC_MAIN_H         (RC_MAIN_BOTTOM - RC_MAIN_TOP)

// ---- Colour palette (RGB565) ----
// All foreground colors tested for readability on near-black background.
#define RC_BG             0x0841       // dark near-black
#define RC_TEXT           0xFFFF       // white — body text
#define RC_DIM            0xBDF7       // light grey — secondary/hints
#define RC_ACCENT         0x07FF       // cyan — headings, room names
#define RC_HIGHLIGHT      0xFFE0       // yellow — CW playing, warnings
#define RC_DANGER         0xF800       // red — death, errors
#define RC_OK             0x07E0       // green — success feedback
#define RC_FRAME          0x528A       // mid-blue — decorative frames (visible on black)

// ---- Save format ----
#define RC_SAVE_VERSION   1            // bump if save-blob layout changes

// ---- CW clue defaults ----
#define RC_CLUE_WPM          30        // step-2 test clue speed
#define RC_CLUE_QRS_DIVISOR  2         // QRS halves the clue speed
#define RC_CLUE_PITCH_HZ    696        // on-theme pitch for CW clues (distinct from keyer sidetone)

// ---- Command buffer ----
#define RC_INPUT_MAX        32         // max accumulated characters in one command
                                       // (must fit the longest QSO phrase:
                                       // "de de rc0 rc0 qsl qsl K K" = 25 chars)
#define RC_TIMEOUT_DITS      7         // silence after keyer idle (in dit-lengths)
                                       // 7 dits = one CW inter-word gap.
                                       // A 300 ms floor is applied in code so
                                       // very-fast keyers still have reaction time.

// ---- World model ----
#define RC_NUM_ROOMS        12         // Forest Path..The Safe
#define RC_NUM_ITEMS         6         // FUEL, MANUAL, MIC, KEY, CHOKE, TUBE
#define RC_INV_MAX           2         // max items carried at once

// Item location sentinel values (normal values are room numbers 1..12)
#define RC_ITEM_CARRIED    100         // in player's inventory
#define RC_ITEM_GONE       101         // consumed / destroyed / installed

// Item identifiers (0 = "no item" / empty slot)
enum RcItem : uint8_t {
    RC_NONE   = 0,
    RC_FUEL   = 1,
    RC_MANUAL = 2,
    RC_MIC    = 3,
    RC_KEY    = 4,
    RC_CHOKE  = 5,
    RC_TUBE   = 6
};

// Direction indices into Room::exits[]
enum RcDir : uint8_t { RC_N = 0, RC_E = 1, RC_S = 2, RC_W = 3 };

// Room descriptor (static data in PROGMEM)
struct Room {
    const char* shortName;             // "Main Corridor"
    int8_t      exits[4];              // room # (1..12) or -1 (no exit)
};

// Item descriptor (static data)
struct Item {
    const char* name;                  // "FUEL" — matched by prefix
    const char* takeName;              // "fuel canister" — used in "You take the X."
    const char* examine;               // shown on LOOK <item>
};

// ---- Game states ----
enum RcState : uint8_t {
    RC_INIT,
    RC_LOBBY,           // entry lobby — controls reminder, press to start
    RC_PLAYING,         // the game proper
    RC_DEAD,            // high-voltage death, offer restart
    RC_WON,             // victory screen
    RC_EXIT
};

// ---- Public interface ----
namespace MorseRadioCave {
    void run();
}

#endif  // CONFIG_CW_GAME
#endif  // MORSERADIOCAVE_H_