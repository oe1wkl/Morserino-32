#pragma once

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSPIFFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "BlackmanHarrisEnvelope.hpp"
#include "ComplexRotorSine.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

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
    // The AUDIO TASK owns the entire clip lifecycle (file open/close, mixer routing):
    // startClip()/stopClip() only post a command into a 1-deep mailbox (latest wins) and
    // return immediately. No other task ever mutates the decode pipeline while the copier
    // is running -- cross-task mutation was the root of a whole class of freezes and
    // crashes. The decoder is REUSED across clips via setStream(), never end()/begin()-
    // reset (that freezes/crashes in every context tried; see teardownClip in the .cpp).
    bool startClip(const char *filename);   // returns false if the file does not exist
    bool serviceClip();                     // true while a clip is pending or playing
    void stopClip();                        // interrupt the current/pending clip
    bool isClipPlaying();
    uint32_t clipsPlayed();                 // diagnostics: completed clips since boot
    bool isOn();
    void tick();
    size_t readBytes (uint8_t *data, size_t len);
    int available ();
    void audioLoop();                       // audio-task context ONLY: one command + one copy()
	private:
    void teardownClip();                    // audio-task context ONLY
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
    // --- V9.0 async clip state ---
    struct ClipCmd { uint8_t op; char path[40]; };  // op: 0 = stop, 1 = play
    QueueHandle_t clipQueue = nullptr;   // UI -> audio-task mailbox (depth 1, xQueueOverwrite)
    volatile bool clipBusy = false;      // set by UI on play request, cleared by the audio task
    bool clipPlaying = false;            // audio-task private: mixer currently on the decoder
    volatile uint32_t clipsDone = 0;     // diagnostics counter
    size_t   wdLastPos = 0;              // watchdog: last observed file position
    uint32_t wdLastMs = 0;               // watchdog: last time the position advanced
};
