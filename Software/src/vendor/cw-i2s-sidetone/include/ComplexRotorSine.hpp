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
/// untouched.  The OK/ERR signalling tones use one of two voiced timbres, summed from
/// the first eight harmonics of the same rotor bank.
///
/// Why they are voiced at all: as pure sines on the M32 Pocket's micro-speaker these
/// signals are inaudibly quiet -- the driver does almost nothing below ~800 Hz, and the
/// ear is least sensitive exactly there.  The first attempt (V9 beta) used the first four
/// terms of a SQUARE wave -- odd harmonics only, at 1/3, 1/5, 1/7.  That is audible but
/// hollow and buzzy, and the field called it, correctly, "harsh and unpleasant".  Odd-only
/// at 1/n is the smoke-alarm recipe; the missing even harmonics are what make it read as
/// electronic rather than musical.
///
/// So each signal now gets a real instrument's harmonic recipe instead:
///
///   Trumpet (OK)   full series, formant around the 2nd-3rd harmonic. Bright and open.
///   Bassoon (ERR)  weak fundamental, strong 2nd-4th, hard roll-off above. Dark, woody.
///
/// The bassoon's weak fundamental is a feature on this driver, not a compromise: the ear
/// still hears the written pitch (missing fundamental) while the energy sits at 1-2 kHz,
/// where the speaker actually radiates.  That is also what let the signals move back DOWN
/// to the classic M32's pitches (440/587 and 366/330) -- the harmonics carry the tone, so
/// the fifth-up transposition the V9 beta needed for a thin sine is no longer required.
///
/// The harmonics MUST stay phase-locked to the fundamental, otherwise the peak of the sum
/// wanders and the codec clips. update_rotor() resets every phasor together, and pwmTone()
/// calls setFrequency() before each tone, so lock is re-established at every key-down.
/// Sines (the imaginary parts) are used for the sum, not cosines: cosines would all peak
/// together at t=0.
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

    enum class Timbre : uint8_t { Sine = 0, Trumpet = 1, Bassoon = 2 };

    /// Single-word write, so the audio task -- which reads timbre_ on every sample, not
    /// just while a tone sounds -- can never observe a half-changed voice. That is also
    /// why the weights are looked up per sample from a static table rather than copied
    /// into the object: there is no multi-word state to tear. Set it before the tone
    /// starts (pwmTone -> setFrequency -> on()), never mid-tone.
    void setTimbre(Timbre t) { timbre_ = t; }
    Timbre getTimbre() const { return timbre_; }

    int16_t readSample() override {
        // Advance the fundamental and its seven harmonic rotors together. Eight complex
        // multiplies per sample is ~2 Mflop/s at 44.1 kHz, well under 1 % of the S3's FPU,
        // and they are run unconditionally so they can never drift out of lock.
        for (int i = 0; i < kHarmonics; ++i) rotor_[i].advance();

        // Re-normalise every 1024 samples to prevent drift
        if (++norm_counter_ >= 1024) {
            norm_counter_ = 0;
            for (auto &r : rotor_) r.normalise();
        }

        // Convert to int16 only at the very end
        float sample;
        if (timbre_ == Timbre::Sine) {
            sample = rotor_[0].re * amplitude_;
        } else {
            const Voice v = voice(timbre_);
            float acc = 0.0f;
            for (int i = 0; i < kHarmonics; ++i)
                acc += rotor_[i].im * v.weight[i];
            sample = acc * v.scale * amplitude_;
        }
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        return (int16_t)sample;
    }

private:
    static constexpr int kHarmonics = 8;

    struct Voice { const float *weight; float scale; };

    /// Harmonic recipes, and the single constant that turns each one into a level.
    ///
    /// `scale` is (target peak) / (true peak of the summed waveform). Both numbers matter:
    ///
    ///  - the TRUE PEAK is not the sum of the weights -- the partials partly cancel at the
    ///    crest -- and it has to be measured, not guessed, or the codec clips. It is 3.2745
    ///    for the trumpet and 2.3285 for the bassoon (devdocs/signal-tones/tone_sim.py).
    ///  - the TARGET PEAK puts each signal about 6 dB BELOW the CW sidetone on the Pocket's
    ///    speaker, and -- unlike the V9 beta, where OK came out 4.5 dB louder than ERR --
    ///    puts the two of them within 0.1 dB of each other. The V9 beta had OK sitting
    ///    3 dB ABOVE the sidetone, i.e. the confirmation beep was louder than the CW being
    ///    practised. That is what the field reported. The two targets differ (0.43 vs 0.61)
    ///    because the voices differ in crest factor and in where they put their energy.
    static Voice voice(Timbre t) {
        static const float kTrumpet[kHarmonics] = {0.50f, 1.00f, 0.90f, 0.72f,
                                                   0.52f, 0.34f, 0.20f, 0.10f};
        static const float kBassoon[kHarmonics] = {0.25f, 1.00f, 0.85f, 0.45f,
                                                   0.18f, 0.07f, 0.03f, 0.01f};
        if (t == Timbre::Bassoon) return { kBassoon, 0.261975f };   // 0.61 / 2.328471
        return { kTrumpet, 0.131316f };                             // 0.43 / 3.274535
    }

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

    float frequency_  = 0.0f;
    float amplitude_  = 32767.0f;
    float delta_time_ = 0.0f;
    Timbre timbre_    = Timbre::Sine;

    Rotor rotor_[kHarmonics];            // fundamental + harmonics 2..8

    int norm_counter_ = 0;

    void update_rotor() {
        if (frequency_ > 0.0f && delta_time_ > 0.0f) {
            float rad = 2.0f * (float)M_PI * frequency_ * delta_time_;
            for (int i = 0; i < kHarmonics; ++i)
                rotor_[i].set(rad * (float)(i + 1));
        }
        norm_counter_ = 0;
    }
};
