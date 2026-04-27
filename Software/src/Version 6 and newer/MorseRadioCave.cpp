/******************************************************************************************************************************
 *  Radio Cave — Text adventure game for Morserino-32 Pocket
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *  Programmed with assistance by Claude (Opus 4.x)
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  Overview
 *  ========
 *  A Colossal-Cave-inspired text adventure where the player explores an
 *  abandoned Cold War radio station, repairs the equipment, and completes
 *  a final QSO with a remote station — all by keying commands as Morse.
 *
 *  The game runs in landscape orientation on a 320×170 sprite (provided
 *  by DisplayWrapper::enterGameModeLandscape). Twelve rooms, six items,
 *  state-dependent room descriptions, CW clues delivered as audio, full
 *  inventory and puzzle logic, victory and death screens, and NVS-backed
 *  save/resume across reboots.
 *
 *  Display layout (top → bottom):
 *   - Top bar:    icon placeholder + room name
 *   - Main area:  room description / examine text / help, scrollable
 *   - Input line: current command buffer or last dispatched result
 *   - Hint line:  exits, inventory count, step counter, encoder mode
 *
 *  Controls (matching the main firmware conventions):
 *   - Paddles       → key a command (decoded by the firmware's keyer
 *                     into gameCharBuffer)
 *   - Encoder turn  → adjusts WPM, volume, or scroll position depending
 *                     on the current encoder mode
 *   - FN short      → cycle encoder mode: speed ↔ volume
 *   - FN long       → toggle scroll mode
 *   - Mode short    → unused in the game proper; click in the lobby to
 *                     enter the game
 *   - Mode long     → exit Radio Cave back to the main menu
 *
 *  Input parser conventions:
 *   - Instant dispatch on single letter S, E, W, I, H (truly unambiguous)
 *   - Timeout dispatch for everything else (silence ≈ 2.5 × interWordSpace)
 *   - Letters preserve case in the buffer; the firmware uses lowercase
 *     for regular letters and uppercase for prosigns (e.g. K = <sk>)
 *   - "eeee" or <hh> (8 dits as one letter) clears the accumulating buffer
 *
 *  Save / resume:
 *   - The full GameState is written to NVS (namespace "radiocave") after
 *     every dispatched command. The save is cleared on victory and death.
 *   - On entry the lobby auto-resumes if a valid save is present.
 *   - The NEW command (with Y confirmation) wipes the save and restarts.
 *****************************************************************************************************************************/

#include "MorseRadioCave.h"

#ifdef CONFIG_CW_GAME

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "MorseGame.h"           // gameMode, gameCharBuffer
#include "morsedefs.h"
#include "ClickButton.h"
#include "DisplayWrapper.h"

#include <LovyanGFX.hpp>
#include <ESP32Encoder.h>
#include <Preferences.h>

// ---- External references ----
extern int      checkEncoder();
extern void     checkShutDown(boolean);
extern void     serialEvent();
extern boolean  checkPaddles();
extern boolean  doPaddleIambic(boolean, boolean);
extern boolean  leftKey, rightKey;
extern void     clearPaddleLatches();
extern KEYERSTATES keyerState;
extern unsigned int ditLength;
extern unsigned int interCharacterSpace;
extern unsigned int interWordSpace;
extern void     updateTimings();
extern void     keyOut(boolean, boolean, int, int);
extern String   generateCWword(const String& symbols);   // m32_v6.ino — pure function
extern DisplayWrapper display;
extern ESP32Encoder   rotaryEncoder;
extern const int      PinCLK;
extern const int      PinDT;


//=============================================================================
// Module-level state
//=============================================================================

static LGFX_Sprite* canvas = nullptr;
static RcState      rcState = RC_INIT;

// What the rotary encoder is currently controlling.
enum RcEncMode : uint8_t { RC_ENC_SPEED, RC_ENC_VOLUME, RC_ENC_SCROLL };
static RcEncMode    encMode = RC_ENC_SPEED;
static RcEncMode    encModeBeforeScroll = RC_ENC_SPEED;   // where to return on scroll exit

//=============================================================================
// ROOM DATA
//=============================================================================
//
// Room numbers 1..12 match the prototype's numbering. Exits are stored as
// an array indexed by RcDir { N=0, E=1, S=2, W=3 }; -1 means "no exit that
// way". Room descriptions are generated at runtime by roomDescription()
// because several rooms have state-dependent text (generator on/off, door
// open/locked, etc.). On first entry to a room we show the long-form
// description; on revisits, a short summary.

static const Room ROOMS[RC_NUM_ROOMS + 1] = {
    // index 0 is unused (room numbering starts at 1)
    { "",                 { -1, -1, -1, -1 } },
    //   N   E   S   W
    { "Forest Path",      {  2, -1, -1, -1 } },     //  1
    { "Cave Entrance",    {  3, -1,  1, -1 } },     //  2
    { "Main Corridor",    {  9,  7,  2,  4 } },     //  3
    { "Generator Room",   { -1,  3,  5, -1 } },     //  4
    { "Storage Room",     {  4,  3,  6, -1 } },     //  5
    { "Workshop",         {  5,  3, -1, -1 } },     //  6
    { "Operating Room",   {  8, 11, -1,  3 } },     //  7  (E→11 opens only if officeOpen)
    { "Receiver Alcove",  { -1, -1,  7, -1 } },     //  8
    { "Antenna Tunnel",   { 10, -1,  3, -1 } },     //  9
    { "Antenna Platform", { -1, -1,  9, -1 } },     // 10
    { "Captain's Office", { -1, -1, -1,  7 } },     // 11
    { "The Safe",         { -1, -1, -1, -1 } }      // 12
};


//=============================================================================
// ITEM DATA
//=============================================================================
//
// Item 0 is unused (RC_NONE / "empty slot" sentinel). Items 1..6 map to the
// RcItem enum. Names are matched by prefix (case-insensitive): "F"→FUEL,
// "MA"→MANUAL, "MI"→MIC, etc. "M" alone is ambiguous (MANUAL vs MIC).

static const Item ITEMS[RC_NUM_ITEMS + 1] = {
    { "",       "",                 "" },                        // 0 unused
    { "FUEL",   "fuel canister",
      "A red metal fuel canister. Sloshes when shaken." },        // 1
    { "MANUAL", "operating manual",
      "The RC0 station operating manual. Thick, dusty." },        // 2
    { "MIC",    "microphone",
      "A vintage desk microphone. The connector is "
      "incompatible with anything in this station." },            // 3
    { "KEY",    "brass key",
      "A small, heavy brass key. Warm to the touch." },           // 4
    { "CHOKE",  "choke coil",
      "A wire-wound RF choke. Its solder joint is "
      "blackened and cracked. Needs repair." },                   // 5
    { "TUBE",   "6146 tube",
      "A pristine 6146 power tetrode wrapped in oilcloth. "
      "Its getter flash is a silver mirror." }                    // 6
};


//=============================================================================
// GAME STATE
//=============================================================================

struct GameState {
    uint8_t  room;                      // current room (1..12)
    uint32_t steps;                     // step counter
    uint16_t visited;                   // bitmask of rooms visited (bits 1..12)

    // Puzzle progression flags:
    bool generator;
    bool antennaGrounded;
    bool officeOpen;
    bool safeOpen;
    bool chokeRepaired;
    bool chokeInstalled;
    bool tubeInstalled;
    bool masterPower;
    bool txMode;
    uint8_t wallLookCount;
    uint8_t qsoStage;
    uint8_t spareTubes;

    // Where each item currently is. Indexed by RcItem (slot 0 unused).
    // Values: a room number 1..12 → item is lying in that room
    //         RC_ITEM_CARRIED     → item is in the player's inventory
    //         RC_ITEM_GONE        → item has been consumed / installed
    uint8_t itemLoc[RC_NUM_ITEMS + 1];

    // Inventory is derived from itemLoc: slots listing which items are
    // carried, for fast display and the 4-item limit. Rebuilt from itemLoc
    // whenever items change via rebuildInventory().
    uint8_t inv[RC_INV_MAX];
    uint8_t invCount;
};
static GameState game;

static void rebuildInventory() {
    game.invCount = 0;
    for (int i = 0; i < RC_INV_MAX; i++) game.inv[i] = 0;
    for (uint8_t it = 1; it <= RC_NUM_ITEMS && game.invCount < RC_INV_MAX; it++) {
        if (game.itemLoc[it] == RC_ITEM_CARRIED) {
            game.inv[game.invCount++] = it;
        }
    }
}

static void newGame() {
    game.room = 1;
    game.steps = 0;
    game.visited = 0;
    game.generator       = false;
    game.antennaGrounded = true;
    game.officeOpen      = false;
    game.safeOpen        = false;
    game.chokeRepaired   = false;
    game.chokeInstalled  = false;
    game.tubeInstalled   = false;
    game.masterPower     = false;
    game.txMode          = false;
    game.wallLookCount   = 0;
    game.qsoStage        = 0;
    game.spareTubes      = 2;

    // Initial item placement (matches prototype's newState)
    game.itemLoc[RC_FUEL]   = 5;   // Storage Room
    game.itemLoc[RC_MANUAL] = 5;   // Storage Room
    game.itemLoc[RC_MIC]    = 5;   // Storage Room
    game.itemLoc[RC_KEY]    = 4;   // Generator Room (but hidden until generator runs)
    game.itemLoc[RC_CHOKE]  = 7;   // Operating Room (on the transmitter rack)
    game.itemLoc[RC_TUBE]   = 12;  // inside The Safe (unreachable until safe opens)

    rebuildInventory();
}


//=============================================================================
// SAVE / LOAD (NVS persistence)
//=============================================================================
//
// We store the entire GameState as a single binary blob under namespace
// "radiocave", key "save". A small header (magic + version) lets us reject
// stale saves cleanly when the firmware updates and the struct layout
// changes — better to start fresh than to crash on garbled state.
//
// The save is written after every command that changes game state (room
// changes, item events, puzzle progression). NVS does its own wear
// levelling, and the rate of writes during normal play is on the order of
// one every few seconds — well within flash lifetime.
//
// The save is cleared on victory and death so the next entry into Radio
// Cave starts from scratch.

#define RC_SAVE_MAGIC      0x52434356u   // "RCCV"
#define RC_SAVE_FMT_VERSION  1

struct RcSaveBlob {
    uint32_t  magic;
    uint8_t   version;
    GameState state;
};

static bool loadGame() {
    Preferences pref;
    if (!pref.begin("radiocave", true)) return false;
    RcSaveBlob blob;
    size_t got = pref.getBytes("save", &blob, sizeof(blob));
    pref.end();
    if (got != sizeof(blob))                return false;
    if (blob.magic != RC_SAVE_MAGIC)        return false;
    if (blob.version != RC_SAVE_FMT_VERSION) return false;
    if (blob.state.room < 1 || blob.state.room > RC_NUM_ROOMS) return false;
    game = blob.state;
    rebuildInventory();
    return true;
}

static void saveGame() {
    Preferences pref;
    if (!pref.begin("radiocave", false)) return;
    RcSaveBlob blob;
    blob.magic   = RC_SAVE_MAGIC;
    blob.version = RC_SAVE_FMT_VERSION;
    blob.state   = game;
    pref.putBytes("save", &blob, sizeof(blob));
    pref.end();
}

static void clearSave() {
    Preferences pref;
    if (!pref.begin("radiocave", false)) return;
    pref.remove("save");
    pref.end();
}

static bool hasSave() {
    Preferences pref;
    if (!pref.begin("radiocave", true)) return false;
    bool exists = pref.isKey("save");
    pref.end();
    return exists;
}

static inline bool roomVisited(uint8_t r) {
    return (game.visited & (1u << r)) != 0;
}
static inline void markVisited(uint8_t r) {
    game.visited |= (1u << r);
}

// Returns true if the given item is currently carried by the player.
static inline bool itemCarried(uint8_t it) {
    return it >= 1 && it <= RC_NUM_ITEMS && game.itemLoc[it] == RC_ITEM_CARRIED;
}

// Returns true if the given item is currently sitting in the given room.
static inline bool itemInRoom(uint8_t it, uint8_t r) {
    return it >= 1 && it <= RC_NUM_ITEMS && game.itemLoc[it] == r;
}

