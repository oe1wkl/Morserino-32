#ifndef INTELONEMONO12PT_SCROLL_H_
#define INTELONEMONO12PT_SCROLL_H_

// Small scroll-area font on the M32 Pocket (posScrollFont == Small): the same
// glyph bitmaps as DisplayWrapper's IntelOneMono_{Regular,Bold}12pt8b, re-declared
// over the codepoints this firmware actually draws - printable ASCII, plus the
// German umlauts and the (c) of the splash - instead of the library's 0x20-0xFF.
//
// Why not just use the library font: LovyanGFX's GFXfont::getDefaultMetric()
// (what display.getStringHeight() reads to compute the scroll area's per-line
// pixel pitch) scans the font's glyphs for the tallest ascent and deepest
// descent. Over the full 0x20-0xFF that lands on 0x9E 0x9F 0xA4 0xA6 0xAC 0xAD -
// which are not letters at all but the font's placeholder boxes, 29px tall - and
// inflates the line height from a true 25px to 29px. That 4px is exactly what
// stands between 4 and 5 visible lines at the small font (SCROLL_TOP=29px, 141px
// available: 5*25=125 fits, 5*29=145 does not).
//
// An earlier version of this file solved that by truncating the range to
// 0x20-0x7E. That worked for the metric, but it also meant the CW decoder's
// German umlauts - ".-.-" is a legitimate "ä", one dit short of "+" - rendered as
// LovyanGFX's dummy box, as did the splash's (c). Reported on the bench, 2026-09.
//
// This version instead uses LovyanGFX's EncodeRange support, which getGlyph() and
// getDefaultMetric() both honour: the ranges below cover ASCII + (c) + the seven
// German characters and nothing else, so the metric scan never sees a placeholder
// box. Measured over these 103 glyphs: ascent 20 + descent 7 = 27px, so 5*27=135px
// still fits the 141px available - the fifth line survives, at a cost of 2px of
// line pitch, and the umlauts and the (c) render properly.
//
// The glyph table below is a compacted copy of the library's entries for exactly
// those codepoints (getDefaultMetric() scans glyph[0..numChars-1], so the entries
// the ranges name must sit contiguously from index 0). It points at the library's
// unchanged Bitmaps arrays via the bitmapOffset field - this remains a
// metadata-only reinterpretation, not a new font asset.

// These quoted includes resolve to the in-tree copies in this same directory
// (pre-existing, left over from #157), not DisplayWrapper's - the compiler's
// same-directory-first lookup wins over the library search path. The two are
// byte-identical today, but that's something to maintain: if the library ever
// updates these fonts, the in-tree copies used here won't follow unless someone
// re-syncs them by hand, AND the glyph table below must be regenerated with them.
// Treat the in-tree copies as the source of truth for what this header renders.
#include <LovyanGFX.hpp>
#include "IntelOneMono_Regular12pt8b.h"   // -> IntelOneMono_Regular12pt8bBitmaps/Glyphs
#include "IntelOneMono_Bold12pt8b.h"      // -> IntelOneMono_Bold12pt8bBitmaps/Glyphs

// start, end, base: base is the index into the glyph table at which `start` sits.
static const lgfx::EncodeRange IntelOneMono12ptScrollRanges[] PROGMEM = {
  { 0x20, 0x7E,   0 },   // printable ASCII
  { 0xA9, 0xA9,  95 },   // (c)
  { 0xC4, 0xC4,  96 },   // Ae
  { 0xD6, 0xD6,  97 },   // Oe
  { 0xDC, 0xDC,  98 },   // Ue
  { 0xDF, 0xDF,  99 },   // ss
  { 0xE4, 0xE4, 100 },   // ae
  { 0xF6, 0xF6, 101 },   // oe
  { 0xFC, 0xFC, 102 },   // ue
};
#define INTELONEMONO12PT_SCROLL_RANGES 9

