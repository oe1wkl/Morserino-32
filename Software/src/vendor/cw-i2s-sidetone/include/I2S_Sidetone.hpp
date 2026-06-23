#pragma once

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSPIFFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "BlackmanHarrisEnvelope.hpp"
#include "ComplexRotorSine.hpp"

class I2S_Sidetone {
	public:
    I2S_Sidetone();
    void begin(int samplerate = 44100, int bps=16, int channels = 2, int buffer_size=32);
    void setFrequency(float f);
    void setVolume(float v);
    float getFrequency();
    void setADSR(float attack, float decay, float sustainLevel, float release);
    void on();
    void off();
    bool playSPIFFSFile(const char *filename);
    // --- Async (non-blocking) clip playback for V9.0 audio accessibility -----------------
    // startClip() returns immediately; poll serviceClip() from the main loop to advance and
    // finish playback; stopClip() interrupts and resets the decoder (end()+begin() clears the
    // reader's result queue) so clips can't accumulate and stall the copier (the freeze bug).
    bool startClip(const char *filename);
    bool serviceClip();
    void stopClip();
    bool isClipPlaying();
    bool isOn();
    void tick();
    size_t readBytes (uint8_t *data, size_t len);
    int available ();
	private:
    I2SStream *i2s;
    ComplexRotorSine *sine;
    GeneratedSoundStream<int16_t> *in;
    AudioEffectStream *effects;
    VolumeStream *volume;
    LogarithmicVolumeControl *lvc;
    StreamCopy *copier;
    BlackmanHarrisEnvelope *adsr;

    File mp3file;
    EncodedAudioStream *decoder;
    InputMixer<int16_t> *mixer;

    float frequency;
    bool clipPlaying = false;     // V9.0 async clip playback state
    uint32_t fileDoneAt = 0;      // millis() when the clip file finished reading (0 = reading)
    AudioInfo clipCfg;            // saved decode config, to re-begin() the decoder per clip
};
