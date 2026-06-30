// qso_content.h — small static content pools used by the QSO Bot to
// fill placeholders ([BOTNAME], [BOTREF], etc.) at QSO start.
//
// Continent-consistency: the bot's callsign is generated with a continent
// (lastGeneratedCallContinent), and the operator name, QTH and SOTA/POTA ref
// are then picked to match it — so a JA station is HIRO from TOKYO, not WERNER
// from VIENNA. Names/QTHs/refs are {value, continent-mask} pairs picked by
// pickByContinent (falling back to any value if nothing matches the wanted
// continent). Names carry a *mask* (a common English name fits several
// continents); QTHs and refs carry a single continent. Rig/ant/wx/age are not
// location-specific, so they stay plain arrays.

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

// A content value tagged with the continent(s) it suits. `continent` is a
// bitmask: a value matches when (value.continent & wantContinent) is nonzero.
struct ContValue {
    const char* value;
    uint8_t     continent;
};

// Pick a value whose continent mask intersects `wantContinent`; if none match
// (or wantContinent is 0/unknown), fall back to any value. Returns the string.
inline const char* pickByContinent(const ContValue* arr, int count, uint8_t wantContinent) {
    int matches = 0;
    for (int i = 0; i < count; i++)
        if (arr[i].continent & wantContinent) matches++;
    if (matches == 0)
        return arr[random(count)].value;                  // no continent match
    int pick = random(matches);
    for (int i = 0; i < count; i++) {
        if (arr[i].continent & wantContinent) {
            if (pick == 0) return arr[i].value;
            pick--;
        }
    }
    return arr[0].value;                                   // unreachable
}

// Operator names — uppercase, short, CW-friendly. A common English name fits
// several continents (NA/EU/OC, sometimes AF); region-specific names are
// tagged narrowly so e.g. a JA call gets HIRO/YUKI, not HANS.
static const ContValue kNames[] = {
    { "JOHN",   CONT_NA|CONT_EU|CONT_OC|CONT_AF }, { "MIKE",  CONT_NA|CONT_EU|CONT_OC|CONT_AF },
    { "DAVE",   CONT_NA|CONT_EU|CONT_OC|CONT_AF }, { "PETER", CONT_NA|CONT_EU|CONT_OC|CONT_AF },
    { "BOB",    CONT_NA|CONT_OC },                 { "TOM",   CONT_NA|CONT_EU|CONT_OC },
    { "JAMES",  CONT_NA|CONT_EU|CONT_OC },         { "MARK",  CONT_NA|CONT_EU|CONT_OC },
    { "TIM",    CONT_NA|CONT_EU|CONT_OC },         { "STEVE", CONT_NA|CONT_EU|CONT_OC },
    { "ALEX",   CONT_NA|CONT_EU|CONT_OC },         { "PAT",   CONT_NA|CONT_EU },
    { "KEN",    CONT_NA|CONT_OC },                 { "BILL",  CONT_NA|CONT_OC },
    { "JIM",    CONT_NA|CONT_OC },                 { "RON",   CONT_NA|CONT_OC },
    { "ED",     CONT_NA },                         { "RAY",   CONT_NA },
    { "AL",     CONT_NA },                         { "ANNA",  CONT_EU|CONT_NA|CONT_AF },
    { "HANS",   CONT_EU },                         { "WERNER",CONT_EU },
    { "SVEN",   CONT_EU },                         { "LARS",  CONT_EU },
    { "NILS",   CONT_EU },                         { "ERIK",  CONT_EU },
    { "JAN",    CONT_EU },                         { "OLEG",  CONT_EU },
    { "IGOR",   CONT_EU },                         { "PAVEL", CONT_EU },
    { "HIRO",   CONT_AS },                         { "YUKI",  CONT_AS },
    { "KENJI",  CONT_AS },                         { "TAK",   CONT_AS },
    { "LEO",    CONT_EU|CONT_SA },                 { "LUIS",  CONT_SA|CONT_EU },
    { "PEDRO",  CONT_SA|CONT_EU },                 { "CARLOS",CONT_SA|CONT_EU },
    { "JUAN",   CONT_SA|CONT_EU },                 { "ANDRE", CONT_SA|CONT_EU|CONT_AF },
};
constexpr int kNamesCount = sizeof(kNames) / sizeof(kNames[0]);

inline const char* pickName(uint8_t wantContinent) {
    return pickByContinent(kNames, kNamesCount, wantContinent);
}

// QTHs — single-token cities, one continent each. The matcher accepts
// multi-word QTHs from the user; the bot keys a single token to stay
// unambiguous.
static const ContValue kQths[] = {
    { "VIENNA", CONT_EU }, { "LONDON", CONT_EU }, { "BERLIN", CONT_EU }, { "ROME",   CONT_EU },
    { "MADRID", CONT_EU }, { "OSLO",   CONT_EU }, { "PRAGUE", CONT_EU }, { "PARIS",  CONT_EU },
    { "BERN",   CONT_EU }, { "WARSAW", CONT_EU }, { "BUDAPEST",CONT_EU}, { "DUBLIN", CONT_EU },
    { "LISBON", CONT_EU }, { "ATHENS", CONT_EU }, { "ZURICH", CONT_EU }, { "HELSINKI",CONT_EU},
    { "DENVER", CONT_NA }, { "BOSTON", CONT_NA }, { "DALLAS", CONT_NA }, { "MIAMI",  CONT_NA },
    { "OTTAWA", CONT_NA }, { "TORONTO",CONT_NA }, { "CHICAGO",CONT_NA }, { "SEATTLE",CONT_NA },
    { "TOKYO",  CONT_AS }, { "OSAKA",  CONT_AS }, { "NAGOYA", CONT_AS }, { "SEOUL",  CONT_AS },
    { "MANILA", CONT_AS },
    { "SYDNEY", CONT_OC }, { "MELBOURNE",CONT_OC},{ "PERTH",  CONT_OC }, { "AUCKLAND",CONT_OC},
    { "RIO",    CONT_SA }, { "LIMA",   CONT_SA }, { "BOGOTA", CONT_SA }, { "SANTIAGO",CONT_SA},
    { "CORDOBA",CONT_SA },
    { "CAIRO",  CONT_AF }, { "NAIROBI",CONT_AF }, { "CAPETOWN",CONT_AF},{ "DURBAN", CONT_AF },
    { "TUNIS",  CONT_AF },
};
constexpr int kQthsCount = sizeof(kQths) / sizeof(kQths[0]);