// Returns whether a given item should be VISIBLE in the room it's in.
// Some items have visibility gating: KEY in room 4 is only visible when the
// generator is running.
static bool itemVisibleInRoom(uint8_t it, uint8_t r) {
    if (!itemInRoom(it, r)) return false;
    if (it == RC_KEY && r == 4) return game.generator;
    // CHOKE in room 7 is visible whether or not it's been "detached" — the
    // description logic handles that via its own state. The masterPower
    // check for the death trap kicks in at TAKE time, not at visibility.
    return true;
}

// Resolve an argument string to an item ID via prefix match. Returns:
//   0                 → no match
//   1..RC_NUM_ITEMS   → unambiguous match
//   255 (RC_ITEM_AMBIGUOUS) → multiple items match the prefix
#define RC_ITEM_AMBIGUOUS 255
static uint8_t resolveItem(const String& lcArg) {
    if (lcArg.length() == 0) return 0;
    uint8_t match = 0;
    int hits = 0;
    String upArg(lcArg); upArg.toUpperCase();
    for (uint8_t it = 1; it <= RC_NUM_ITEMS; it++) {
        String name(ITEMS[it].name);
        if (name.startsWith(upArg)) {
            match = it;
            hits++;
        }
    }
    if (hits == 0) return 0;
    if (hits > 1)  return RC_ITEM_AMBIGUOUS;
    return match;
}

// Context-aware prefix match restricted to items the player could plausibly
// mean in a given context. This disambiguates things like "T M" → MANUAL
// when MIC has already been taken (so only MANUAL is visible in the room).
//
// `candidates` is a bitmask of item IDs to consider; bit i set means item i
// is a candidate.
static uint8_t resolveItemAmong(const String& lcArg, uint8_t mask) {
    if (lcArg.length() == 0) return 0;
    uint8_t match = 0;
    int hits = 0;
    String upArg(lcArg); upArg.toUpperCase();
    for (uint8_t it = 1; it <= RC_NUM_ITEMS; it++) {
        if (!(mask & (1u << it))) continue;
        String name(ITEMS[it].name);
        if (name.startsWith(upArg)) {
            match = it;
            hits++;
        }
    }
    if (hits == 0) return 0;
    if (hits > 1)  return RC_ITEM_AMBIGUOUS;
    return match;
}

// Build a candidate mask of items TAKE-able from the current room.
static uint8_t takeableItemsMask() {
    uint8_t mask = 0;
    for (uint8_t it = 1; it <= RC_NUM_ITEMS; it++) {
        if (itemVisibleInRoom(it, game.room)) mask |= (1u << it);
    }
    return mask;
}

// Build a candidate mask of items currently carried (DROP-able).
static uint8_t carriedItemsMask() {
    uint8_t mask = 0;
    for (uint8_t it = 1; it <= RC_NUM_ITEMS; it++) {
        if (itemCarried(it)) mask |= (1u << it);
    }
    return mask;
}

// Resolve an exit, respecting puzzle-state gating. Returns the destination
// room (1..12) or -1 if no exit / exit blocked.
static int8_t getExit(uint8_t r, RcDir dir) {
    int8_t dest = ROOMS[r].exits[dir];
    if (dest <= 0) return -1;
    // Room 7 east → Captain's Office (11) is gated by officeOpen
    if (r == 7 && dir == RC_E && !game.officeOpen) return -1;
    return dest;
}


//=============================================================================
// Room descriptions
//=============================================================================
//
// Returns the long-form (first-visit) or short-form (revisit) description.
// Several rooms have variations driven by puzzle state — the generator
// running, the office door open, the safe open, the choke removed, etc. —
// so we generate descriptions at runtime rather than baking them in.

static String roomDescription(uint8_t r, bool full) {
    bool v = roomVisited(r) && !full;
    switch (r) {
    case 1:
        return v ? F("Forest trail. A squeaking metal "
                     "door leads north into the mountain.")
                 : F("A narrow forest trail. The wind "
                     "pushes an old metal door back and "
                     "forth on its rusty hinges — it "
                     "squeaks with every gust. Behind it, "
                     "a dark passage leads into the "
                     "mountainside.");
    case 2:
        return v ? F("Cave entrance. \"RC0\" stenciled "
                     "above the door. Corridor leads north.")
                 : F("A heavy steel door, rusted but ajar, "
                     "leads deeper into the mountain. Cold "
                     "air flows from inside. Above the door, "
                     "faded stenciled letters read: RC0. "
                     "Beyond the door, a corridor stretches "
                     "into darkness.");
    case 3:
        return v ? F("Main corridor. Doors lead in all "
                     "directions.")
                 : F("A long concrete corridor lit by "
                     "emergency strips that barely glow. "
                     "Doors line both sides. Signs on the "
                     "walls point to various rooms. The "
                     "air smells of dust and old "
                     "electronics. A faded poster on the "
                     "wall shows radio operating "
                     "procedures.");
    case 4:
        if (!game.generator)
            return v ? F("Generator room. The generator is "
                         "silent. Fuel gauge reads empty.")
                     : F("A large diesel generator "
                         "dominates this room. The fuel "
                         "gauge reads empty. A thick power "
                         "cable runs from the generator "
                         "through the wall. The room smells "
                         "of old oil. It is dim in here.");
        // Generator is running; key is taken if not still in the room.
        return v ? F("Generator room. The generator hums "
                     "steadily.")
                 : F("The generator hums steadily, "
                     "powering the station.");
    case 5:
        return v ? F("Storage room. Shelves with equipment.")
                 : F("Metal shelves line the walls, stacked "
                     "with dusty boxes and old equipment.");
    case 6:
        if (!game.generator)
            return v ? F("Workshop. Soldering iron is cold "
                         "— no power.")
                     : F("A sturdy workbench with a "
                         "soldering iron plugged into an "
                         "outlet, wire cutters, and various "
                         "tools. A magnifying lamp hangs "
                         "over the bench. Without power, "
                         "the soldering iron is cold and "
                         "useless.");
        return v ? F("Workshop. Soldering iron ready.")
                 : F("The workbench is brightly lit. The "
                     "soldering iron is hot, ready for use. "
                     "Wire cutters, solder, and various "
                     "tools are neatly arranged.");
    case 7: {
        if (!game.tubeInstalled) {
            const __FlashStringHelper* doorText = game.officeOpen
                ? F("— it stands open.")
                : F("— it is locked.");
            if (v) {
                return game.officeOpen
                    ? F("Operating room. Tube dead, choke "
                        "disconnected. Captain's door is open.")
                    : F("Operating room. Tube dead, choke "
                        "disconnected. Captain's door is "
                        "locked.");
            }
            // Long form: mention the choke only if it's still in this room
            // (i.e. the player hasn't taken it). After taking, the choke
            // disappears from the description via the items suffix.
            String head;
            if (itemInRoom(RC_CHOKE, 7)) {
                head = F("The heart of the station. A large "
                        "transmitter rack fills one wall, "
                        "dominated by a 6146 power tube. "
                        "The tube's getter flash has "
                        "turned milky white — it's clearly "
                        "dead. A choke coil is "
                        "disconnected from a wire going to "
                        "the plate connector and obviously "
                        "needs repair. A straight key is "
                        "bolted to the operating desk. On "
                        "the wall: a master power switch "
                        "(OFF position) and a TX/RX toggle "
                        "switch. The walls are covered in "
                        "scribbled notes, callsigns, and "
                        "doodles. On the east wall, a "
                        "heavy door is marked \"Captain\" ");
            } else {
                head = F("The heart of the station. A large "
                        "transmitter rack fills one wall, "
                        "dominated by a 6146 power tube — its "
                        "getter flash is milky white, clearly "
                        "dead. The plate connector wire hangs "
                        "loose where a choke coil was removed. "
                        "A straight key is bolted to the "
                        "operating desk. On the wall: a master "
                        "power switch and TX/RX toggle. The "
                        "walls are covered in scribbled notes. "
                        "On the east wall, a heavy door marked "
                        "\"Captain\" ");
            }
            head += doorText;
            return head;
        }
        // Tube is installed — station repaired
        if (v) {
            return game.officeOpen
                ? F("Operating room. All equipment ready. "
                    "Captain's door is open.")
                : F("Operating room. All equipment ready.");
        }
        return game.officeOpen
            ? F("The transmitter rack gleams with the new 6146 — "
                "its getter flash a pristine silver mirror. The "
                "Captain's door stands open to the east.")
            : F("The transmitter rack gleams with the new 6146 — "
                "its getter flash a pristine silver mirror.");
    }
    case 8:
        if (!game.generator)
            return v ? F("Receiver alcove. No power.")
                     : F("A small alcove with a receiver "
                         "setup. A pair of old headphones "
                         "hangs from a hook. The receiver "
                         "is dark and silent.");
        if (game.antennaGrounded)
            return v ? F("Receiver alcove. Static only.")
                     : F("The receiver hums with power, "
                         "but produces only static.");
        return v ? F("Receiver alcove. A CW signal is "
                     "coming in!")
                 : F("The receiver crackles to life. "
                     "Through the headphones you hear a "
                     "clear CW signal — someone is calling!");
    case 9:
        return v ? F("Antenna tunnel. Ground switch on the "
                     "wall.")
                 : F("A long, sloping tunnel leading "
                     "upward. A ladder-wire feed line runs "
                     "along the ceiling toward daylight. "
                     "Halfway through, a heavy double knife "
                     "switch is mounted on the wall, "
                     "labeled \"ANTENNA GROUND\".");
    case 10:
        return v ? F("Antenna platform. Mast towers above.")
                 : F("Daylight! You emerge onto a rocky "
                     "platform high on the mountainside. A "
                     "tall antenna mast rises above you, "
                     "its guy wires creaking in the wind. "
                     "The view across the valley is "
                     "breathtaking.");
    case 11: {
        if (v) {
            return game.safeOpen
                ? F("Captain's office. Logbook on desk. Safe is open.")
                : F("Captain's office. Logbook on desk. "
                    "Safe in the corner.");
        }
        return game.safeOpen
            ? F("A small office with a wooden desk and a "
                "leather chair. A framed sign reading "
                "\"RC0\" hangs on the wall. On the desk "
                "lies a thick logbook, its pages yellowed "
                "with age. The heavy safe in the corner "
                "stands open.")
            : F("A small office with a wooden desk "
                "and a leather chair. A framed sign "
                "reading \"RC0\" hangs on the wall. "
                "On the desk lies a thick logbook, "
                "its pages yellowed with age. A heavy "
                "safe sits in the corner, its door "
                "closed.");
    }
    case 12:
        return F("Inside the safe: a spare 6146 tube "
                 "wrapped in oilcloth. A card reads: "
                 "\"Spare 6146. Handle with care.\"");
    }
    return F("");
}


//=============================================================================
// Word-wrap helper — renders a long string into wrapped lines that fit the
// main area width. Called when the room changes or on LOOK.
//=============================================================================

#define RC_MAX_WRAP_LINES  32
static String wrappedLines[RC_MAX_WRAP_LINES];
static int    wrappedCount = 0;

// Wrap text into lines that fit within maxWidth pixels using the currently
// selected canvas font. Preserves paragraph breaks on '\n'.
static void wrapText(const String& text, int maxWidth);   // fwd decl
static void refreshRoomText(bool full = false);            // fwd decl
static String roomDescription(uint8_t r, bool full);       // fwd decl
static bool movePlayer(RcDir dir);                         // fwd decl
static bool tryKeyPhrase(const String& phraseCase, const String& phraseLc, bool explicitKey); // fwd decl

static int  scrollLine = 0;                                // top visible line
static int  totalLines = 0;                                // synonym for wrappedCount, kept for compat

// Main-area layout inside the sprite.
// Body text uses FreeSans9pt7b (proportional sans) for readability.
// Line height ~16 px → 6 visible lines in a 96 px main area.
static const int MAIN_Y      = 32;
static const int MAIN_BOTTOM = 128;
static const int LINE_H      = 16;              // FreeSans9pt7b ascent+descent + gap
static const int VISIBLE_LINES = (MAIN_BOTTOM - MAIN_Y) / LINE_H;

// Input line
static const int INPUT_Y     = 132;             // single line below main

// Hint line
static const int HINT_Y      = 154;             // bottom

