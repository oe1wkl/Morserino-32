// qso_content.h — small static content pools used by the QSO Bot to
// fill placeholders ([BOTNAME], [BOTREF], etc.) at QSO start.
//
// PR-3 ships only the slots needed for SOTA (names + summit refs). PR-4
// adds POTA refs, QTHs, rigs, antennas, weather, ages.

#ifndef QSO_CONTENT_H_
#define QSO_CONTENT_H_

#ifdef CONFIG_QSO_BOT

#include <Arduino.h>

// Continent bitmask values — mirror the definitions in callsign_prefixes.h
// (which we deliberately do NOT include here, to avoid pulling its ~8 KB
// PROGMEM prefix table into this translation unit). Guarded so there's no
// clash if both headers are ever included together.
#ifndef CONT_EU
#define CONT_EU  0x01
#define CONT_NA  0x02
#define CONT_SA  0x04
#define CONT_AF  0x08
#define CONT_AS  0x10
#define CONT_OC  0x20
#define CONT_AN  0x40
#define CONT_ALL 0x7F
#endif

namespace QsoContent {

// Operator names — uppercase, short (3–6 chars). Realistic CW-style names.
static const char* const kNames[] = {
    "JOHN", "BOB", "MIKE", "ANNA", "HANS", "WERNER", "TOM",
    "JAMES", "PAT", "SVEN", "KEN", "LUIS", "HIRO", "PETER",
    "DAVE", "MARK", "LEO", "JAN", "TIM", "STEVE",
};
constexpr int kNamesCount = sizeof(kNames) / sizeof(kNames[0]);

// Standard-QSO content pools. Single-token values keep the bot's keyed
// over unambiguous; the matcher accepts multi-word values from the user.
static const char* const kQths[] = {
    "VIENNA", "LONDON", "BERLIN", "ROME", "MADRID", "OSLO", "PRAGUE",
    "PARIS", "BERN", "WARSAW", "DENVER", "BOSTON", "TOKYO", "SYDNEY",
};
constexpr int kQthsCount = sizeof(kQths) / sizeof(kQths[0]);

static const char* const kRigs[] = {
    "IC7300", "FT991", "K3", "KX3", "KX2", "TS590", "FT817", "IC705",
    "FTDX10", "K4", "ELECRAFT", "FT710",
};
constexpr int kRigsCount = sizeof(kRigs) / sizeof(kRigs[0]);

static const char* const kAnts[] = {
    "DIPOLE", "EFHW", "VERT", "BEAM", "HEXBEAM", "GP", "G5RV", "LOOP",
    "ENDFED", "INVV",
};
constexpr int kAntsCount = sizeof(kAnts) / sizeof(kAnts[0]);

static const char* const kWxs[] = {
    "SUNNY", "CLOUDY", "RAIN", "SNOW", "CLEAR", "FOG", "WINDY", "COLD",
    "WARM", "HOT", "MILD",
};
constexpr int kWxsCount = sizeof(kWxs) / sizeof(kWxs[0]);

static const char* const kAges[] = {
    "25", "32", "41", "55", "63", "70", "45", "38", "52", "29", "47", "60",
};
constexpr int kAgesCount = sizeof(kAges) / sizeof(kAges[0]);

// SOTA summit references, each tagged with the continent of its
// association so the bot can pick a summit consistent with the
// continent of its (randomly generated) callsign. Pattern:
// "PREFIX/REGION-NNN". Not country-exact, but continent-consistent.
struct SotaRef {
    const char* ref;
    uint8_t     continent;
};

static const SotaRef kSotaRefs[] = {
    { "oe/at-001", CONT_EU }, { "oe/st-002", CONT_EU },
    { "oe/sb-005", CONT_EU }, { "oe/kt-010", CONT_EU },
    { "g/ld-001",  CONT_EU }, { "g/ld-003",  CONT_EU },
    { "g/sp-001",  CONT_EU }, { "g/wb-002",  CONT_EU },
    { "dl/al-001", CONT_EU }, { "dl/bw-021", CONT_EU },
    { "dl/am-006", CONT_EU }, { "dl/sx-001", CONT_EU },
    { "f/vo-001",  CONT_EU }, { "hb/vs-001", CONT_EU },
    { "sp/bz-001", CONT_EU }, { "i/ra-001",  CONT_EU },
    { "w6/ss-001", CONT_NA }, { "w6/ct-001", CONT_NA },
    { "w2/cr-003", CONT_NA }, { "w4/cm-005", CONT_NA },
    { "ja/nn-001", CONT_AS }, { "ja/so-004", CONT_AS },
    { "vk1/ac-014", CONT_OC }, { "vk3/vs-001", CONT_OC },
};
constexpr int kSotaRefsCount = sizeof(kSotaRefs) / sizeof(kSotaRefs[0]);

// POTA park references. Modern POTA format is "XX-NNNN" (entity prefix +
// 4-5 digit park number). Continent-tagged like the SOTA refs.
struct PotaRef {
    const char* ref;
    uint8_t     continent;
};

static const PotaRef kPotaRefs[] = {
    { "oe-0012", CONT_EU }, { "oe-0044", CONT_EU },
    { "dl-0021", CONT_EU }, { "dl-0103", CONT_EU },
    { "g-0001",  CONT_EU }, { "g-0034",  CONT_EU },
    { "f-1002",  CONT_EU }, { "i-0150",  CONT_EU },
    { "us-0001", CONT_NA }, { "us-1234", CONT_NA },
    { "us-4567", CONT_NA }, { "ve-0099", CONT_NA },
    { "ja-0007", CONT_AS }, { "ja-0210", CONT_AS },
    { "vk-0042", CONT_OC }, { "vk-0301", CONT_OC },
};
constexpr int kPotaRefsCount = sizeof(kPotaRefs) / sizeof(kPotaRefs[0]);

// Shared continent picker over a {ref, continent} array.
template <typename T>
inline const char* pickRefByContinent(const T* arr, int count, uint8_t wantContinent) {
    int matches = 0;
    for (int i = 0; i < count; i++)
        if (arr[i].continent & wantContinent) matches++;
    if (matches == 0)
        return arr[random(count)].ref;                    // no continent match
    int pick = random(matches);
    for (int i = 0; i < count; i++) {
        if (arr[i].continent & wantContinent) {
            if (pick == 0) return arr[i].ref;
            pick--;
        }
    }
    return arr[0].ref;                                     // unreachable
}

// Pick a SOTA / POTA ref on the wanted continent (CONT_* mask), falling
// back to any ref if none match. Returns a lowercase reference string.
inline const char* pickSotaRef(uint8_t wantContinent) {
    return pickRefByContinent(kSotaRefs, kSotaRefsCount, wantContinent);
}
inline const char* pickPotaRef(uint8_t wantContinent) {
    return pickRefByContinent(kPotaRefs, kPotaRefsCount, wantContinent);
}

}  // namespace QsoContent

#endif  // CONFIG_QSO_BOT
#endif  // QSO_CONTENT_H_