static const lgfx::GFXglyph IntelOneMono_Regular12pt8b_ScrollGlyphs[] PROGMEM = {
  {     0,   1,   1,  15,    0,    0 },   // [  0] 0x20 space
  {     1,   4,  17,  15,    5,  -16 },   // [  1] 0x21 '!'
  {    10,   9,   7,  15,    3,  -16 },   // [  2] 0x22 '"'
  {    18,  13,  16,  15,    1,  -15 },   // [  3] 0x23 '#'
  {    44,  11,  22,  15,    2,  -18 },   // [  4] 0x24 '$'
  {    75,  13,  16,  15,    1,  -15 },   // [  5] 0x25 '%'
  {   101,  13,  16,  15,    1,  -15 },   // [  6] 0x26 '&'
  {   127,   3,   7,  15,    6,  -16 },   // [  7] 0x27 '''
  {   130,  10,  23,  15,    3,  -18 },   // [  8] 0x28 '('
  {   159,  10,  23,  15,    2,  -18 },   // [  9] 0x29 ')'
  {   188,  12,  11,  15,    1,  -14 },   // [ 10] 0x2A '*'
  {   205,  11,  12,  15,    2,  -13 },   // [ 11] 0x2B '+'
  {   222,   6,   7,  15,    3,   -2 },   // [ 12] 0x2C ','
  {   228,   9,   2,  15,    3,   -8 },   // [ 13] 0x2D '-'
  {   231,   4,   4,  15,    5,   -3 },   // [ 14] 0x2E '.'
  {   233,  13,  20,  15,    1,  -17 },   // [ 15] 0x2F '/'
  {   266,  11,  16,  15,    2,  -15 },   // [ 16] 0x30 '0'
  {   288,  10,  16,  15,    3,  -15 },   // [ 17] 0x31 '1'
  {   308,  11,  16,  15,    2,  -15 },   // [ 18] 0x32 '2'
  {   330,  11,  16,  15,    2,  -15 },   // [ 19] 0x33 '3'
  {   352,  13,  16,  15,    1,  -15 },   // [ 20] 0x34 '4'
  {   378,  11,  16,  15,    2,  -15 },   // [ 21] 0x35 '5'
  {   400,  11,  16,  15,    2,  -15 },   // [ 22] 0x36 '6'
  {   422,  12,  16,  15,    1,  -15 },   // [ 23] 0x37 '7'
  {   446,  11,  16,  15,    2,  -15 },   // [ 24] 0x38 '8'
  {   468,  11,  16,  15,    2,  -15 },   // [ 25] 0x39 '9'
  {   490,   4,  11,  15,    5,  -10 },   // [ 26] 0x3A ':'
  {   496,   7,  15,  15,    3,  -10 },   // [ 27] 0x3B ';'
  {   510,  11,  12,  15,    2,  -13 },   // [ 28] 0x3C '<'
  {   527,  11,   7,  15,    2,  -10 },   // [ 29] 0x3D '='
  {   537,  11,  12,  15,    2,  -13 },   // [ 30] 0x3E '>'
  {   554,  11,  17,  15,    2,  -16 },   // [ 31] 0x3F '?'
  {   578,  12,  19,  15,    1,  -16 },   // [ 32] 0x40 '@'
  {   607,  13,  16,  15,    1,  -15 },   // [ 33] 0x41 'A'
  {   633,  12,  16,  15,    2,  -15 },   // [ 34] 0x42 'B'
  {   657,  13,  16,  15,    1,  -15 },   // [ 35] 0x43 'C'
  {   683,  11,  16,  15,    2,  -15 },   // [ 36] 0x44 'D'
  {   705,  11,  16,  15,    2,  -15 },   // [ 37] 0x45 'E'
  {   727,  11,  16,  15,    2,  -15 },   // [ 38] 0x46 'F'
  {   749,  12,  16,  15,    1,  -15 },   // [ 39] 0x47 'G'
  {   773,  11,  16,  15,    2,  -15 },   // [ 40] 0x48 'H'
  {   795,   9,  16,  15,    3,  -15 },   // [ 41] 0x49 'I'
  {   813,  11,  16,  15,    1,  -15 },   // [ 42] 0x4A 'J'
  {   835,  12,  16,  15,    2,  -15 },   // [ 43] 0x4B 'K'
  {   859,  11,  16,  15,    2,  -15 },   // [ 44] 0x4C 'L'
  {   881,  12,  16,  15,    1,  -15 },   // [ 45] 0x4D 'M'
  {   905,  11,  16,  15,    2,  -15 },   // [ 46] 0x4E 'N'
  {   927,  12,  16,  15,    1,  -15 },   // [ 47] 0x4F 'O'
  {   951,  12,  16,  15,    2,  -15 },   // [ 48] 0x50 'P'
  {   975,  14,  19,  15,    1,  -15 },   // [ 49] 0x51 'Q'
  {  1009,  12,  16,  15,    2,  -15 },   // [ 50] 0x52 'R'
  {  1033,  11,  16,  15,    2,  -15 },   // [ 51] 0x53 'S'
  {  1055,  13,  16,  15,    1,  -15 },   // [ 52] 0x54 'T'
  {  1081,  11,  16,  15,    2,  -15 },   // [ 53] 0x55 'U'
  {  1103,  13,  16,  15,    1,  -15 },   // [ 54] 0x56 'V'
  {  1129,  13,  16,  15,    1,  -15 },   // [ 55] 0x57 'W'
  {  1155,  13,  16,  15,    1,  -15 },   // [ 56] 0x58 'X'
  {  1181,  13,  16,  15,    1,  -15 },   // [ 57] 0x59 'Y'
  {  1207,  11,  16,  15,    2,  -15 },   // [ 58] 0x5A 'Z'
  {  1229,   8,  22,  15,    5,  -18 },   // [ 59] 0x5B '['
  {  1251,  13,  20,  15,    1,  -17 },   // [ 60] 0x5C backslash
  {  1284,   8,  22,  15,    2,  -18 },   // [ 61] 0x5D ']'
  {  1306,  13,   9,  15,    1,  -15 },   // [ 62] 0x5E '^'
  {  1321,  11,   2,  15,    2,    2 },   // [ 63] 0x5F '_'
  {  1324,   6,   7,  15,    4,  -16 },   // [ 64] 0x60 '`'
  {  1330,  12,  11,  15,    2,  -10 },   // [ 65] 0x61 'a'
  {  1347,  11,  17,  15,    2,  -16 },   // [ 66] 0x62 'b'
  {  1371,  11,  11,  15,    2,  -10 },   // [ 67] 0x63 'c'
  {  1387,  12,  17,  15,    1,  -16 },   // [ 68] 0x64 'd'
  {  1413,  11,  11,  15,    2,  -10 },   // [ 69] 0x65 'e'
  {  1429,  11,  17,  15,    2,  -16 },   // [ 70] 0x66 'f'
  {  1453,  12,  17,  15,    1,  -10 },   // [ 71] 0x67 'g'
  {  1479,  11,  17,  15,    2,  -16 },   // [ 72] 0x68 'h'
  {  1503,  10,  17,  15,    3,  -16 },   // [ 73] 0x69 'i'
  {  1525,   8,  23,  15,    2,  -16 },   // [ 74] 0x6A 'j'
  {  1548,  10,  17,  15,    3,  -16 },   // [ 75] 0x6B 'k'
  {  1570,  12,  17,  15,    1,  -16 },   // [ 76] 0x6C 'l'
  {  1596,  13,  11,  15,    1,  -10 },   // [ 77] 0x6D 'm'
  {  1614,  11,  11,  15,    2,  -10 },   // [ 78] 0x6E 'n'
  {  1630,  11,  11,  15,    2,  -10 },   // [ 79] 0x6F 'o'
  {  1646,  11,  17,  15,    2,  -10 },   // [ 80] 0x70 'p'
  {  1670,  12,  17,  15,    1,  -10 },   // [ 81] 0x71 'q'
  {  1696,  12,  11,  15,    1,  -10 },   // [ 82] 0x72 'r'
  {  1713,  11,  11,  15,    2,  -10 },   // [ 83] 0x73 's'
  {  1729,  12,  15,  15,    1,  -14 },   // [ 84] 0x74 't'
  {  1752,  10,  11,  15,    2,  -10 },   // [ 85] 0x75 'u'
  {  1766,  12,  11,  15,    1,  -10 },   // [ 86] 0x76 'v'
  {  1783,  13,  11,  15,    1,  -10 },   // [ 87] 0x77 'w'
  {  1801,  11,  11,  15,    2,  -10 },   // [ 88] 0x78 'x'
  {  1817,  12,  17,  15,    1,  -10 },   // [ 89] 0x79 'y'
  {  1843,  11,  11,  15,    2,  -10 },   // [ 90] 0x7A 'z'
  {  1859,  11,  22,  15,    2,  -18 },   // [ 91] 0x7B '{'
  {  1890,   2,  20,  15,    6,  -17 },   // [ 92] 0x7C '|'
  {  1895,  11,  22,  15,    2,  -18 },   // [ 93] 0x7D '}'
  {  1926,  13,   5,  15,    1,   -9 },   // [ 94] 0x7E '~'
  {  3594,  12,  19,  15,    1,  -16 },   // [ 95] 0xA9 (c)
  {  4206,  13,  21,  15,    1,  -20 },   // [ 96] 0xC4 Ae
  {  4760,  12,  21,  15,    1,  -20 },   // [ 97] 0xD6 Oe
  {  4934,  11,  21,  15,    2,  -20 },   // [ 98] 0xDC Ue
  {  5025,  12,  17,  15,    2,  -16 },   // [ 99] 0xDF ss
  {  5159,  12,  16,  15,    2,  -15 },   // [100] 0xE4 ae
  {  5588,  11,  16,  15,    2,  -15 },   // [101] 0xF6 oe
  {  5719,  10,  16,  15,    2,  -15 },   // [102] 0xFC ue
};

