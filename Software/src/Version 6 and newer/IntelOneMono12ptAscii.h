#ifndef INTELONEMONO12PT_ASCII_H_
#define INTELONEMONO12PT_ASCII_H_

// Small scroll-area font on the M32 Pocket (posScrollFont == Small): the same
// glyph bitmaps as DisplayWrapper's IntelOneMono_{Regular,Bold}12pt8b, but
// declared over a narrower Unicode range - printable ASCII, 0x20-0x7E -
// instead of the library's own 0x20-0xFF.
//
// Why: LovyanGFX's GFXfont::getDefaultMetric() (what display.getStringHeight()
// reads to compute the scroll area's per-line pixel pitch) scans the font's
// *entire* first..last range for the tallest ascent/deepest descent, not just
// the glyphs actually drawn. IntelOneMono's own 0x20-0xFF range includes
// accented Latin-1 characters this firmware's English-only UI never draws
// (CLAUDE.md section 5), which inflates the reported line height from a true
// ~25px (over the glyphs this app actually uses) to ~29px. That 4px is
// exactly what stood between 4 and 5 visible lines at the small font on the
// Pocket (SCROLL_TOP=29px, 141px available: 5*25=125 fits with 16px to
// spare, 5*29=145 doesn't - a prior attempt to just hand-pick a tighter
// LINE_HEIGHT instead of fixing the underlying range clipped every line's
// bottom pixel row, since actual glyph *positioning* also comes from this
// same whole-range metric, not just the reported height).
//
// GFXfont::getGlyph() range-checks the codepoint against [first, last] and
// returns nullptr (rendered as blank) outside it (lgfx_fonts.cpp); inside the
// range it indexes into the glyph array as (codepoint - first), same as
// before. So reusing the identical bitmap/glyph data with a truncated `last`
// renders every character still in range exactly as before, and anything
// beyond it as blank rather than a wrong glyph (see the (c) trade-off below)
// - this is a metadata-only reinterpretation, not a new font asset. The
// #include below duplicates ~7KB of already-shipped bitmap data
// into this translation unit rather than reusing DisplayWrapper.cpp's copy:
// those arrays are plain `const` at namespace scope, which in C++ (unlike
// C) defaults to internal linkage, so they aren't visible outside the file
// that defines them and can't be extern-declared from here.
//
// Trade-off: the firmware's one non-ASCII character, the (c) in the startup
// splash's copyright line (morsedefs.h COPYRIGHT, U+00A9 = 0xA9), falls
// outside 0x20-0x7E. Extending the range to include it pulls the tight
// height back up to 29px (some other codepoint between 0x7F and 0xA9 is as
// tall as the accented capitals this is trying to exclude), so it isn't
// included. It renders as a blank space instead of "(c)" specifically when
// the small scroll font is active; everything else in the printable-ASCII
// range this app ever draws is unaffected.

// These quoted includes resolve to the in-tree copies in this same directory
// (pre-existing, left over from #157), not DisplayWrapper's - the compiler's
// same-directory-first lookup wins over the library search path. The two are
// byte-identical today, but that's now something to maintain: if the library
// ever updates these fonts, the in-tree copies used here won't follow unless
// someone re-syncs them by hand. Treat the in-tree copies as the source of
// truth for what this header renders.
#include <LovyanGFX.hpp>
#include "IntelOneMono_Regular12pt8b.h"   // -> IntelOneMono_Regular12pt8bBitmaps/Glyphs (+ the unused full-range IntelOneMono_Regular12pt8b, dropped at link time)
#include "IntelOneMono_Bold12pt8b.h"      // -> IntelOneMono_Bold12pt8bBitmaps/Glyphs    (+ the unused full-range IntelOneMono_Bold12pt8b, dropped at link time)

const lgfx::GFXfont IntelOneMono_Regular12pt8b_Ascii PROGMEM = {
  (uint8_t *) IntelOneMono_Regular12pt8bBitmaps,
  (lgfx::GFXglyph *) IntelOneMono_Regular12pt8bGlyphs,
  0x20, 0x7E, 32
};

const lgfx::GFXfont IntelOneMono_Bold12pt8b_Ascii PROGMEM = {
  (uint8_t *) IntelOneMono_Bold12pt8bBitmaps,
  (lgfx::GFXglyph *) IntelOneMono_Bold12pt8bGlyphs,
  0x20, 0x7E, 32
};

#endif
