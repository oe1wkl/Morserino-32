// arduino_string_shim.h — just enough of the Arduino `String` API to compile
// and exercise MorseQsoBotMatch.h on the host. NOT a full String: only the
// members the matcher primitives (and these tests) actually call. Semantics
// follow Arduino's String where it matters (toInt() of junk == 0, out-of-range
// index == 0, substring clamping, case-insensitive compare).

#ifndef ARDUINO_STRING_SHIM_H_
#define ARDUINO_STRING_SHIM_H_

#include <string>
#include <cctype>
#include <cstdlib>

class String {
  public:
    String() {}
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    String(const String&) = default;
    String& operator=(const String&) = default;

    unsigned length() const { return (unsigned) s_.size(); }

    // Arduino returns 0 for an out-of-range index rather than UB.
    char operator[](unsigned i) const { return i < s_.size() ? s_[i] : 0; }
    char operator[](int i) const {
        return (i >= 0 && (size_t) i < s_.size()) ? s_[i] : 0;
    }

    const char* c_str() const { return s_.c_str(); }

    void toUpperCase() { for (auto& c : s_) c = (char) std::toupper((unsigned char) c); }
    void toLowerCase() { for (auto& c : s_) c = (char) std::tolower((unsigned char) c); }

    void reserve(unsigned n) { s_.reserve(n); }

    long toInt() const {
        char* end = nullptr;
        long v = std::strtol(s_.c_str(), &end, 10);
        return (end == s_.c_str()) ? 0 : v;   // Arduino: unparsable -> 0
    }

    String& operator+=(char c)         { s_ += c; return *this; }
    String& operator+=(const char* c)  { s_ += (c ? c : ""); return *this; }
    String& operator+=(const String& o){ s_ += o.s_; return *this; }

    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator==(const char* o) const   { return s_ == (o ? o : ""); }
    bool operator!=(const String& o) const { return s_ != o.s_; }
    bool operator!=(const char* o) const   { return s_ != (o ? o : ""); }

    int lastIndexOf(char ch) const {
        size_t p = s_.rfind(ch);
        return (p == std::string::npos) ? -1 : (int) p;
    }

    String substring(int from) const {
        if (from < 0) from = 0;
        if ((size_t) from >= s_.size()) return String();
        return String(s_.substr(from));
    }
    String substring(int from, int to) const {
        if (from < 0) from = 0;
        if (to > (int) s_.size()) to = (int) s_.size();
        if (from >= to) return String();
        return String(s_.substr(from, to - from));
    }

    bool equalsIgnoreCase(const String& o) const {
        if (s_.size() != o.s_.size()) return false;
        for (size_t i = 0; i < s_.size(); i++)
            if (std::tolower((unsigned char) s_[i]) !=
                std::tolower((unsigned char) o.s_[i])) return false;
        return true;
    }

  private:
    std::string s_;
};

#endif  // ARDUINO_STRING_SHIM_H_
