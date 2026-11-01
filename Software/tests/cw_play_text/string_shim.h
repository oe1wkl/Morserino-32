// string_shim.h -- just enough of Arduino's String (and Arduino.h) to compile the firmware's cw/play
// text path (extracted.inc) on the host. Only what that code calls; Arduino semantics where they
// matter (charAt out of range == 0, replace() of every occurrence).
#ifndef CW_PLAY_STRING_SHIM_H_
#define CW_PLAY_STRING_SHIM_H_

#include <string>
#include <cstring>
#include <cctype>
#include <cstdint>

typedef uint8_t byte;
typedef bool boolean;
inline bool isSpace(int c) { return std::isspace(c) != 0; }

class String {
  public:
    String() {}
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    unsigned length() const { return (unsigned) s_.size(); }
    char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
    const char* c_str() const { return s_.c_str(); }
    void reserve(unsigned n) { s_.reserve(n); }
    void toLowerCase() { for (auto& c : s_) c = (char) std::tolower((unsigned char) c); }
    int indexOf(char c) const { size_t p = s_.find(c); return p == std::string::npos ? -1 : (int) p; }
    void replace(char a, char b) { for (auto& c : s_) if (c == a) c = b; }
    void replace(const String& a, const String& b) {
        if (a.s_.empty()) return;
        std::string out;
        for (size_t i = 0; i < s_.size(); ) {
            if (s_.compare(i, a.s_.size(), a.s_) == 0) { out += b.s_; i += a.s_.size(); }
            else out += s_[i++];
        }
        s_ = out;
    }
    String& operator+=(char c) { s_ += c; return *this; }
    bool operator==(const char* o) const { return s_ == (o ? o : ""); }
    const std::string& str() const { return s_; }
  private:
    std::string s_;
};

#endif
