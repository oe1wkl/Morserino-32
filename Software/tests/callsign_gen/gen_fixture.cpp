// gen_fixture.cpp -- runs the firmware's getRandomCall() and getRandomPrefix() (extract.py copies it out of m32_v6.ino)
// with a deterministic random(): mulberry32, the M32 Trainer engine's generator, and
// random(a, b) = a + floor(r * (b - a)). Writes ../trainer/fixtures/random_calls.json; the Node
// test (../trainer/callsign.test.js) checks that the engine's randomCall() makes the same calls.
//   make run
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

typedef uint8_t byte;
#define PROGMEM
#define memcpy_P memcpy
#define pgm_read_byte(p) (*(const uint8_t*)(p))

class String {
  public:
    String(const char* s) : s_(s) {}
    const char* c_str() const { return s_.c_str(); }
  private:
    std::string s_;
};

static uint32_t rngState;
static double mulberry32() {                     // = mulberry32() in m32_trainer_engine.js
    rngState += 0x6D2B79F5u;
    uint32_t t = rngState;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (double) (t ^ (t >> 14)) / 4294967296.0;
}
static long random(long a, long b) { return a + (long) std::floor(mulberry32() * (double) (b - a)); }

enum { posCallLength, posCallContinent, posCallCommon };
namespace MorsePreferences { struct { uint8_t value; } pliste[3]; }

#include "../../src/Version 6 and newer/callsign_prefixes.h"
#include "extracted.inc"

int main() {
    const int lengths[] = {0, 1, 2, 3, 5};
    FILE* f = std::fopen("../trainer/fixtures/random_calls.json", "w");
    if (!f) { std::perror("random_calls.json"); return 1; }
    std::fprintf(f, "[\n");
    bool first = true;
    for (int len : lengths)
      for (int cont : {0, 2, 4, 6, 7})                // all, NA, AF, OC, VK/ZL
        for (int common = 0; common <= 1; common++)
          for (uint32_t seed = 1; seed <= 2; seed++) {
            MorsePreferences::pliste[posCallLength].value = len;
            MorsePreferences::pliste[posCallContinent].value = cont;
            MorsePreferences::pliste[posCallCommon].value = common;
            rngState = seed;
            std::fprintf(f, "%s{\"maxLength\":%d,\"continentPref\":%d,\"common\":%d,\"seed\":%u,\"calls\":[",
                         first ? "" : ",\n", len, cont, common, seed);
            first = false;
            for (int i = 0; i < 40; i++)
                std::fprintf(f, "%s[\"%s\",%d,%d]", i ? "," : "", getRandomCall(len).c_str(),
                             lastGeneratedCallContinent, lastGeneratedCallCqZone);
            std::fprintf(f, "]}");
          }
    // getRandomPrefix(): Length Calls does not apply; "prefix":1 marks these entries
    for (int cont = 0; cont <= 7; cont++)
      for (int common = 0; common <= 1; common++)
        for (uint32_t seed = 1; seed <= 2; seed++) {
            MorsePreferences::pliste[posCallContinent].value = cont;
            MorsePreferences::pliste[posCallCommon].value = common;
            rngState = seed;
            std::fprintf(f, ",\n{\"prefix\":1,\"continentPref\":%d,\"common\":%d,\"seed\":%u,\"calls\":[", cont, common, seed);
            for (int i = 0; i < 40; i++)
                std::fprintf(f, "%s[\"%s\",%d,%d]", i ? "," : "", getRandomPrefix().c_str(),
                             lastGeneratedCallContinent, lastGeneratedCallCqZone);
            std::fprintf(f, "]}");
        }
    std::fprintf(f, "\n]\n");
    std::fclose(f);
    std::printf("random_calls.json written\n");
    return 0;
}
