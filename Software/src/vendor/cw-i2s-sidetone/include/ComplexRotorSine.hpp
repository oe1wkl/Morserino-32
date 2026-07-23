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

    int16_t readSample() override {
        // Complex rotation: (re, im) *= (dre, dim)
        float new_re = phase_re_ * dphase_re_ - phase_im_ * dphase_im_;
        float new_im = phase_re_ * dphase_im_ + phase_im_ * dphase_re_;
        phase_re_ = new_re;
        phase_im_ = new_im;

        // Re-normalise every 1024 samples to prevent drift
        if (++norm_counter_ >= 1024) {
            norm_counter_ = 0;
            float mag = phase_re_ * phase_re_ + phase_im_ * phase_im_;
            if (mag > 0.0f) {
                float inv = 1.0f / sqrtf(mag);
                phase_re_ *= inv;
                phase_im_ *= inv;
            }
        }

        // Convert to int16 only at the very end
        float sample = phase_re_ * amplitude_;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        return (int16_t)sample;
    }

private:
    float frequency_  = 0.0f;
    float amplitude_  = 32767.0f;
    float delta_time_ = 0.0f;

    // Phasor state (unit complex number)
    float phase_re_  = 1.0f;
    float phase_im_  = 0.0f;

    // Rotation vector (pre-computed from frequency)
    float dphase_re_ = 1.0f;
    float dphase_im_ = 0.0f;

    int norm_counter_ = 0;

    void update_rotor() {
        if (frequency_ > 0.0f && delta_time_ > 0.0f) {
            float rad = 2.0f * (float)M_PI * frequency_ * delta_time_;
            dphase_re_ = cosf(rad);
            dphase_im_ = sinf(rad);
        }
        // Reset phase for clean start
        phase_re_ = 1.0f;
        phase_im_ = 0.0f;
        norm_counter_ = 0;
    }
};
