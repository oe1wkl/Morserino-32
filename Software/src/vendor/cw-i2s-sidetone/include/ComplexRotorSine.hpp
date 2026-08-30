#pragma once

#include "AudioTools/CoreAudio/AudioEffects/SoundGenerator.h"
#include <cmath>

/// Sine wave generator using a complex-rotor oscillator.
///
/// Multiplies a unit complex phasor by a pre-computed rotation vector each
/// sample — one complex multiply (4 real multiplies + 2 adds) per sample,
/// producing a mathematically clean sine with no table quantisation or
/// interpolation artefacts.  The ESP32-S3 hardware FPU makes this cheap.
///
/// The real part of the phasor is the sine output.  A periodic
/// re-normalisation keeps amplitude drift below -120 dB even after hours
/// of continuous keying.
///
/// Timbre: the CW sidetone is always a pure sine (Timbre::Sine) and that path is
/// untouched.  Timbre::Rich adds the 3rd, 5th and 7th harmonics at 1/3, 1/5 and 1/7 --
/// the first four terms of a square wave -- for the OK/ERR signalling tones, which are
/// far too quiet as pure sines on the M32 Pocket's micro-speaker (its response below
/// ~500 Hz is poor, and the ear is least sensitive exactly there).  Truncating at the
/// 7th keeps it band-limited: no aliasing, and much softer than a real square.
///
/// The harmonics MUST stay phase-locked to the fundamental, otherwise the peak of the
/// sum wanders between 0.93 and 1.68 (the sum of the weights) and the codec clips.
/// update_rotor() resets every phasor together, and pwmTone() calls setFrequency() before
/// each tone, so lock is re-established at every key-down.  Sines (the imaginary parts)
/// are used for the sum, not cosines: cosines would all peak together at t=0 at 1.68.
class ComplexRotorSine : public audio_tools::SoundGenerator<int16_t> {
public:
    ComplexRotorSine(float amplitude = 32767.0f)
        : amplitude_(amplitude) {}

    bool begin() override {
        SoundGenerator<int16_t>::begin();
        delta_time_ = 1.0f / audioInfo().sample_rate;
        update_rotor();
        return true;
    }

    bool begin(audio_tools::AudioInfo info) override {
        SoundGenerator<int16_t>::begin(info);
        delta_time_ = 1.0f / audioInfo().sample_rate;
        update_rotor();
        return true;
    }

    bool begin(audio_tools::AudioInfo info, float frequency) {
        frequency_ = frequency;
        return begin(info);
    }

    bool begin(int channels, int sample_rate, float frequency) {
        audio_tools::AudioInfo ai;
        ai.channels = channels;
        ai.sample_rate = sample_rate;
        ai.bits_per_sample = 16;
        frequency_ = frequency;
        return begin(ai);
    }

    void setFrequency(float frequency) override {
        frequency_ = frequency;
        update_rotor();
    }

    void setAmplitude(float amp) { amplitude_ = amp; }

    enum class Timbre : uint8_t { Sine = 0, Rich = 1 };

    /// Single-word write, so the audio task can never observe a half-changed timbre.
    /// Set it before the tone starts (pwmTone -> setFrequency -> on()), never mid-tone.
    void setTimbre(Timbre t) { timbre_ = t; }
    Timbre getTimbre() const { return timbre_; }

    int16_t readSample() override {
        // Advance the fundamental and the three harmonic rotors together. The harmonics
        // cost ~3 complex multiplies per sample (<1 % CPU on the S3's FPU) and are run
        // unconditionally so they can never drift out of lock with the fundamental.
        rotor_[0].advance();
        rotor_[1].advance();
        rotor_[2].advance();
        rotor_[3].advance();

        // Re-normalise every 1024 samples to prevent drift
        if (++norm_counter_ >= 1024) {
            norm_counter_ = 0;
            for (auto &r : rotor_) r.normalise();
        }

        // Convert to int16 only at the very end
        float sample;
        if (timbre_ == Timbre::Rich) {
            // Weighted SINES (imaginary parts), scaled so the peak of the sum lands
            // exactly where a pure sine's does -- see kRichScale.
            sample = (rotor_[0].im + rotor_[1].im * (1.0f/3.0f)
                                   + rotor_[2].im * (1.0f/5.0f)
                                   + rotor_[3].im * (1.0f/7.0f)) * kRichScale * amplitude_;
        } else {
            sample = rotor_[0].re * amplitude_;
        }
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        return (int16_t)sample;
    }

private:
    // One unit phasor plus its per-sample rotation vector.
    struct Rotor {
        float re = 1.0f, im = 0.0f;      // phasor state
        float dre = 1.0f, dim = 0.0f;    // rotation per sample
        void advance() {
            float n_re = re * dre - im * dim;
            float n_im = re * dim + im * dre;
            re = n_re; im = n_im;
        }
        void normalise() {
            float mag = re * re + im * im;
            if (mag > 0.0f) { float inv = 1.0f / sqrtf(mag); re *= inv; im *= inv; }
        }
        void set(float rad) { dre = cosf(rad); dim = sinf(rad); re = 1.0f; im = 0.0f; }
    };

    // Peak of sin(x) + sin(3x)/3 + sin(5x)/5 + sin(7x)/7. The weights sum to 1.676, but the
    // harmonics partly cancel at the crest, so the true peak is 0.9301 -- dividing by it
    // makes the rich timbre occupy exactly the same peak budget as a pure sine (and hence
    // ~0.6 dB MORE energy, not less). Getting this wrong clips the codec.
    static constexpr float kRichPeak  = 0.9301f;
    // ...and then deliberately back off. On the bench the rich signals came out "almost too
    // loud" against the sidetone once the harmonics were doing their work, so they run 3 dB
    // below full scale rather than at it. They are still far louder to the ear than the pure
    // sines they replaced -- the audibility comes from the harmonic content, not the level.
    static constexpr float kRichLevel = 0.71f;               // -3.0 dB
    static constexpr float kRichScale = kRichLevel / kRichPeak;

    float frequency_  = 0.0f;
    float amplitude_  = 32767.0f;
    float delta_time_ = 0.0f;
    Timbre timbre_    = Timbre::Sine;

    Rotor rotor_[4];                     // fundamental, 3rd, 5th, 7th

    int norm_counter_ = 0;

    void update_rotor() {
        if (frequency_ > 0.0f && delta_time_ > 0.0f) {
            float rad = 2.0f * (float)M_PI * frequency_ * delta_time_;
            // Unrolled on purpose: a static constexpr array would be odr-used here and
            // needs an out-of-line definition before C++17 (this builds as gnu++11).
            rotor_[0].set(rad);
            rotor_[1].set(rad * 3.0f);
            rotor_[2].set(rad * 5.0f);
            rotor_[3].set(rad * 7.0f);
        }
        norm_counter_ = 0;
    }
};
