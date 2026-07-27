#pragma once

#include "AudioTools/CoreAudio/AudioEffects/AudioEffect.h"
#include <cmath>
#include <algorithm>
#include <array>

/// Audio effect that applies a Blackman-Harris windowed attack/release envelope.
/// Produces the same smooth S-curve shape used by the ALSA simulator, which
/// eliminates the subtle harshness of linear ADSR ramps.
///
/// For CW sidetone, only attack and release are meaningful — sustain is always
/// 1.0 while the key is held.  The Blackman-Harris window has extremely low
/// sidelobe energy (~-92 dB), producing a clean, radio-grade sidetone.
class BlackmanHarrisEnvelope : public audio_tools::AudioEffect {
public:
    // Fixed capacity, good for up to ~20ms at 48kHz (current caller never asks for
    // more than 9ms at 44.1kHz). setAttackRate()/setReleaseRate() overwrite these
    // tables in place and publish the new length last, so process() — which runs on
    // a separate, higher-priority core — only ever indexes a fully valid float,
    // never a reallocated/torn buffer. See setAttackRate()/setReleaseRate().
    static constexpr int kMaxSamples = 961;

    BlackmanHarrisEnvelope(float attack_s = 0.005f, float release_s = 0.005f,
                           int sample_rate = 48000)
        : sample_rate_(sample_rate)
    {
        build_rise(attack_s);
        build_fall(release_s);
    }

    ~BlackmanHarrisEnvelope() override = default;

    BlackmanHarrisEnvelope(const BlackmanHarrisEnvelope& o)
    {
        sample_rate_    = o.sample_rate_;
        attack_table_   = o.attack_table_;
        release_table_  = o.release_table_;
        attack_len_     = o.attack_len_;
        release_len_    = o.release_len_;
        state_          = Idle;
        cursor_         = 0;
        copyParent((AudioEffect*)&o);
    }

    BlackmanHarrisEnvelope* clone() override {
        return new BlackmanHarrisEnvelope(*this);
    }

    // ── parameter setters (compatible with ADSRGain interface) ─────────────

    void setAttackRate(float seconds)  { build_rise(seconds); }
    void setReleaseRate(float seconds) { build_fall(seconds); }
    void setSustainLevel(float) {}     // always 1.0 for CW
    void setDecayRate(float) {}        // unused for CW

    void setSampleRate(int sr) {
        sample_rate_ = sr;
    }

    // ── key control ────────────────────────────────────────────────────────

    void keyOn(float = 0) {
        state_  = Rise;
        cursor_ = 0;
    }

    void keyOff() {
        if (state_ != Idle) {
            state_  = Fall;
            cursor_ = 0;
        }
    }

    bool isActive() { return state_ != Idle; }

    // ── per-sample processing ──────────────────────────────────────────────

    effect_t process(effect_t input) override {
        if (!active()) return input;

        float env = 0.0f;
        switch (state_) {
        case Idle:
            return 0;

        case Rise:
            if (cursor_ < attack_len_) {
                env = attack_table_[cursor_++];
            } else {
                state_ = On;
                env = 1.0f;
            }
            break;

        case On:
            env = 1.0f;
            break;

        case Fall:
            if (cursor_ < release_len_) {
                env = release_table_[cursor_++];
            } else {
                state_ = Idle;
                env = 0.0f;
            }
            break;
        }
        return (effect_t)(env * input);
    }

private:
    enum State { Idle, Rise, On, Fall };

    int   sample_rate_;
    State state_  = Idle;
    int   cursor_ = 0;

    std::array<float, kMaxSamples> attack_table_{};   // 0 → 1
    std::array<float, kMaxSamples> release_table_{};  // 1 → 0
    int   attack_len_  = 0;
    int   release_len_ = 0;

    /// One point of the Blackman-Harris window.
    static float bh(int size, int k) {
        constexpr double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
        double arg = k * 2.0 * M_PI / (size - 1);
        return (float)(a0 - a1 * cos(arg) + a2 * cos(2.0 * arg) - a3 * cos(3.0 * arg));
    }

    /// Build the attack (rise) table: values go 0 → 1 using the first half
    /// of a Blackman-Harris window of length (2n - 1). Overwrites the fixed
    /// attack_table_ in place and only publishes attack_len_ once every sample
    /// up to n has been written, so a concurrent reader on another core never
    /// observes a partially-built table under its own length.
    void build_rise(float seconds) {
        int n = std::max(1, (int)(seconds * sample_rate_));
        if ((n & 1) == 0) ++n;
        n = std::min(n, kMaxSamples);
        for (int i = 0; i < n; ++i)
            attack_table_[i] = bh(2 * n - 1, i);
        attack_len_ = n;
    }

    /// Build the release (fall) table: values go 1 → 0 using (1 - rise). Same
    /// write-then-publish discipline as build_rise().
    void build_fall(float seconds) {
        int n = std::max(1, (int)(seconds * sample_rate_));
        if ((n & 1) == 0) ++n;
        n = std::min(n, kMaxSamples);
        for (int i = 0; i < n; ++i)
            release_table_[i] = 1.0f - bh(2 * n - 1, i);
        release_len_ = n;
    }
};