inline const char* pickQth(uint8_t wantContinent) {
    return pickByContinent(kQths, kQthsCount, wantContinent);
}

// Rig / antenna / weather / age — not location-specific, so plain arrays.
static const char* const kRigs[] = {
    "IC7300", "FT991", "K3", "KX3", "KX2", "TS590", "FT817", "IC705",
    "FTDX10", "K4", "ELECRAFT", "FT710",
    // QRP / SOTA tier
    "QCX", "QMX", "MTR", "KX1", "ATS", "HB1B", "HOMEBREW",
    // classics + bare makers
    "FT101", "TS520", "IC756", "TS850", "YAESU", "ICOM", "KENWOOD",
};
constexpr int kRigsCount = sizeof(kRigs) / sizeof(kRigs[0]);

static const char* const kAnts[] = {
    "DIPOLE", "EFHW", "VERT", "BEAM", "HEXBEAM", "GP", "G5RV", "LOOP",
    "ENDFED", "INVV", "YAGI", "DELTA", "SLOPER", "MAGLOOP", "JPOLE",
    "FAN", "LONGWIRE", "W3DZZ",
};
constexpr int kAntsCount = sizeof(kAnts) / sizeof(kAnts[0]);

static const char* const kWxs[] = {
    "SUNNY", "CLOUDY", "RAIN", "SNOW", "CLEAR", "FOG", "WINDY", "COLD",
    "WARM", "HOT", "MILD", "STORM", "DRIZZLE", "HUMID", "FROST", "ICE",
    "HAIL", "OVERCAST", "BREEZY",
};
constexpr int kWxsCount = sizeof(kWxs) / sizeof(kWxs[0]);

static const char* const kAges[] = {
    "25", "32", "41", "55", "63", "70", "45", "38", "52", "29", "47", "60",
};
constexpr int kAgesCount = sizeof(kAges) / sizeof(kAges[0]);

// SOTA summit references, continent-tagged so the bot picks a summit
// consistent with the continent of its (randomly generated) callsign.
// Pattern: "PREFIX/REGION-NNN". Not country-exact, but continent-consistent.
static const ContValue kSotaRefs[] = {
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
    { "w7/wa-005", CONT_NA }, { "ve7/vc-001", CONT_NA },
    { "ja/nn-001", CONT_AS }, { "ja/so-004", CONT_AS },
    { "hl/gd-001", CONT_AS }, { "bv/tp-001", CONT_AS },
    { "vk1/ac-014", CONT_OC }, { "vk3/vs-001", CONT_OC },
    { "vk5/se-001", CONT_OC }, { "zl1/wk-001", CONT_OC },
    { "lu/ic-001", CONT_SA }, { "py/sp-001", CONT_SA },
    { "ce/ms-001", CONT_SA },
    { "zs/wc-001", CONT_AF }, { "cn/mo-001", CONT_AF },
    { "ea8/lp-001", CONT_AF },
};
constexpr int kSotaRefsCount = sizeof(kSotaRefs) / sizeof(kSotaRefs[0]);

// POTA park references. Modern POTA format is "XX-NNNN" (entity prefix +
// 4-5 digit park number). Continent-tagged like the SOTA refs.
static const ContValue kPotaRefs[] = {
    { "oe-0012", CONT_EU }, { "oe-0044", CONT_EU },
    { "dl-0021", CONT_EU }, { "dl-0103", CONT_EU },
    { "g-0001",  CONT_EU }, { "g-0034",  CONT_EU },
    { "f-1002",  CONT_EU }, { "i-0150",  CONT_EU },
    { "us-0001", CONT_NA }, { "us-1234", CONT_NA },
    { "us-4567", CONT_NA }, { "us-7777", CONT_NA },
    { "ve-0099", CONT_NA }, { "ve-0050", CONT_NA },
    { "ja-0007", CONT_AS }, { "ja-0210", CONT_AS },
    { "hl-0003", CONT_AS }, { "bv-0001", CONT_AS },
    { "vk-0042", CONT_OC }, { "vk-0301", CONT_OC },
    { "vk-0500", CONT_OC }, { "zl-0004", CONT_OC },
    { "lu-0001", CONT_SA }, { "py-0010", CONT_SA },
    { "ce-0005", CONT_SA },
    { "zs-0007", CONT_AF }, { "cn-0003", CONT_AF },
};
constexpr int kPotaRefsCount = sizeof(kPotaRefs) / sizeof(kPotaRefs[0]);

// Pick a SOTA / POTA ref on the wanted continent (CONT_* mask), falling
// back to any ref if none match. Returns a lowercase reference string.
inline const char* pickSotaRef(uint8_t wantContinent) {
    return pickByContinent(kSotaRefs, kSotaRefsCount, wantContinent);
}
inline const char* pickPotaRef(uint8_t wantContinent) {
    return pickByContinent(kPotaRefs, kPotaRefsCount, wantContinent);
}

}  // namespace QsoContent

#endif  // CONFIG_QSO_BOT
#endif  // QSO_CONTENT_H_