// --- Command parser state ---
static char          inputBuf[RC_INPUT_MAX + 1];
static uint8_t       inputLen       = 0;
static bool          inputDirty     = false;    // true if buffer needs redraw
static String        lastCommand    = "";       // echoed back to user
static String        lastClue       = "";       // for QRS replay
static uint8_t       lastClueWpm    = RC_CLUE_WPM;
static bool          pendingNewConfirm = false; // NEW asks for Y to confirm wipe

// --- CW player state (non-blocking) ---
static String        cwElements    = "";        // '1','2','0' stream (plus ' ' for word-gap we insert)
static int           cwPos         = 0;
static uint8_t       cwWpm         = RC_CLUE_WPM;
static unsigned int  cwDitMs       = 40;        // 1200 / wpm, computed on play
static unsigned long cwNextEventMs = 0;
static bool          cwPlaying     = false;     // true while the player runs (mutes input parsing)
static bool          cwToneOn      = false;     // true when we're currently sounding a tone


//=============================================================================
// Drawing helpers
//=============================================================================

static void pushFrame() { display.pushGameFrame(); }

static void drawCentred(int y, const char* text, uint16_t color,
                        const lgfx::IFont* font = nullptr) {
    if (font) canvas->setFont(font);
    canvas->setTextColor(color, RC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(text, RC_SCREEN_W / 2, y);
    canvas->setTextDatum(lgfx::top_left);
}

static void drawCentredAt(int x, int y, const char* text, uint16_t color,
                          const lgfx::IFont* font = nullptr) {
    if (font) canvas->setFont(font);
    canvas->setTextColor(color, RC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(text, x, y);
    canvas->setTextDatum(lgfx::top_left);
}

static void drawLeft(int x, int y, const char* text, uint16_t color,
                     const lgfx::IFont* font = nullptr) {
    if (font) canvas->setFont(font);
    canvas->setTextColor(color, RC_BG);
    canvas->setTextDatum(lgfx::top_left);
    canvas->drawString(text, x, y);
}


//=============================================================================
// LOBBY — entry screen shown once on entry to Radio Cave
//=============================================================================

static void drawLobby() {
    canvas->fillSprite(RC_BG);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentred(6, "RADIO CAVE", RC_ACCENT);
    canvas->drawFastHLine(20, 34, RC_SCREEN_W - 40, RC_FRAME);

    canvas->setFont(&fonts::FreeSans9pt7b);
    const int labelX = 12, valueX = 90;
    int y = 48; const int rowH = 22;

    drawLeft(labelX, y, "Paddle",  RC_ACCENT);
    drawLeft(valueX, y, "commands", RC_TEXT);   y += rowH;

    drawLeft(labelX, y, "Encoder", RC_ACCENT);
    drawLeft(valueX, y, "spd/vol/scroll", RC_TEXT); y += rowH;

    drawLeft(labelX, y, "FN",      RC_ACCENT);
    drawLeft(valueX, y, "toggle mode", RC_TEXT); y += rowH;

    drawLeft(labelX, y, "Hold",    RC_ACCENT);
    drawLeft(valueX, y, "exit", RC_TEXT);

    canvas->drawFastVLine(190, 42, RC_SCREEN_H - 48, RC_FRAME);

    const int rightCx = 252;
    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentredAt(rightCx, 48, "Explore",   RC_HIGHLIGHT);

    canvas->setFont(&fonts::Font0);
    drawCentredAt(rightCx, 72, "OE1WKL",   RC_DIM);

    canvas->drawFastHLine(205, 86, 100, RC_FRAME);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    drawCentredAt(rightCx,  96, "Click",  RC_OK);
    drawCentredAt(rightCx, 118, "or key", RC_OK);

    canvas->setFont(&fonts::FreeSans9pt7b);
    drawCentredAt(rightCx, 146, "to start", RC_TEXT);

    pushFrame();
}

static void lobbyLoop() {
    // Defensive: make sure we enter lobby with clean keyer state,
    // regardless of how we got here (cold start, back from play, etc).
    gameMode = false;
    gameCharBuffer = 0;
    clearPaddleLatches();

    drawLobby();
    while (rcState == RC_LOBBY) {
        // Either a paddle press or a mode-button short-press enters the
        // game. Paddle is the "natural" way for a CW user; mode-button
        // click is the fallback convention that matches other firmware
        // screens.
        if (checkPaddles()) {
            MorseOutput::resetTOT();
            while (checkPaddles()) delay(5);
            clearPaddleLatches();
            gameCharBuffer = 0;
            rcState = RC_PLAYING;
            return;
        }
        if (checkEncoder() != 0) MorseOutput::resetTOT();

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1) {
            clearPaddleLatches();
            gameCharBuffer = 0;
            rcState = RC_PLAYING;
            return;
        }
        if (Buttons::modeButton.clicks == -1) { rcState = RC_EXIT; return; }

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == -1) { rcState = RC_EXIT; return; }

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}


//=============================================================================
// CW PLAYER — non-blocking, driven from the main loop
//=============================================================================

//=============================================================================
// CW PLAYER — non-blocking, driven from the main loop.
//
// Modeled on MorsePileup.cpp's cwPlayer (a known-working implementation).
// Key design points:
//
// 1. Uses MorseOutput::pwmTone / pwmNoTone DIRECTLY, not keyOut(). keyOut()
//    is tangled with the keyer's intTone/extTone state and transmitter
//    gating, which is wrong for game-internal playback.
//
// 2. Pauses while the keyer is active or a paddle is held, so the player's
//    own sidetone isn't clipped by our playback.
//
// 3. Element gaps are timed directly (no separate KEY_DOWN / KEY_UP state).
//    On each tick: if tone is on → turn it off, schedule inter-element gap.
//    Otherwise → fetch next element, start tone (or schedule larger gap).
//=============================================================================

// Build a CW element stream for a multi-word text. Each word gets
// Build a CW element stream for a multi-word text.
//
// Case convention (matches the firmware's CWchars table in m32_v6.ino):
//   lowercase a-z → regular letters  (e.g. 'k' = K = dah-dit-dah)
//   uppercase letters in SANKEBH → prosigns:
//       S = <as>   A = <ka>   N = <kn>   K = <sk>
//       E = <ve>   B = <bk>   H = <ch>
//   Digits 0-9, punctuation .,:-/=?@+ also supported.
//
// So to key the final QSO phrase "DE RC0 QSL <SK>", callers should pass
// the string literal  "de rc0 qsl K"  (lowercase letters, uppercase K
// for the <sk> prosign). Mixed-case inputs are preserved.
//
// Output alphabet:
//   '1' = dit,  '2' = dah,  '0' = char-gap,  ' ' = word-gap
static String buildCwStream(const String& text) {
    String out;
    out.reserve(text.length() * 8);
    int start = 0;
    while (start < (int)text.length()) {
        int sp = text.indexOf(' ', start);
        String word = (sp < 0) ? text.substring(start) : text.substring(start, sp);
        // Do NOT lowercase — the caller has chosen case deliberately so that
        // uppercase SANKEBH characters map to prosigns. generateCWword()
        // looks up each character against CWchars, which contains lowercase
        // letters AND the uppercase prosign codes, side by side.
        String elems = generateCWword(word);
        out += elems;
        if (sp < 0) break;
        out += ' ';                                  // word-gap marker
        start = sp + 1;
    }
    return out;
}

static void cwPlayerStart(const String& text, uint8_t wpm) {
    cwElements     = buildCwStream(text);
    cwPos          = 0;
    cwWpm          = wpm;
    cwDitMs        = 1200 / wpm;
    cwToneOn       = false;
    cwNextEventMs  = millis();
    cwPlaying      = true;
    lastClue       = text;
    lastClueWpm    = wpm;
}

static void cwPlayerStop() {
    if (cwToneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        cwToneOn = false;
    }
    cwPlaying = false;
}

// Service the CW player. Call every loop iteration.
static void cwPlayerTick() {
    if (!cwPlaying) return;

    // Mute while the keyer is active or a paddle is held (same as Pileup's
    // cwPlayer). This prevents our playback from colliding with the player's
    // own keying sidetone.
    bool shouldMute = (keyerState != IDLE_STATE) || leftKey || rightKey;
    if (shouldMute) {
        if (cwToneOn) {
            MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
            cwToneOn = false;
        }
        cwNextEventMs = millis() + 100;              // recheck shortly
        return;
    }

    if ((long)(millis() - cwNextEventMs) < 0) return;    // waiting

    // If tone is on, turn it off (end of dit/dah)
    if (cwToneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        cwToneOn = false;
        cwNextEventMs = millis() + cwDitMs - 7;       // inter-element gap
        return;
    }

    // Advance to next element
    if (cwPos >= (int)cwElements.length()) {
        cwPlayerStop();
        return;
    }

    char c = cwElements[cwPos++];
    switch (c) {
        case '1':   // dit
            MorseOutput::pwmTone(RC_CLUE_PITCH_HZ,
                                 MorsePreferences::sidetoneVolume, false);
            cwToneOn = true;
            cwNextEventMs = millis() + cwDitMs - 7;
            break;
        case '2':   // dah
            MorseOutput::pwmTone(RC_CLUE_PITCH_HZ,
                                 MorsePreferences::sidetoneVolume, false);
            cwToneOn = true;
            cwNextEventMs = millis() + (cwDitMs * 3) - 7;
            break;
        case '0':   // char-gap (already had 1 dit inter-element, need 2 more)
            cwNextEventMs = millis() + cwDitMs * 2;
            break;
        case ' ':   // word-gap (already had 1 dit inter-element, need 6 more)
            cwNextEventMs = millis() + cwDitMs * 6;
            break;
        default:
            break;                                    // unknown — skip
    }
}


//=============================================================================
// COMMAND PARSER
//=============================================================================

// Tokenize the command into (verb, arg). Both are lowercase. Only the first
// space is a token boundary — if the command was "pse qrs" the arg is
// "qrs" and we separately match the whole thing as well.
static void tokenize(const String& lc, String& verb, String& arg) {
    int sp = lc.indexOf(' ');
    if (sp < 0) { verb = lc; arg = ""; }
    else        { verb = lc.substring(0, sp); arg = lc.substring(sp + 1); }
}

// Returns true when `a` is a non-empty prefix of `target` — allowing CW
// abbreviations like "S" for "SWITCH", "W" for "WALL", "SW" for "SWITCH".
// Caller arranges the order of checks so ambiguities resolve sensibly.
static bool argIs(const String& a, const char* target) {
    if (a.length() == 0) return false;
    String t(target);
    return t.startsWith(a);
}

// Handle a "keyed phrase" — either from `KEY <phrase>` (explicitKey=true)
// or from a bare word that the player keyed directly (explicitKey=false).
//
// Returns true if the phrase was recognized and handled (lastCommand /
// screen updated). Returns false if unrecognized — the caller should fall
// through to its own unknown-command handling.
//
// Takes both the case-preserving form and the lowercase form. Most simple
// phrases (XYZZY, JN78DH) match on the lowercase form. The final-QSO
// phrases must match on the case-preserving form because an uppercase K
// means the <SK> prosign in firmware convention — lowercasing would turn
// the prosign into a plain letter K.
//
// Adding a new key-phrase here makes it usable both as `KEY foo` and as
// just `foo`. Restrict to phrases whose only plausible interpretation is
// "player is trying to send this at some kind of CW-sensitive device"
// (magic words, callsigns, locators, prosign-containing messages).
static bool tryKeyPhrase(const String& phraseCase, const String& phraseLc, bool explicitKey) {
    (void)explicitKey;  // reserved for future use (e.g. different feedback)

    // XYZZY — opens the Captain's office (alternative to USE KEY)
    if (phraseLc == "xyzzy") {
        if (game.room == 7 && !game.officeOpen) {
            game.officeOpen = true;
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("You key X-Y-Z-Z-Y on the straight key. "
                       "From the east, you hear a click — the "
                       "Captain's Office door swings open!"),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return true;
        }
        if (game.room == 11) {
            lastCommand = "You're already inside.";
            return true;
        }
        lastCommand = "Nothing happens.";
        return true;
    }

    // JN78DH — opens the safe
    if (phraseLc == "jn78dh") {
        if (game.room == 11 && !game.safeOpen) {
            game.safeOpen = true;
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("You key J-N-7-8-D-H. The safe's lock "
                       "whirs and clicks. The heavy door swings "
                       "open. Inside: a pristine 6146 tube "
                       "wrapped in oilcloth."),
                     RC_SCREEN_W - 10);
            if (game.itemLoc[RC_TUBE] == 12)
                game.itemLoc[RC_TUBE] = 11;
            lastCommand = "";
            refreshRoomText(false);
            inputDirty = true;
            return true;
        }
        if (game.room == 11 && game.safeOpen) {
            lastCommand = "The safe is already open.";
            return true;
        }
        lastCommand = "Nothing happens.";
        return true;
    }

    // --- Final QSO phrases ---
    //
    // The final QSO is only possible in specific conditions: in room 7,
    // master power on, TX mode, tube installed, antenna not grounded.
    //
    // Canonical phrases (case matters, uppercase K = <sk> prosign):
    //   Stage 0 → 2:   "de rc0 qsl K"       (player's first call)
    //   Stage 2 → WON: "de de rc0 rc0 qsl qsl K K"  (QSZ — each word twice)
    //
    // We also accept lenient forms where the player used lowercase k
    // instead of the prosign — this is friendlier and matches the
    // prototype. The CW audio we play ALWAYS uses the proper prosign.

    bool qsoReady = (game.room == 7 && game.masterPower && game.txMode &&
                     game.tubeInstalled && !game.antennaGrounded);

    // Match both strict-case "de rc0 qsl K" and lenient lowercase
    // "de rc0 qsl k". We check against the case-preserving form so we
    // can distinguish strict vs lenient in the future if desired.
    bool isQslSingle =
        (phraseCase == "de rc0 qsl K" || phraseLc == "de rc0 qsl k" ||
         phraseLc == "de rc0 qsl sk");
    bool isQslDouble =
        (phraseCase == "de de rc0 rc0 qsl qsl K K" ||
         phraseLc  == "de de rc0 rc0 qsl qsl k k" ||
         phraseLc  == "de de rc0 rc0 qsl qsl sk sk");

    if (qsoReady && (isQslSingle || isQslDouble)) {
        if (isQslSingle && game.qsoStage == 0) {
            // First call successful — IR7 responds asking for repetition
            game.qsoStage = 2;
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("You key: DE RC0 QSL <SK>\n\n"
                       "A pause... then the receiver comes alive!\n\n"
                       "IR7 responds: RC0 DE IR7 PSE QSZ"),
                     RC_SCREEN_W - 10);
            cwPlayerStart(String("rc0 de ir7 pse qsz"), 30);
            lastCommand = "";
            inputDirty = true;
            return true;
        }
        if (isQslSingle && game.qsoStage == 2) {
            // Player repeated the single message — IR7 patient reminder
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("IR7 patiently repeats: RC0 DE IR7 PSE QSZ\n\n"
                       "(Hint: QSZ means send each word twice.)"),
                     RC_SCREEN_W - 10);
            cwPlayerStart(String("rc0 de ir7 pse qsz"), 30);
            lastCommand = "";
            inputDirty = true;
            return true;
        }
        if (isQslDouble && game.qsoStage == 2) {
            // VICTORY
            game.qsoStage = 3;
            clearSave(); rcState = RC_WON;
            return true;
        }
        // Out-of-order phrase — treat as unhelpful key
        canvas->setFont(&fonts::FreeSans9pt7b);
        wrapText(F("You key the message. No response."),
                 RC_SCREEN_W - 10);
        lastCommand = "";
        inputDirty = true;
        return true;
    }

    // Same phrases but preconditions not met — acknowledge that we
    // recognized it, but the station can't transmit. This also prevents
    // the "bare text → KEY" shortcut from falling through and saying
    // "What?" on a QSO attempt.
    if (isQslSingle || isQslDouble) {
        if (game.room != 7)        lastCommand = "You're not at the transmitter.";
        else if (!game.masterPower) lastCommand = "Master power is off.";
        else if (!game.txMode)      lastCommand = "Not in TX mode.";
        else if (!game.tubeInstalled) lastCommand = "The tube is dead.";
        else if (game.antennaGrounded) lastCommand = "Antenna is grounded.";
        else                        lastCommand = "Nothing happens.";
        return true;
    }

    return false;
}