static const lgfx::GFXglyph IntelOneMono_Bold12pt8b_ScrollGlyphs[] PROGMEM = {
  {     0,   1,   1,  15,    0,    0 },   // [  0] 0x20 space
  {     1,   7,  17,  15,    4,  -16 },   // [  1] 0x21 '!'
  {    16,  11,   8,  15,    2,  -16 },   // [  2] 0x22 '"'
  {    27,  13,  16,  15,    1,  -15 },   // [  3] 0x23 '#'
  {    53,  12,  22,  15,    1,  -18 },   // [  4] 0x24 '$'
  {    86,  14,  16,  15,    0,  -15 },   // [  5] 0x25 '%'
  {   114,  13,  16,  15,    1,  -15 },   // [  6] 0x26 '&'
  {   140,   5,   8,  15,    5,  -16 },   // [  7] 0x27 '''
  {   145,  11,  23,  15,    2,  -18 },   // [  8] 0x28 '('
  {   177,  11,  23,  15,    1,  -18 },   // [  9] 0x29 ')'
  {   209,  12,  11,  15,    1,  -14 },   // [ 10] 0x2A '*'
  {   226,  12,  12,  15,    1,  -12 },   // [ 11] 0x2B '+'
  {   244,   8,   8,  15,    2,   -3 },   // [ 12] 0x2C ','
  {   252,   9,   3,  15,    3,   -8 },   // [ 13] 0x2D '-'
  {   256,   6,   5,  15,    4,   -4 },   // [ 14] 0x2E '.'
  {   260,  13,  20,  15,    1,  -17 },   // [ 15] 0x2F '/'
  {   293,  11,  16,  15,    2,  -15 },   // [ 16] 0x30 '0'
  {   315,  11,  16,  15,    2,  -15 },   // [ 17] 0x31 '1'
  {   337,  12,  16,  15,    1,  -15 },   // [ 18] 0x32 '2'
  {   361,  11,  16,  15,    2,  -15 },   // [ 19] 0x33 '3'
  {   383,  13,  16,  15,    1,  -15 },   // [ 20] 0x34 '4'
  {   409,  11,  16,  15,    2,  -15 },   // [ 21] 0x35 '5'
  {   431,  12,  16,  15,    1,  -15 },   // [ 22] 0x36 '6'
  {   455,  12,  16,  15,    1,  -15 },   // [ 23] 0x37 '7'
  {   479,  12,  16,  15,    1,  -15 },   // [ 24] 0x38 '8'
  {   503,  12,  16,  15,    1,  -15 },   // [ 25] 0x39 '9'
  {   527,   6,  11,  15,    4,  -10 },   // [ 26] 0x3A ':'
  {   536,   9,  15,  15,    2,  -10 },   // [ 27] 0x3B ';'
  {   553,  12,  13,  15,    1,  -13 },   // [ 28] 0x3C '<'
  {   573,  11,   9,  15,    2,  -11 },   // [ 29] 0x3D '='
  {   586,  11,  13,  15,    2,  -13 },   // [ 30] 0x3E '>'
  {   604,  12,  17,  15,    1,  -16 },   // [ 31] 0x3F '?'
  {   630,  12,  19,  15,    1,  -16 },   // [ 32] 0x40 '@'
  {   659,  13,  16,  15,    1,  -15 },   // [ 33] 0x41 'A'
  {   685,  12,  16,  15,    2,  -15 },   // [ 34] 0x42 'B'
  {   709,  13,  16,  15,    1,  -15 },   // [ 35] 0x43 'C'
  {   735,  12,  16,  15,    2,  -15 },   // [ 36] 0x44 'D'
  {   759,  11,  16,  15,    2,  -15 },   // [ 37] 0x45 'E'
  {   781,  11,  16,  15,    2,  -15 },   // [ 38] 0x46 'F'
  {   803,  12,  16,  15,    1,  -15 },   // [ 39] 0x47 'G'
  {   827,  11,  16,  15,    2,  -15 },   // [ 40] 0x48 'H'
  {   849,   9,  16,  15,    3,  -15 },   // [ 41] 0x49 'I'
  {   867,  12,  16,  15,    1,  -15 },   // [ 42] 0x4A 'J'
  {   891,  13,  16,  15,    2,  -15 },   // [ 43] 0x4B 'K'
  {   917,  11,  16,  15,    2,  -15 },   // [ 44] 0x4C 'L'
  {   939,  12,  16,  15,    1,  -15 },   // [ 45] 0x4D 'M'
  {   963,  11,  16,  15,    2,  -15 },   // [ 46] 0x4E 'N'
  {   985,  13,  16,  15,    1,  -15 },   // [ 47] 0x4F 'O'
  {  1011,  12,  16,  15,    2,  -15 },   // [ 48] 0x50 'P'
  {  1035,  14,  20,  15,    1,  -15 },   // [ 49] 0x51 'Q'
  {  1070,  12,  16,  15,    2,  -15 },   // [ 50] 0x52 'R'
  {  1094,  12,  16,  15,    1,  -15 },   // [ 51] 0x53 'S'
  {  1118,  13,  16,  15,    1,  -15 },   // [ 52] 0x54 'T'
  {  1144,  11,  16,  15,    2,  -15 },   // [ 53] 0x55 'U'
  {  1166,  13,  16,  15,    1,  -15 },   // [ 54] 0x56 'V'
  {  1192,  13,  16,  15,    1,  -15 },   // [ 55] 0x57 'W'
  {  1218,  13,  16,  15,    1,  -15 },   // [ 56] 0x58 'X'
  {  1244,  14,  16,  15,    0,  -15 },   // [ 57] 0x59 'Y'
  {  1272,  12,  16,  15,    1,  -15 },   // [ 58] 0x5A 'Z'
  {  1296,   9,  23,  15,    4,  -18 },   // [ 59] 0x5B '['
  {  1322,  13,  20,  15,    1,  -17 },   // [ 60] 0x5C backslash
  {  1355,   9,  23,  15,    2,  -18 },   // [ 61] 0x5D ']'
  {  1381,  13,   9,  15,    1,  -15 },   // [ 62] 0x5E '^'
  {  1396,  11,   3,  15,    2,    2 },   // [ 63] 0x5F '_'
  {  1401,   8,   8,  15,    3,  -16 },   // [ 64] 0x60 '`'
  {  1409,  13,  11,  15,    1,  -10 },   // [ 65] 0x61 'a'
  {  1427,  12,  17,  15,    2,  -16 },   // [ 66] 0x62 'b'
  {  1453,  12,  11,  15,    1,  -10 },   // [ 67] 0x63 'c'
  {  1470,  12,  17,  15,    1,  -16 },   // [ 68] 0x64 'd'
  {  1496,  12,  11,  15,    1,  -10 },   // [ 69] 0x65 'e'
  {  1513,  12,  17,  15,    2,  -16 },   // [ 70] 0x66 'f'
  {  1539,  12,  17,  15,    1,  -10 },   // [ 71] 0x67 'g'
  {  1565,  11,  17,  15,    2,  -16 },   // [ 72] 0x68 'h'
  {  1589,  11,  17,  15,    2,  -16 },   // [ 73] 0x69 'i'
  {  1613,  10,  23,  15,    2,  -16 },   // [ 74] 0x6A 'j'
  {  1642,  12,  17,  15,    2,  -16 },   // [ 75] 0x6B 'k'
  {  1668,  12,  17,  15,    1,  -16 },   // [ 76] 0x6C 'l'
  {  1694,  13,  11,  15,    1,  -10 },   // [ 77] 0x6D 'm'
  {  1712,  11,  11,  15,    2,  -10 },   // [ 78] 0x6E 'n'
  {  1728,  13,  11,  15,    1,  -10 },   // [ 79] 0x6F 'o'
  {  1746,  12,  17,  15,    2,  -10 },   // [ 80] 0x70 'p'
  {  1772,  12,  17,  15,    1,  -10 },   // [ 81] 0x71 'q'
  {  1798,  13,  11,  15,    1,  -10 },   // [ 82] 0x72 'r'
  {  1816,  11,  11,  15,    2,  -10 },   // [ 83] 0x73 's'
  {  1832,  12,  15,  15,    1,  -14 },   // [ 84] 0x74 't'
  {  1855,  11,  11,  15,    2,  -10 },   // [ 85] 0x75 'u'
  {  1871,  13,  11,  15,    1,  -10 },   // [ 86] 0x76 'v'
  {  1889,  13,  11,  15,    1,  -10 },   // [ 87] 0x77 'w'
  {  1907,  13,  11,  15,    1,  -10 },   // [ 88] 0x78 'x'
  {  1925,  13,  17,  15,    1,  -10 },   // [ 89] 0x79 'y'
  {  1953,  11,  11,  15,    2,  -10 },   // [ 90] 0x7A 'z'
  {  1969,  13,  23,  15,    1,  -18 },   // [ 91] 0x7B '{'
  {  2007,   3,  20,  15,    6,  -17 },   // [ 92] 0x7C '|'
  {  2015,  12,  23,  15,    1,  -18 },   // [ 93] 0x7D '}'
  {  2050,  13,   6,  15,    1,   -9 },   // [ 94] 0x7E '~'
  {  3736,  12,  19,  15,    1,  -16 },   // [ 95] 0xA9 (c)
  {  4370,  13,  21,  15,    1,  -20 },   // [ 96] 0xC4 Ae
  {  4942,  13,  21,  15,    1,  -20 },   // [ 97] 0xD6 Oe
  {  5123,  11,  21,  15,    2,  -20 },   // [ 98] 0xDC Ue
  {  5217,  12,  17,  15,    2,  -16 },   // [ 99] 0xDF ss
  {  5363,  13,  16,  15,    1,  -15 },   // [100] 0xE4 ae
  {  5842,  13,  16,  15,    1,  -15 },   // [101] 0xF6 oe
  {  5989,  11,  16,  15,    2,  -15 },   // [102] 0xFC ue
};

const lgfx::GFXfont IntelOneMono_Regular12pt8b_Scroll PROGMEM = {
  (uint8_t *) IntelOneMono_Regular12pt8bBitmaps,
  (lgfx::GFXglyph *) IntelOneMono_Regular12pt8b_ScrollGlyphs,
  0x20, 0xFC, 32,
  INTELONEMONO12PT_SCROLL_RANGES, (lgfx::EncodeRange *) IntelOneMono12ptScrollRanges
};

const lgfx::GFXfont IntelOneMono_Bold12pt8b_Scroll PROGMEM = {
  (uint8_t *) IntelOneMono_Bold12pt8bBitmaps,
  (lgfx::GFXglyph *) IntelOneMono_Bold12pt8b_ScrollGlyphs,
  0x20, 0xFC, 32,
  INTELONEMONO12PT_SCROLL_RANGES, (lgfx::EncodeRange *) IntelOneMono12ptScrollRanges
};

#endif