static void dispatchCommand(const char* cmd) {
    // The Morserino decoder appends a trailing space when it detects the
    // end of a word (line 130 of MorseDecoder.cpp). Strip it — plus any
    // other trailing whitespace — before matching.
    String trimmed(cmd);
    while (trimmed.length() > 0 &&
           (trimmed[trimmed.length() - 1] == ' ' ||
            trimmed[trimmed.length() - 1] == '\t')) {
        trimmed.remove(trimmed.length() - 1);
    }
    if (trimmed.length() == 0) return;

    // Every accepted command counts as a step (matches the prototype).
    game.steps++;

    // Display the dispatched command in uppercase for readability.
    // Internal comparisons use lowercase (matches the buffer convention).
    String display(trimmed);
    display.toUpperCase();
    lastCommand = display;

    String lc(trimmed);
    lc.toLowerCase();

    // Tokenize twice: once on the lowercased form (for verb matching and
    // most arg checks), once on the case-preserving form (for KEY phrases
    // that contain prosigns, since uppercase letters have special meaning
    // in firmware CW encoding — e.g. K = <sk>).
    String verb, arg;
    tokenize(lc, verb, arg);
    String verbCase, argCase;
    tokenize(trimmed, verbCase, argCase);

    // --- Movement ---
    if (verb == "n") { movePlayer(RC_N); return; }
    if (verb == "s") { movePlayer(RC_S); return; }
    if (verb == "e") { movePlayer(RC_E); return; }
    if (verb == "w") { movePlayer(RC_W); return; }

    // --- LOOK / L ---
    // "l" or "look" alone → redisplay full room description.
    // "l <obj>" or "look <obj>" → examine item or room feature.
    if (verb == "l" || verb == "look") {
        if (arg.length() == 0) {
            refreshRoomText(true);
            lastCommand = "";           // room text speaks
            inputDirty = true;
            return;
        }
        // First try items that are actually HERE (room or inventory).
        // We deliberately don't match items that exist-in-general but
        // aren't reachable — that lets the feature handlers below take
        // over (e.g. `L TUBE` in room 7 after installing the tube, where
        // TUBE the item is gone but "tube" as a room feature still means
        // something).
        uint8_t localMask = takeableItemsMask() | carriedItemsMask();
        uint8_t it = resolveItemAmong(arg, localMask);
        if (it == RC_ITEM_AMBIGUOUS) {
            lastCommand = "Which one?";
            return;
        }
        if (it > 0) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(String(ITEMS[it].examine), RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }

        // Not a reachable item — try room features (prefix-matched).
        String a = arg;  // already lowercase from tokenize
        // --- Room 1: door, sign ---
        if (game.room == 1 && (argIs(a, "door") || argIs(a, "sign"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("An old metal door hanging off one hinge, "
                       "squeaking in the wind."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 2: door, sign ---
        if (game.room == 2 && (argIs(a, "door") || argIs(a, "sign"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("Faded stenciled letters above the door: \"RC0\"."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 3: poster, sign ---
        if (game.room == 3 && (argIs(a, "poster") || argIs(a, "sign"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("A faded poster with radio operating procedures."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 4: generator ---
        if (game.room == 4 && argIs(a, "generator")) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(game.generator
                     ? F("The generator rumbles steadily.")
                     : F("A large diesel generator. Fuel gauge reads empty."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 5: shelf, shelves ---
        if (game.room == 5 && (argIs(a, "shelf") || argIs(a, "shelves"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("Metal shelves lined with dusty boxes and old equipment."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 6: bench, iron ---
        if (game.room == 6 && (argIs(a, "bench") || argIs(a, "iron"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(game.generator
                     ? F("The soldering iron is hot and ready.")
                     : F("The soldering iron is cold — no power."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 7: door, switch, tube, wall ---
        if (game.room == 7 && argIs(a, "door")) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(game.officeOpen
                     ? F("The Captain's door stands open. The office is to the east.")
                     : F("A heavy door marked \"Captain\". It is locked. "
                         "There is a keyhole below the handle."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        if (game.room == 7 && argIs(a, "switch")) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            char line[80];
            snprintf(line, sizeof(line),
                     "Master power: %s. Mode: %s.",
                     game.masterPower ? "ON" : "OFF",
                     game.txMode      ? "TX" : "RX");
            wrapText(String(line), RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        if (game.room == 7 && argIs(a, "tube")) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(game.tubeInstalled
                     ? F("The new 6146 gleams with a pristine silver getter flash.")
                     : F("The 6146's getter flash has turned milky white. "
                         "The vacuum seal is broken. This tube is dead."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        if (game.room == 7 && argIs(a, "wall")) {
            // Show the wall scribbles, then start the CW clue.
            // First look: plays XYZZY. Subsequent looks: plays "de rc0 qsl K"
            //   (uppercase K = <sk> prosign in firmware convention).
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("88   CQ DX   6HKE   0ULP   W3DZZ\n"
                       "QRZ?   XYZZY   DE RC0 QSL <SK>\n"
                       "QTH?   QLF G5RV   RST 577  55"),
                     RC_SCREEN_W - 10);
            game.wallLookCount++;
            if (game.wallLookCount == 1) {
                cwPlayerStart(String("xyzzy"), 16);
                lastCommand = "Scribbles on the wall. CW...";
            } else {
                cwPlayerStart(String("de rc0 qsl K"), 16);
                lastCommand = "Scribbles. CW replaying...";
            }
            inputDirty = true;
            return;
        }
        // --- Room 8: receiver, phones ---
        if (game.room == 8 && (argIs(a, "receiver") || argIs(a, "phones"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            if (!game.generator) {
                wrapText(F("Receiver is dead. No power."), RC_SCREEN_W - 10);
            } else if (game.antennaGrounded) {
                wrapText(F("Only static through the phones."), RC_SCREEN_W - 10);
            } else {
                wrapText(F("A clear CW signal comes through!"), RC_SCREEN_W - 10);
            }
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 9: switch, cable, feedline ---
        if (game.room == 9 && argIs(a, "switch")) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            char line[80];
            snprintf(line, sizeof(line),
                     "Antenna ground switch: %s.",
                     game.antennaGrounded ? "GROUND" : "OPERATE");
            wrapText(String(line), RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        if (game.room == 9 && (argIs(a, "cable") || argIs(a, "feedline"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("Ladder-wire feed line running up to the antenna platform."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 10: antenna, mast ---
        if (game.room == 10 && (argIs(a, "antenna") || argIs(a, "mast"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("Tall antenna mast. Guy wires creak. Looks functional."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        // --- Room 11: desk, log, logbook, safe ---
        if (game.room == 11 && (argIs(a, "desk") || argIs(a, "log") || argIs(a, "logbook"))) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("A thick logbook lies on the desk. "
                       "Use READ LOG to examine it."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        if (game.room == 11 && argIs(a, "safe")) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(game.safeOpen
                     ? F("The safe stands open.")
                     : F("A heavy safe. The door is closed. There is a combination lock."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }

        lastCommand = "You don't see anything special.";
        return;
    }

    // --- INVENTORY ---
    if (verb == "i" || verb == "inv") {
        if (game.invCount == 0) {
            lastCommand = "You're carrying nothing.";
            return;
        }
        // Short enough for the input line? Only when carrying one item.
        // Otherwise put the list in the main area so it can wrap cleanly.
        if (game.invCount == 1) {
            String s = "Carrying: ";
            s += ITEMS[game.inv[0]].takeName;
            s += ".";
            lastCommand = s;
            return;
        }
        String s = "Carrying:\n";
        for (uint8_t i = 0; i < game.invCount; i++) {
            s += "• ";
            s += ITEMS[game.inv[i]].takeName;
            s += "\n";
        }
        canvas->setFont(&fonts::FreeSans9pt7b);
        wrapText(s, RC_SCREEN_W - 10);
        lastCommand = "";
        inputDirty = true;
        return;
    }

    // --- HELP ---
    if (verb == "h" || verb == "help") {
        // Re-use the main area for help by temporarily wrapping a help
        // string. The next movement will restore the room text.
        String help = F("Commands:\n"
                        "N/S/E/W — move\n"
                        "L <obj> — look / examine\n"
                        "I — inventory\n"
                        "TAKE, DROP <item>\n"
                        "USE <item/thing>\n"
                        "USE TX / USE RX\n"
                        "FIX <item> — repair\n"
                        "READ MANUAL, READ LOG\n"
                        "KEY <word> — send CW\n"
                        "QRS — replay slower\n"
                        "NEW — restart game\n"
                        "H — this help");
        canvas->setFont(&fonts::FreeSans9pt7b);
        wrapText(help, RC_SCREEN_W - 10);
        lastCommand = "";
        inputDirty = true;
        return;
    }

    // --- QRS — replay last clue at half speed ---
    if (lc == "qrs" || lc == "pse qrs") {
        if (lastClue.length() > 0) {
            cwPlayerStart(lastClue, _max(5, lastClueWpm / RC_CLUE_QRS_DIVISOR));
            lastCommand = "QRS (replay)";
        } else {
            lastCommand = "QRS — nothing to replay yet";
        }
        return;
    }

    // --- NEW — confirmation flow (key Y to wipe save and restart) ---
    if (verb == "new") {
        if (game.steps == 0) {
            // Already at the start — no need to confirm
            lastCommand = "Already a fresh game.";
            return;
        }
        pendingNewConfirm = true;
        lastCommand = "Wipe save? Key Y to confirm.";
        return;
    }
    // Confirmation answers (only meaningful while pendingNewConfirm)
    if (pendingNewConfirm) {
        pendingNewConfirm = false;
        if (verb == "y" || verb == "yes") {
            clearSave();
            newGame();
            markVisited(game.room);
            refreshRoomText(true);
            lastCommand = "Game restarted.";
            inputDirty = true;
            return;
        }
        // anything else cancels
        lastCommand = "Cancelled.";
        return;
    }

    // --- TAKE / T <item> ---
    if (verb == "t" || verb == "take") {
        if (arg.length() == 0) { lastCommand = "Take what?"; return; }
        // Try context-restricted first: items takeable from here.
        uint8_t mask = takeableItemsMask();
        uint8_t it = resolveItemAmong(arg, mask);
        if (it == 0) {
            // Nothing here matches. Does it name a known item at all?
            uint8_t globalIt = resolveItem(arg);
            if (globalIt == RC_ITEM_AMBIGUOUS) {
                lastCommand = "Which one?";
            } else if (globalIt > 0 && itemCarried(globalIt)) {
                lastCommand = "You already have that.";
            } else {
                lastCommand = "There's nothing like that here.";
            }
            return;
        }
        if (it == RC_ITEM_AMBIGUOUS) { lastCommand = "Which one?"; return; }
        if (game.invCount >= RC_INV_MAX) {
            lastCommand = "Your hands are full. Drop something.";
            return;
        }

        // Death trap: reaching for the choke in room 7 when master
        // power is on delivers a fatal shock. The choke hangs next to
        // the plate connector — a very bad place for fingers.
        if (it == RC_CHOKE && game.room == 7 && game.masterPower) {
            clearSave(); rcState = RC_DEAD;
            return;
        }

        game.itemLoc[it] = RC_ITEM_CARRIED;
        rebuildInventory();
        refreshRoomText(false);

        String msg = "You take the ";
        msg += ITEMS[it].takeName;
        msg += ".";
        lastCommand = msg;
        return;
    }

    // --- DROP / D <item> ---
    if (verb == "d" || verb == "dr" || verb == "drop") {
        if (arg.length() == 0) { lastCommand = "Drop what?"; return; }
        uint8_t mask = carriedItemsMask();
        uint8_t it = resolveItemAmong(arg, mask);
        if (it == 0)                 { lastCommand = "You don't have that."; return; }
        if (it == RC_ITEM_AMBIGUOUS) { lastCommand = "Which one?"; return; }

        game.itemLoc[it] = game.room;
        rebuildInventory();
        refreshRoomText(false);

        String msg = "You drop the ";
        msg += ITEMS[it].takeName;
        msg += ".";
        lastCommand = msg;
        return;
    }

    // --- USE <item/thing> ---
    if (verb == "u" || verb == "use") {
        if (arg.length() == 0) { lastCommand = "Use what?"; return; }

        // --- USE FUEL (in room 4, to start the generator) ---
        if (argIs(arg, "fuel")) {
            if (!itemCarried(RC_FUEL))  { lastCommand = "You don't have that."; return; }
            if (game.room != 4)         { lastCommand = "Nothing happens."; return; }
            if (game.generator)         { lastCommand = "Generator already runs."; return; }
            game.generator = true;
            game.itemLoc[RC_FUEL] = RC_ITEM_GONE;   // consumed
            rebuildInventory();
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("You pour the fuel into the tank. The generator "
                       "coughs, sputters... and rumbles to life! Lights "
                       "flicker on throughout the station. In the light, "
                       "you notice a small brass key hanging on a nail "
                       "behind the generator."),
                     RC_SCREEN_W - 10);
            lastCommand = "";
            inputDirty = true;
            return;
        }

        // --- USE KEY (opens Captain's office in room 7 or 11) ---
        if (argIs(arg, "key")) {
            if (!itemCarried(RC_KEY)) { lastCommand = "You don't have that."; return; }
            if ((game.room == 7 || game.room == 11) && !game.officeOpen) {
                game.officeOpen = true;
                lastCommand = "Click! The door swings open.";
                refreshRoomText(false);
                inputDirty = true;
                return;
            }
            if (game.officeOpen && (game.room == 7 || game.room == 11)) {
                lastCommand = "The door is already open.";
                return;
            }
            lastCommand = "The key doesn't fit anything here.";
            return;
        }

        // --- USE SWITCH ---
        if (argIs(arg, "switch")) {
            // Antenna ground switch in room 9
            if (game.room == 9) {
                game.antennaGrounded = !game.antennaGrounded;
                if (game.antennaGrounded) {
                    lastCommand = "Switch → GROUND.";
                } else {
                    lastCommand = "Switch → OPERATE. Antenna live.";
                }
                refreshRoomText(false);
                inputDirty = true;
                return;
            }
            // Master power switch in room 7
            if (game.room == 7) {
                game.masterPower = !game.masterPower;
                canvas->setFont(&fonts::FreeSans9pt7b);
                if (game.masterPower) {
                    if (game.tubeInstalled && game.chokeInstalled) {
                        wrapText(F("You flip the master power switch ON. The "
                                   "transmitter hums to life. Meters flicker. "
                                   "The station is operational!"),
                                 RC_SCREEN_W - 10);
                    } else {
                        wrapText(F("You flip the master power switch ON. "
                                   "Nothing much happens — the transmitter "
                                   "section seems dead."),
                                 RC_SCREEN_W - 10);
                    }
                } else {
                    wrapText(F("You flip the master power switch OFF. "
                               "The transmitter goes silent."),
                             RC_SCREEN_W - 10);
                }
                lastCommand = "";
                inputDirty = true;
                return;
            }
            lastCommand = "There's no switch here.";
            return;
        }

        // --- USE TX (switch transmitter to transmit mode) ---
        // Requires at least 2 chars to avoid conflict with "t" = tube.
        //
        // SUBTRAP: keying TX with the antenna grounded kills the tube and
        // breaks the choke again. Runs only if tubeInstalled, so an already-
        // dead tube doesn't re-die.
        if (arg.length() >= 2 && argIs(arg, "tx")) {
            if (game.room != 7)         { lastCommand = "Nothing happens."; return; }
            if (!game.masterPower)      { lastCommand = "Master power is off."; return; }
            if (game.antennaGrounded && game.tubeInstalled) {
                game.tubeInstalled   = false;
                game.chokeInstalled  = false;
                game.chokeRepaired   = false;
                // Put the choke back in the room so player can try again
                game.itemLoc[RC_CHOKE] = 7;
                rebuildInventory();
                refreshRoomText(false);
                canvas->setFont(&fonts::FreeSans9pt7b);
                // The 6146 the player installed is gone, and there is no
                // spare in this implementation — so the station is truly
                // unwinnable from here. Tell the player directly so they
                // don't spin trying to recover.
                wrapText(F("A loud POP from the transmitter and a puff "
                           "of smoke! The 6146 now looks milky white — "
                           "the tube is dead. The choke's solder joint "
                           "has cracked open. There are no spare tubes "
                           "left. The station cannot be revived."),
                         RC_SCREEN_W - 10);
                lastCommand = "Station dead. Key NEW to restart.";
                inputDirty = true;
                return;
            }
            game.txMode = true;
            if (game.tubeInstalled && !game.antennaGrounded && game.qsoStage == 0) {
                canvas->setFont(&fonts::FreeSans9pt7b);
                wrapText(F("Switched to TX mode.\n\n"
                           "The frequency is clear. You can key your "
                           "message now."),
                         RC_SCREEN_W - 10);
                lastCommand = "";
                inputDirty = true;
            } else {
                lastCommand = "Switched to TX mode.";
            }
            return;
        }

        // --- USE RX (switch transmitter to receive mode) ---
        if (arg.length() >= 2 && argIs(arg, "rx")) {
            if (game.room != 7) { lastCommand = "Nothing happens."; return; }
            game.txMode = false;
            if (game.masterPower && game.tubeInstalled && !game.antennaGrounded) {
                // Only replay the initial CQ if we haven't advanced past it.
                if (game.qsoStage == 0) {
                    cwPlayerStart(String("rc0 rc0 de ir7 ir7 k"), 30);
                }
                lastCommand = "Switched to RX mode.";
            } else {
                lastCommand = "Switched to RX mode.";
            }
            return;
        }

        // --- USE CHOKE (repair in room 6, install in room 7) ---
        if (argIs(arg, "choke")) {
            if (!itemCarried(RC_CHOKE)) {
                if (game.room == 7 && game.chokeInstalled)
                    lastCommand = "The choke coil is already installed.";
                else
                    lastCommand = "You don't have that.";
                return;
            }
            if (game.room == 6) {
                if (!game.generator) {
                    lastCommand = "Soldering iron is cold. No power.";
                    return;
                }
                if (game.chokeRepaired) {
                    lastCommand = "The choke coil is already repaired.";
                    return;
                }
                game.chokeRepaired = true;
                lastCommand = "Choke coil resoldered.";
                inputDirty = true;
                return;
            }
            if (game.room == 7) {
                // Death trap: reaching into the plate connector with HV live
                if (game.masterPower) {
                    clearSave(); rcState = RC_DEAD;
                    return;
                }
                if (!game.chokeRepaired) {
                    lastCommand = "Joint still cracked. Fix it first.";
                    return;
                }
                game.chokeInstalled = true;
                game.itemLoc[RC_CHOKE] = RC_ITEM_GONE;
                rebuildInventory();
                refreshRoomText(false);
                lastCommand = "Choke coil reattached.";
                inputDirty = true;
                return;
            }
            lastCommand = "Nothing happens.";
            return;
        }

        // --- USE TUBE (install in room 7) ---
        if (argIs(arg, "tube")) {
            // Note: "t" as verb is TAKE; "t" as arg to USE is TUBE. The
            // dispatcher already routed verb="use" arg="t" here.
            if (!itemCarried(RC_TUBE)) { lastCommand = "You don't have that."; return; }
            if (game.room == 6) {
                lastCommand = "Install the tube in Operating Room.";
                return;
            }
            if (game.room == 7) {
                if (game.masterPower) {
                    clearSave(); rcState = RC_DEAD;
                    return;
                }
                if (!game.chokeInstalled) {
                    lastCommand = "Choke isn't installed yet.";
                    return;
                }
                game.tubeInstalled = true;
                game.itemLoc[RC_TUBE] = RC_ITEM_GONE;
                rebuildInventory();
                refreshRoomText(false);
                lastCommand = "6146 installed. Transmitter ready!";
                inputDirty = true;
                return;
            }
            lastCommand = "Nothing happens.";
            return;
        }

        // --- USE MANUAL / USE MIC ---
        // "ma" = manual, "mi" = mic, bare "m" = disambiguate based on
        // what the player is carrying.
        if (argIs(arg, "manual")) {
            if (!argIs(arg, "mic") || arg.length() >= 2) {
                // Unambiguous manual match
                if (itemCarried(RC_MANUAL))
                    lastCommand = "Use READ MANUAL to read it.";
                else
                    lastCommand = "You don't have that.";
                return;
            }
            // arg is "m" — ambiguous
            bool hasManual = itemCarried(RC_MANUAL);
            bool hasMic    = itemCarried(RC_MIC);
            if (hasManual && !hasMic) {
                lastCommand = "Use READ MANUAL to read it.";
            } else if (hasMic && !hasManual) {
                lastCommand = "Connector incompatible. CW-only.";
            } else if (!hasManual && !hasMic) {
                lastCommand = "You don't have that.";
            } else {
                lastCommand = "Which one?";
            }
            return;
        }
        if (argIs(arg, "mic")) {
            lastCommand = "Connector incompatible. CW-only.";
            return;
        }

        lastCommand = "Nothing happens.";
        return;
    }

    // --- FIX / SOLDER (alias for USE CHOKE in room 6) ---
    if (verb == "f" || verb == "fix" || verb == "solder") {
        if (arg.length() == 0) { lastCommand = "Fix what?"; return; }
        if (argIs(arg, "choke")) {
            if (!itemCarried(RC_CHOKE)) { lastCommand = "You don't have that."; return; }
            if (game.room != 6)         { lastCommand = "Nothing happens."; return; }
            if (!game.generator)        { lastCommand = "Soldering iron is cold. No power."; return; }
            if (game.chokeRepaired)     { lastCommand = "The choke coil is already repaired."; return; }
            game.chokeRepaired = true;
            lastCommand = "Choke coil resoldered.";
            inputDirty = true;
            return;
        }
        lastCommand = "Nothing to fix like that.";
        return;
    }

    // --- READ ---
    if (verb == "r" || verb == "read") {
        if (arg.length() == 0) { lastCommand = "Read what?"; return; }

        // READ MANUAL (bare "m" matches since mic isn't readable anyway)
        if (argIs(arg, "manual")) {
            if (!itemCarried(RC_MANUAL)) {
                lastCommand = "You don't have that.";
                return;
            }
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("STATION RC0 — OPERATING PROCEDURES\n\n"
                       "! Danger! High Voltage!\n"
                       "Check tubes before powering on!\n\n"
                       "Always verify all tubes are seated and "
                       "functional before engaging the master "
                       "power switch."),
                     RC_SCREEN_W - 10);
            cwPlayerStart(String("danger"), 12);
            lastCommand = "CW playing: DANGER";
            inputDirty = true;
            return;
        }
        // READ LOG / LOGBOOK
        if (argIs(arg, "log") || argIs(arg, "logbook")) {
            if (game.room != 11) {
                lastCommand = "There's nothing to read here.";
                return;
            }
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("STATION LOG\n\n"
                       "19510901 23:15  IR5    LOC BK29??\n"
                       "19510917 04:45  XYZZY  LOC JN78DH\n"
                       "19540430 05:05  K666?  LOC CM87VK"),
                     RC_SCREEN_W - 10);
            cwPlayerStart(String("19510917"), 36);
            lastCommand = "CW playing...";
            inputDirty = true;
            return;
        }
        // READ WALL in room 7 → same as LOOK WALL. Synthesize:
        if (argIs(arg, "wall") && game.room == 7) {
            canvas->setFont(&fonts::FreeSans9pt7b);
            wrapText(F("88   CQ DX   6HKE   0ULP   W3DZZ\n"
                       "QRZ?   XYZZY   DE RC0 QSL <SK>\n"
                       "QTH?   QLF G5RV   RST 577  55"),
                     RC_SCREEN_W - 10);
            game.wallLookCount++;
            if (game.wallLookCount == 1) cwPlayerStart(String("xyzzy"), 16);
            else                          cwPlayerStart(String("de rc0 qsl K"), 16);
            lastCommand = "";
            inputDirty = true;
            return;
        }
        lastCommand = "There's nothing to read.";
        return;
    }

    // --- KEY <word> (send CW at a device that listens for a keyword) ---
    if (verb == "k" || verb == "key") {
        if (arg.length() == 0) { lastCommand = "Key what?"; return; }
        // Delegate to the phrase handler. Always reports back — if arg is
        // unrecognized, gives "Nothing happens". Pass both case-preserved
        // and lowercase forms: prosigns in QSO messages need the
        // uppercase K to survive.
        if (tryKeyPhrase(argCase, arg, /*explicit=*/true)) return;
        lastCommand = "Nothing happens.";
        return;
    }

    // --- Bare-text fallback: if the whole command is a recognized CW
    // phrase (like "xyzzy" or "jn78dh"), treat it as if keyed via KEY.
    // This matches the natural CW-operator reflex of just keying the
    // magic word on the straight key.
    //
    // This is conservative: only phrases tryKeyPhrase() recognizes are
    // treated this way. Random bogus words fall through to "What?".
    if (tryKeyPhrase(trimmed, lc, /*explicit=*/false)) return;

    // Unknown command — tell the player, keeping it CW-friendly
    lastCommand = "What? Key H for help.";
}

static void inputBufClear() {
    inputLen = 0;
    inputBuf[0] = '\0';
    inputDirty = true;
}

static void inputBufAppend(char c) {
    if (inputLen >= RC_INPUT_MAX) return;            // silently ignore overflow

    // The Morserino decoder appends a space after each word (end-of-word
    // detection). Drop leading spaces (nothing-yet) and collapse multiple
    // trailing spaces — they don't carry meaning for the command parser,
    // and a single inter-word space within a command (e.g. "pse qrs") is
    // already captured because it's followed by more letters.
    if (c == ' ' && (inputLen == 0 || inputBuf[inputLen - 1] == ' ')) {
        return;                                       // skip leading/double spaces
    }

    // Preserve case — the firmware uses lowercase for regular letters and
    // uppercase for prosigns (see encodeProSigns() in m32_v6.ino):
    //   'r' = letter R            'R' = <err>/<hh> prosign
    //   's' = letter S            'S' = <as> prosign
    //   'k' = letter K            'K' = <sk> prosign, etc.
    // We compare against lowercase command names in dispatchCommand().
    inputBuf[inputLen++] = c;
    inputBuf[inputLen] = '\0';
    inputDirty = true;
}

// Return true if the buffer, on its own, is an instant-dispatchable
// command (unambiguous single letter). Buffer holds lowercase letters for
// normal keyed characters.
static bool isInstantCommand(const char* buf) {
    if (strlen(buf) != 1) return false;
    char c = buf[0];
    // Unambiguous single-letter commands:
    //  s, e, w — movement (never ambiguous)
    //  i — inventory
    //  h — help
    //  y — only when we're awaiting NEW confirmation; otherwise leave it
    //      to the timeout (so unknown letters don't dispatch instantly)
    // Ambiguous single letters that must wait for the timeout:
    //  n — could be NORTH (alone) or NEW (with more chars)
    //  l — could be LOOK alone or LOOK [obj] (space-separated second token)
    //  t, u, d, r, f, k, q — command prefixes that need an argument
    if (c == 's' || c == 'e' || c == 'w' || c == 'i' || c == 'h') return true;
    if (c == 'y' && pendingNewConfirm) return true;
    return false;
}

// Called every loop. Decodes one keyed char (if any), appends, and dispatches
// when either instant-match or silence-timeout happens.
//
// SILENCE TIMEOUT LOGIC:
//   We measure ACTUAL SILENCE: the timer only runs when the keyer is fully
//   idle (no element in progress, no paddle held). Any activity resets the
//   silence clock.
//
//   Timeout value: 2.5× the firmware's interWordSpace. This automatically
//   respects the player's Farnsworth preferences (posInterWordSpace) — a
//   player with wide word spacing gets a longer timeout, which matches their
//   keying rhythm. At default 17 WPM with standard 7-dit spacing, the
//   timeout is ~1050 ms, generous enough to not fire mid-command but tight
//   enough that it feels responsive. Minimum floor of 400 ms protects very
//   fast keyers.
//
// ERROR / ABORT CONVENTION:
//   - "eeee" (4 consecutive E's keyed as separate letters) clears the buffer.
//   - The <hh> prosign convention is NOT used, because the decoder renders
//     <hh> as a single 'R' character — indistinguishable from a normal R
//     keyed in words like QRS, DROP, READ. Only eeee is unambiguous.
static void parserTick() {
    if (cwPlaying) return;                           // mute parser while CW plays

    // Drive the keyer — we re-use the existing Morse Invaders plumbing.
    // gameMode = true tells doPaddleIambic to deposit the decoded char
    // into gameCharBuffer.
    checkPaddles();
    doPaddleIambic(leftKey, rightKey);

    // Stuck-keyer protection (same as Morse Invaders).
    // Long idle periods between commands make this more likely here.
    static unsigned long lastIdleTime = 0;
    if (keyerState == IDLE_STATE) {
        lastIdleTime = millis();
    } else if (!leftKey && !rightKey && millis() - lastIdleTime > 2000) {
        keyerState = IDLE_STATE;
        clearPaddleLatches();
        lastIdleTime = millis();
    }

    char c = gameCharBuffer;
    if (c != 0) {
        gameCharBuffer = 0;

        // Keyer activity — keep the M32's idle shutdown timer at bay.
        MorseOutput::resetTOT();

        // Prosign handling (firmware convention: uppercase = prosign):
        //   'R' = <err>/<hh>   — error, please resend. Clear buffer.
        //   'S','A','N','K','E','B','H','U' = other prosigns. They flow
        //     into the buffer like any other character. The final-QSO
        //     phrases need this — "DE RC0 QSL <SK>" produces 'K' (= <sk>)
        //     which must reach the dispatcher. If a prosign appears
        //     accidentally in some other command, the dispatcher will
        //     just say "What?" and the player can retry.
        if (c == 'R') {
            inputBufClear();
            lastCommand = "(cleared — <hh>)";
            return;
        }

        inputBufAppend(c);

        // "eeee" error convention: four consecutive lowercase e's
        // (each keyed as a separate letter with inter-char gaps).
        if (inputLen >= 4 &&
            inputBuf[inputLen - 1] == 'e' &&
            inputBuf[inputLen - 2] == 'e' &&
            inputBuf[inputLen - 3] == 'e' &&
            inputBuf[inputLen - 4] == 'e') {
            inputBufClear();
            lastCommand = "(cleared — eeee)";
            return;
        }

        // Instant-dispatch check
        if (isInstantCommand(inputBuf)) {
            char tmp[RC_INPUT_MAX + 1];
            strcpy(tmp, inputBuf);
            inputBufClear();
            dispatchCommand(tmp);
            // Persist after every dispatch so the player can resume
            // mid-game after a timeout shutdown or menu exit. Terminal
            // states (RC_DEAD, RC_WON) clear the save themselves.
            if (rcState == RC_PLAYING) saveGame();
            return;
        }
    }

    // Silence-timeout check: only tick the clock when keyer is fully idle
    // AND no paddle is currently held. Any activity resets silenceStart.
    static unsigned long silenceStart = 0;
    bool keyerIdle = (keyerState == IDLE_STATE) && !leftKey && !rightKey;

    if (!keyerIdle) {
        silenceStart = 0;                            // activity — restart clock
    } else if (inputLen > 0) {
        if (silenceStart == 0) silenceStart = millis();

        // Timeout = 2.5 × interWordSpace, floored at 400 ms.
        // interWordSpace is in ms, already computed from prefs by updateTimings().
        unsigned long timeoutMs = (interWordSpace * 5UL) / 2UL;
        if (timeoutMs < 400) timeoutMs = 400;

        if (millis() - silenceStart >= timeoutMs) {
            char tmp[RC_INPUT_MAX + 1];
            strcpy(tmp, inputBuf);
            inputBufClear();
            dispatchCommand(tmp);
            if (rcState == RC_PLAYING) saveGame();
            silenceStart = 0;
        }
    } else {
        silenceStart = 0;                            // nothing in buffer, nothing to time
    }
}


//=============================================================================
// IN-GAME SCREEN drawing
//=============================================================================

// Wrap text into lines that fit within maxWidth pixels using the currently
// selected canvas font. Preserves paragraph breaks on '\n'. Results land in
// wrappedLines[], count in wrappedCount / totalLines.
static void wrapText(const String& text, int maxWidth) {
    wrappedCount = 0;
    scrollLine = 0;

    const int len = text.length();
    int i = 0;

    while (i < len && wrappedCount < RC_MAX_WRAP_LINES) {
        // Start of a new output line. Accumulate words until adding the next
        // would overflow maxWidth.
        String line = "";

        // Absorb any leading spaces on this line (they're just inter-word
        // gaps from the previous line). Do NOT skip newlines — those
        // produce empty lines intentionally.
        while (i < len && text[i] == ' ') i++;

        // Handle explicit paragraph break: emit empty line and move past \n.
        if (i < len && text[i] == '\n') {
            wrappedLines[wrappedCount++] = "";
            i++;
            continue;
        }

        while (i < len && text[i] != '\n') {
            // Find next word (run of non-space, non-newline)
            int wordStart = i;
            while (i < len && text[i] != ' ' && text[i] != '\n') i++;
            String word = text.substring(wordStart, i);

            // Build trial line
            String trial = (line.length() == 0) ? word : line + " " + word;
            if ((int)canvas->textWidth(trial) <= maxWidth) {
                line = trial;
            } else {
                // Overflow. If line is still empty, the single word is too
                // long for the width — emit it anyway (it'll clip).
                // Otherwise roll back: don't consume this word on this line.
                if (line.length() == 0) {
                    line = word;
                } else {
                    // Roll i back to the word start so it lands on the next line.
                    i = wordStart;
                }
                break;
            }

            // After the word, skip the single space separator (if any).
            // Leave newlines for the outer loop to handle.
            if (i < len && text[i] == ' ') i++;
        }

        wrappedLines[wrappedCount++] = line;

        // Skip the newline that ended this line (if any)
        if (i < len && text[i] == '\n') i++;
    }

    totalLines = wrappedCount;
}

// Returns "You also see: fuel canister, operating manual." (or similar) for
// items currently sitting in the given room. Returns empty string if no
// items are here. Only visible items are listed (e.g. the key in room 4
// is hidden until the generator runs).
//
// Special case: room 5 already bakes its items into its description text in
// the prototype's version, but our version uses the common appendRoomItems
// for ALL rooms, including room 5 — and room 5's description DOES NOT itself
// list the items, to avoid duplication.
static String roomItemsSuffix(uint8_t r) {
    // Count visible items
    int count = 0;
    for (uint8_t it = 1; it <= RC_NUM_ITEMS; it++) {
        if (itemVisibleInRoom(it, r)) count++;
    }
    if (count == 0) return "";

    String s = "\n\nYou see: ";
    int shown = 0;
    for (uint8_t it = 1; it <= RC_NUM_ITEMS; it++) {
        if (itemVisibleInRoom(it, r)) {
            if (shown > 0) {
                s += (shown == count - 1) ? " and " : ", ";
            }
            s += ITEMS[it].takeName;
            shown++;
        }
    }
    s += ".";
    return s;
}

// Recompute the wrapped lines for the current room description.
static void refreshRoomText(bool full) {
    canvas->setFont(&fonts::FreeSans9pt7b);
    String desc = roomDescription(game.room, full);
    desc += roomItemsSuffix(game.room);
    wrapText(desc, RC_SCREEN_W - 10);    // leave 6 px left margin + 4 px right (for scrollbar)
}

// Top bar: cave map (showing current location) + room name
static void drawTopBar() {
    canvas->fillRect(0, 0, RC_SCREEN_W, 30, RC_BG);

    // ----- Cave map -----
    //
    // A 32×24 floor-plan map of the cave system. Rooms are drawn as
    // rectangles whose sizes and positions are roughly proportional to
    // the actual layout, with shared walls visible between adjacent
    // rooms. NESW directions on the map match exits in the world:
    //
    //              [    10    ]                  outside, on top of mountain
    //              [    9     ]                  antenna tunnel
    //   [4][      ][    ][8 ][   ]
    //   [5][  3   ][   7  ][ 11]                main level
    //   [6][      ][      ][   ]
    //              [    2     ]                 cave entrance (south)
    //   [        forest 1           ]           outside, in front of cave
    //
    // The current room is filled in yellow; all other rooms render in dim
    // gray. The map is purely a spatial reference — it doesn't track
    // visited rooms and doesn't gate on locked doors. Rooms 4/5/6 connect
    // to 3 via one-way doors (the prototype's quirk); the map just shows
    // that they're adjacent.
    //
    // Coordinates are in the local map area, then offset by (mapX, mapY).
    static const int mapX = 4;
    static const int mapY = 3;

    // Faint frame around the map area for visual containment.
    canvas->drawRoundRect(mapX, mapY, 32, 24, 2, RC_FRAME);

    // Per-room rectangles (x, y, w, h) — local to mapX/mapY. Room 0 is
    // unused (sentinel) and room 12 is abstract (inside the safe).
    struct RoomRect { uint8_t x, y, w, h; };
    static const RoomRect RECTS[RC_NUM_ROOMS + 1] = {
        { 0,  0,  0,  0 },     // 0  unused
        { 1, 20, 29,  2 },     // 1  Forest Path     — wide thin bar at the bottom (outside)
        { 9, 16, 10,  2 },     // 2  Cave Entrance   — south of corridor
        { 9,  6, 10,  9 },     // 3  Main Corridor   — large central rectangle
        { 1,  6,  6,  2 },     // 4  Generator       — top of west column
        { 1,  9,  6,  2 },     // 5  Storage         — middle of west column
        { 1, 12,  6,  3 },     // 6  Workshop        — bottom of west column
        { 20,  9, 5,  6 },     // 7  Operating       — east of corridor (below 8)
        { 20,  6, 5,  2 },     // 8  Receiver        — north of operating
        { 9,  3, 10,  2 },     // 9  Antenna Tunnel  — wide bar above corridor
        { 4,  0, 20,  2 },     // 10 Antenna Platform — widest bar at the top (mountain top)
        { 26,  6, 4,  9 },     // 11 Captain's       — far-east tall rectangle (paired with 7+8)
        { 0,  0,  0,  0 }      // 12 The Safe        — abstract, not on the map
    };

    // Draw all rooms in dim gray.
    for (uint8_t r = 1; r <= RC_NUM_ROOMS; r++) {
        if (RECTS[r].w == 0) continue;
        canvas->fillRect(mapX + RECTS[r].x, mapY + RECTS[r].y,
                         RECTS[r].w, RECTS[r].h, RC_DIM);
    }

    // Highlight current room in bright yellow.
    if (game.room >= 1 && game.room <= RC_NUM_ROOMS && RECTS[game.room].w > 0) {
        canvas->fillRect(mapX + RECTS[game.room].x, mapY + RECTS[game.room].y,
                         RECTS[game.room].w, RECTS[game.room].h,
                         RC_HIGHLIGHT);
    }

    // ----- Room name -----
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextColor(RC_ACCENT, RC_BG);
    canvas->drawString(ROOMS[game.room].shortName, 44, 6);

    canvas->drawFastHLine(0, 28, RC_SCREEN_W, RC_FRAME);
}

// Main area: the wrapped room description, with optional scrollbar
static void drawMainArea() {
    canvas->fillRect(0, MAIN_Y - 1, RC_SCREEN_W, MAIN_BOTTOM - MAIN_Y + 1, RC_BG);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(RC_TEXT, RC_BG);
    canvas->setTextDatum(lgfx::top_left);

    for (int i = 0; i < VISIBLE_LINES; i++) {
        int idx = scrollLine + i;
        if (idx >= wrappedCount) break;
        canvas->drawString(wrappedLines[idx], 4, MAIN_Y + i * LINE_H);
    }

    // Scroll indicator on the right edge (only if content overflows)
    if (wrappedCount > VISIBLE_LINES) {
        int barTop    = MAIN_Y;
        int barHeight = MAIN_BOTTOM - MAIN_Y;
        int thumbTop  = barTop + (barHeight * scrollLine) / wrappedCount;
        int thumbH    = _max(8, (barHeight * VISIBLE_LINES) / wrappedCount);
        uint16_t col  = (encMode == RC_ENC_SCROLL) ? RC_HIGHLIGHT : RC_FRAME;
        canvas->drawFastVLine(RC_SCREEN_W - 3, barTop, barHeight, RC_FRAME);
        canvas->fillRect(RC_SCREEN_W - 5, thumbTop, 4, thumbH, col);
    }
}

// Input line + last-result line
static void drawInputLine() {
    canvas->fillRect(0, INPUT_Y - 1, RC_SCREEN_W, 18, RC_BG);
    canvas->setFont(&fonts::FreeSans9pt7b);

    if (inputLen > 0) {
        // Live buffer — uppercase for display (the buffer itself is lowercase
        // to preserve the letter-vs-prosign distinction internally)
        String shown(inputBuf);
        shown.toUpperCase();
        canvas->setTextColor(RC_OK, RC_BG);
        canvas->setTextDatum(lgfx::top_left);
        canvas->drawString("> ", 6, INPUT_Y);
        canvas->drawString(shown, 22, INPUT_Y);
        // Blinking cursor
        int cursorX = 22 + canvas->textWidth(shown);
        if ((millis() / 400) & 1)
            canvas->fillRect(cursorX + 1, INPUT_Y + 2, 6, 12, RC_OK);
    } else if (lastCommand.length() > 0) {
        // Last dispatched command
        canvas->setTextColor(RC_DIM, RC_BG);
        canvas->setTextDatum(lgfx::top_left);
        canvas->drawString("= ", 6, INPUT_Y);
        canvas->setTextColor(RC_TEXT, RC_BG);
        canvas->drawString(lastCommand, 22, INPUT_Y);
    } else {
        canvas->setTextColor(RC_DIM, RC_BG);
        canvas->setTextDatum(lgfx::top_left);
        canvas->drawString("key a command...", 6, INPUT_Y);
    }
}

// Bottom hint line: exits, step count, encoder mode indicator.
// When CW is playing, show "CW playing..." instead.
static void drawHintLine() {
    canvas->fillRect(0, HINT_Y - 1, RC_SCREEN_W, RC_SCREEN_H - HINT_Y + 1, RC_BG);
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(lgfx::top_left);

    if (cwPlaying) {
        canvas->setTextColor(RC_HIGHLIGHT, RC_BG);
        canvas->drawString("CW playing...", 6, HINT_Y);
        return;
    }

    // Exits — filtered through getExit() so locked doors don't show.
    char exbuf[12] = {0};
    int  ex = 0;
    if (getExit(game.room, RC_N) > 0) { exbuf[ex++] = 'N'; exbuf[ex++] = ' '; }
    if (getExit(game.room, RC_E) > 0) { exbuf[ex++] = 'E'; exbuf[ex++] = ' '; }
    if (getExit(game.room, RC_S) > 0) { exbuf[ex++] = 'S'; exbuf[ex++] = ' '; }
    if (getExit(game.room, RC_W) > 0) { exbuf[ex++] = 'W'; exbuf[ex++] = ' '; }
    if (ex > 0) exbuf[ex - 1] = '\0';               // trim trailing space
    else strcpy(exbuf, "-");

    canvas->setTextColor(RC_ACCENT, RC_BG);
    canvas->drawString(exbuf, 6, HINT_Y);

    // Step counter + inventory count centred. Format: "3/4 • 12"
    // Inventory on the left of the bullet, steps on the right.
    char sbuf[24];
    snprintf(sbuf, sizeof(sbuf), "%d/%d \u2022 %lu",
             (int)game.invCount, RC_INV_MAX, (unsigned long)game.steps);
    canvas->setTextColor(RC_DIM, RC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString(sbuf, RC_SCREEN_W / 2, HINT_Y);

    // Encoder-mode indicator on the right — show the live value when in
    // speed or volume mode so the user sees the effect of encoder turns.
    char modeBuf[12];
    uint16_t modeColor;
    switch (encMode) {
        case RC_ENC_SPEED:
            snprintf(modeBuf, sizeof(modeBuf), "%d wpm", MorsePreferences::wpm);
            modeColor = RC_DIM;
            break;
        case RC_ENC_VOLUME:
            snprintf(modeBuf, sizeof(modeBuf), "vol %d", MorsePreferences::sidetoneVolume);
            modeColor = RC_HIGHLIGHT;
            break;
        case RC_ENC_SCROLL:
        default:
            strcpy(modeBuf, "scroll");
            modeColor = RC_HIGHLIGHT;
            break;
    }
    canvas->setTextColor(modeColor, RC_BG);
    canvas->setTextDatum(lgfx::top_right);
    canvas->drawString(modeBuf, RC_SCREEN_W - 6, HINT_Y);
    canvas->setTextDatum(lgfx::top_left);
}

static void drawGameScreen() {
    canvas->fillSprite(RC_BG);
    drawTopBar();
    drawMainArea();
    drawInputLine();
    drawHintLine();
    pushFrame();
}

// Partial redraws to keep frame rate up
static void redrawAsNeeded() {
    static unsigned long lastFullRedraw = 0;
    static unsigned long lastCursorBlink = 0;
    static bool cwWasPlaying = false;
    unsigned long now = millis();

    bool needFull = false;
    if (inputDirty) { needFull = true; inputDirty = false; }
    if (cwPlaying != cwWasPlaying) { needFull = true; cwWasPlaying = cwPlaying; }
    if (now - lastFullRedraw > 250) needFull = true;    // periodic refresh
    if (!needFull && now - lastCursorBlink > 200 && inputLen > 0) {
        // Cheap: just the input line, for the cursor blink
        drawInputLine();
        pushFrame();
        lastCursorBlink = now;
        return;
    }
    if (needFull) {
        drawGameScreen();
        lastFullRedraw = now;
        lastCursorBlink = now;
    }
}


//=============================================================================
// MOVEMENT
//=============================================================================

// Move the player in the given direction. Returns true if the move succeeded
// (a valid exit existed). The step counter is incremented regardless — any
// command keyed by the player counts as a step, even a bumped wall.
static bool movePlayer(RcDir dir) {
    int8_t dest = getExit(game.room, dir);
    if (dest < 0) {
        lastCommand = "You can't go that way.";
        return false;
    }
    game.room = dest;
    // Decide full vs short description BEFORE marking the room visited,
    // so the FIRST visit gets the long form.
    bool firstVisit = !roomVisited(dest);
    markVisited(dest);
    refreshRoomText(firstVisit);
    lastCommand = "";                  // room text speaks for itself
    inputDirty  = true;

    // Room 8 (Receiver Alcove): if the station is receiving properly AND
    // the initial CQ hasn't yet been responded to, play the incoming CW
    // call. cwPlayerStart() seeds lastClue so QRS works.
    if (dest == 8 && game.generator && !game.antennaGrounded &&
        game.qsoStage == 0) {
        cwPlayerStart(String("rc0 rc0 de ir7 ir7 k"), 30);
    }

    return true;
}


//=============================================================================
// DEATH screen
//=============================================================================
//
// Entered when the player dies (currently: touching plate connector with
// master power on). Shows a red death message and waits for any paddle
// touch or mode-button click to restart (which transitions back to
// RC_PLAYING, which calls newGame()).

static void drawDeathScreen() {
    canvas->fillSprite(RC_BG);

    // Red top band
    canvas->fillRect(0, 0, RC_SCREEN_W, 30, RC_DANGER);
    canvas->setFont(&fonts::FreeSansBold18pt7b);
    canvas->setTextColor(RC_BG, RC_DANGER);
    canvas->setTextDatum(lgfx::middle_center);
    canvas->drawString("GAME OVER", RC_SCREEN_W / 2, 15);
    canvas->setTextDatum(lgfx::top_left);

    // Body
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(RC_TEXT, RC_BG);
    wrapText(F("A blinding arc of high voltage leaps through your "
               "hand. Everything goes dark.\n\n"
               "You suffered a painful death by high voltage.\n\n"
               "IR7 continues to call... waiting."),
             RC_SCREEN_W - 20);
    for (int i = 0; i < wrappedCount && i < 7; i++) {
        canvas->drawString(wrappedLines[i], 10, 44 + i * LINE_H);
    }

    // Bottom prompt
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(RC_DANGER, RC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString("[ key any letter or click to restart ]",
                       RC_SCREEN_W / 2, 154);
    canvas->setTextDatum(lgfx::top_left);

    pushFrame();
}

static void deathLoop() {
    // Any lingering CW playback from the death moment should be silenced.
    cwPlayerStop();
    inputBufClear();
    lastCommand = "";
    gameCharBuffer = 0;
    clearPaddleLatches();

    drawDeathScreen();

    while (rcState == RC_DEAD) {
        // Any keyed character → restart
        if (gameCharBuffer != 0) {
            gameCharBuffer = 0;
            MorseOutput::resetTOT();
            rcState = RC_PLAYING;
            return;
        }
        // Mode-button click → restart
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == 1) {
            rcState = RC_PLAYING;
            return;
        }
        if (Buttons::modeButton.clicks == -1) {
            rcState = RC_EXIT;
            return;
        }
        // FN long-press → exit to menu
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks == -1) {
            rcState = RC_EXIT;
            return;
        }

        // Keep paddle activity feeding gameCharBuffer (we stay in gameMode)
        doPaddleIambic(leftKey, rightKey);
        checkPaddles();

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}


//=============================================================================
// VICTORY screen
//=============================================================================
//
// Entered when the player completes the final QSO. Plays IR7's final
// acknowledgment in CW, shows the challenge-complete message. Any paddle
// or mode-click returns to the menu.

static void drawWonScreen() {
    canvas->fillSprite(RC_BG);

    // Green top band
    canvas->fillRect(0, 0, RC_SCREEN_W, 30, RC_OK);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextColor(RC_BG, RC_OK);
    canvas->setTextDatum(lgfx::middle_center);
    canvas->drawString("CHALLENGE COMPLETE", RC_SCREEN_W / 2, 15);
    canvas->setTextDatum(lgfx::top_left);

    // Body
    char body[256];
    snprintf(body, sizeof(body),
             "After 50 years of silence, station RC0 "
             "has returned to the air. Your message has "
             "been received.\n\n"
             "        73 de IR7\n\n"
             "Steps taken: %lu",
             (unsigned long)game.steps);
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(RC_TEXT, RC_BG);
    wrapText(String(body), RC_SCREEN_W - 20);
    for (int i = 0; i < wrappedCount && i < 7; i++) {
        canvas->drawString(wrappedLines[i], 10, 44 + i * LINE_H);
    }

    // Bottom prompt
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(RC_OK, RC_BG);
    canvas->setTextDatum(lgfx::top_center);
    canvas->drawString("[ key any letter or click to exit ]",
                       RC_SCREEN_W / 2, 154);
    canvas->setTextDatum(lgfx::top_left);

    pushFrame();
}

static void wonLoop() {
    // Play IR7's final 73 greeting in CW.
    cwPlayerStart(String("r r tu 73 K K K"), 16);
    inputBufClear();
    lastCommand = "";
    gameCharBuffer = 0;
    clearPaddleLatches();

    drawWonScreen();

    while (rcState == RC_WON) {
        cwPlayerTick();                    // let the final 73 play out

        // Any keyed character → exit
        if (gameCharBuffer != 0) {
            gameCharBuffer = 0;
            MorseOutput::resetTOT();
            cwPlayerStop();
            rcState = RC_EXIT;
            return;
        }
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks != 0) {
            cwPlayerStop();
            rcState = RC_EXIT;
            return;
        }
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::volButton.clicks != 0) {
            cwPlayerStop();
            rcState = RC_EXIT;
            return;
        }

        doPaddleIambic(leftKey, rightKey);
        checkPaddles();

        serialEvent();
        checkShutDown(false);
        delay(10);
    }
}


//=============================================================================
// PLAY loop
//=============================================================================

static void playLoop() {
    // Try to resume from a previous save. If none exists or it's invalid,
    // start fresh. This is what makes Radio Cave durable across shutdowns
    // — a 5-minute idle timeout, a manual exit, or a power-off all leave
    // the save behind, and the next entry picks up where the player was.
    bool resumed = loadGame();
    if (!resumed) {
        newGame();
    }
    markVisited(game.room);
    encMode = RC_ENC_SPEED;
    inputBufClear();
    lastCommand = resumed ? "Welcome back." : "";
    lastClue    = "";
    lastClueWpm = RC_CLUE_WPM;
    pendingNewConfirm = false;
    gameMode = true;
    clearPaddleLatches();
    gameCharBuffer = 0;

    // Show short-form description on resume (it's a familiar room), full
    // form on a fresh start.
    refreshRoomText(!resumed);
    drawGameScreen();

    while (rcState == RC_PLAYING) {
        // --- CW player service tick (first — timing sensitive) ---
        cwPlayerTick();

        // --- Command parser tick (no-op while CW plays) ---
        parserTick();

        // --- Encoder ---
        int enc = checkEncoder();
        if (enc) {
            MorseOutput::resetTOT();   // encoder turn = user activity
            switch (encMode) {
                case RC_ENC_SPEED: {
                    int newWpm = constrain((int)MorsePreferences::wpm + enc,
                                           (int)MorsePreferences::wpmMin,
                                           (int)MorsePreferences::wpmMax);
                    MorsePreferences::wpm = newWpm;
                    updateTimings();
                    MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                    inputDirty = true;
                    break;
                }
                case RC_ENC_VOLUME: {
                    int newVol = constrain((int)MorsePreferences::sidetoneVolume + enc, 0, 20);
                    MorsePreferences::sidetoneVolume = newVol;
                    MorseOutput::pwmClick(newVol);
                    inputDirty = true;
                    break;
                }
                case RC_ENC_SCROLL: {
                    int newLine = scrollLine + enc;
                    int maxLine = _max(0, wrappedCount - VISIBLE_LINES);
                    scrollLine = constrain(newLine, 0, maxLine);
                    MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                    inputDirty = true;
                    break;
                }
            }
        }

        // --- FN (volButton) ---
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks != 0) MorseOutput::resetTOT();
        switch (Buttons::volButton.clicks) {
            case 1:
                // Short press: toggle speed ↔ volume (exit scroll if in scroll)
                if (encMode == RC_ENC_SCROLL) {
                    encMode = encModeBeforeScroll;
                } else if (encMode == RC_ENC_SPEED) {
                    encMode = RC_ENC_VOLUME;
                } else {
                    encMode = RC_ENC_SPEED;
                }
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                inputDirty = true;
                break;
            case -1:
                // Long press: toggle scroll mode
                if (encMode == RC_ENC_SCROLL) {
                    encMode = encModeBeforeScroll;
                } else {
                    encModeBeforeScroll = encMode;
                    encMode = RC_ENC_SCROLL;
                }
                MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
                inputDirty = true;
                break;
        }

        // --- Mode button: long-press = exit. Short-press currently unused. ---
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks != 0) MorseOutput::resetTOT();
        if (Buttons::modeButton.clicks == -1) {
            cwPlayerStop();
            gameMode = false;
            clearPaddleLatches();
            rcState = RC_EXIT;
            return;
        }

        redrawAsNeeded();

        serialEvent();
        checkShutDown(false);
        delay(2);
    }

    cwPlayerStop();
    gameMode = false;
    clearPaddleLatches();
}


//=============================================================================
// Main entry point
//=============================================================================

void MorseRadioCave::run() {
    canvas = display.enterGameModeLandscape(MorsePreferences::leftHanded);
    if (!canvas) return;

    rcState = RC_LOBBY;

    while (rcState != RC_EXIT) {
        switch (rcState) {
            case RC_LOBBY:   lobbyLoop();  break;
            case RC_PLAYING: playLoop();   break;
            case RC_DEAD:    deathLoop();  break;
            case RC_WON:     wonLoop();    break;
            default:         rcState = RC_EXIT; break;
        }
    }

    // Make sure we leave the keyer and audio in a clean state
    keyOut(false, true, 0, 0);
    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
    gameMode = false;
    clearPaddleLatches();

    display.exitGameMode();

    // exitGameMode() calls setRotation(3) — that's landscape, correct for
    // Radio Cave on re-entry but WRONG for the Morserino menu which expects
    // portrait. When a portrait-mode game (Invaders/Pileup) exits, the
    // rotation change to 3 coincidentally doesn't matter because... [historical
    // reason]. Coming out of landscape, the menu stays landscape and looks
    // 90°-rotated after the first encoder turn.
    //
    // Fix: explicitly restore the menu's portrait rotation. The menu uses
    // rotation 2 normally, or 0 for left-handed users.
    DisplayWrapper::getLGFX()->setRotation(MorsePreferences::leftHanded ? 0 : 2);

    // Restore normal display for menu (same sequence as MorseGame::run)
    MorseOutput::initDisplay();
    #ifdef CONFIG_DISPLAYWRAPPER
    MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
    #endif

    pinMode(PinCLK, INPUT_PULLUP);
    pinMode(PinDT,  INPUT_PULLUP);
    rotaryEncoder.attachHalfQuad(PinDT, PinCLK);
    rotaryEncoder.setCount(0);
}

#endif  // CONFIG_CW_GAME