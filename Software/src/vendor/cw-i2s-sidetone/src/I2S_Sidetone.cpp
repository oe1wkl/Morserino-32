#include "I2S_Sidetone.hpp"

static void audio_task(void *userData)
{
  // The audio task owns the whole decode pipeline: it processes clip commands AND runs the
  // copier, strictly sequentially (see I2S_Sidetone::audioLoop). copy() blocks on the I2S
  // DMA queue, which is what lets lower-priority tasks run despite our high priority.
  I2S_Sidetone *st = (I2S_Sidetone*)userData;
  while (1) {
    st->audioLoop();
  }
}

I2S_Sidetone::I2S_Sidetone() {
    frequency=0.0;
}
void I2S_Sidetone::begin(int samplerate, int bps, int channels, int buffer_size) {
    // Don't bump the audio_tools logger up to Info — it dumps a couple
    // dozen lines of init info into the boot log, which is noise for
    // production builds. The library's compile-time default LOG_LEVEL is
    // already Warning, and we restore Error explicitly at the end of
    // begin(). If a downstream user wants the verbose output for
    // debugging, they can call AudioLogger::instance().begin(Serial,
    // AudioLogger::Info) themselves before sidetone.begin().
    i2s = new I2SStream;
#ifdef CONFIG_I2S_DATA_IN_PIN
    I2SConfig config = i2s->defaultConfig(RXTX_MODE);
#else
    I2SConfig config = i2s->defaultConfig(TX_MODE);
#endif
    config.sample_rate = samplerate;
    config.channels = channels;
    config.bits_per_sample = bps;
    config.pin_bck = CONFIG_I2S_BCK_PIN; // define your i2s pins
    config.pin_ws = CONFIG_I2S_LRCK_PIN;
    config.pin_data = CONFIG_I2S_DATA_PIN;
#ifdef CONFIG_I2S_DATA_IN_PIN
    config.pin_data_rx = CONFIG_I2S_DATA_IN_PIN;
#endif
    config.buffer_size=buffer_size;

    // Serial.println("starting I2S...");
    i2s->begin(config);
        
    sine = new ComplexRotorSine();
    in = new GeneratedSoundStream<int16_t>(*sine);

    volume = new VolumeStream(*i2s);
    //lvc = new LogarithmicVolumeControl(0.1); // not using LVC anymore due to direct amp control with codec
    effects = new AudioEffectStream(*in);

    decoder = new EncodedAudioStream(&mp3file, new MP3DecoderHelix());
    decoder->setNotifyActive (false);
    decoder->transformationReader().setResultQueueFactor(14); // workaround see discussion 1828
    decoder->begin(config);
    clipCfg = config;                  // V9.0: save the decode config to re-begin() the decoder per clip

    mixer = new InputMixer<int16_t>();
    mixer->setLimitToAvailableData(true);
    mixer->setNotifyActive (false);
    mixer->add(*effects);
    // mixer->add(*decoder);
    mixer->begin(config);

    copier = new StreamCopy(*volume, *mixer);
    //copier = new StreamCopy(*volume, *mixer, buffer_size); // using "our" buffer size crashes -  https://github.com/pschatzmann/arduino-audio-tools/discussions/1828
    //copier = new StreamCopy(*volume, *mixer, 256); // also crashes

    adsr = new BlackmanHarrisEnvelope(0.005f, 0.005f, samplerate);

    float freq = 600.0;
    sine->begin(config, freq);
    in->begin(config);

    effects->addEffect(*adsr);
    effects->begin(config);

    volume->begin(config);
    //volume->setVolumeControl(*lvc);
    volume->setVolume(0.8);

    AudioLogger::instance().begin(Serial,AudioLogger::Error);
    clipQueue = xQueueCreate(1, sizeof(ClipCmd));   // V9.0: UI -> audio-task command mailbox
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, (void*)this, configMAX_PRIORITIES - 1, nullptr, 1);
}
void I2S_Sidetone::setADSR(float attack=0.001, float decay=0.001, float sustainLevel=0.5, float release=0.005) {
    adsr->setAttackRate(attack);
    adsr->setDecayRate(decay);
    adsr->setReleaseRate(release);
    adsr->setSustainLevel(sustainLevel);
}
size_t I2S_Sidetone::readBytes (uint8_t *data, size_t len) {
    return i2s->readBytes(data,len);
}
int I2S_Sidetone::available() {
    return i2s->available();
}
void I2S_Sidetone::tick() {
    copier->copy();
}
void I2S_Sidetone::setFrequency(float f) {
    sine->setFrequency(f);
    frequency = f;
}
void I2S_Sidetone::setVolume(float v) {
    volume->setVolume(v);
}
float I2S_Sidetone::getFrequency() {
    return frequency;
}
void I2S_Sidetone::on() {
    adsr->keyOn();
}
void I2S_Sidetone::off() {
    adsr->keyOff();
}

bool I2S_Sidetone::playSPIFFSFile(const char *filename) {
    // If an async voice clip is in flight, stop it and wait for the audio task to hand the
    // mixer back: the blocking path below mutates the same decoder/mixer from this task.
    if (clipBusy) {
        stopClip();
        uint32_t t0 = millis();
        while (clipBusy && millis() - t0 < 500) delay(5);
    }
    if(SPIFFS.exists(filename)) {
        mp3file = SPIFFS.open(filename, "r");
        if(!mp3file){
            Serial.println("Failed to open file for reading");
        }
        //Serial.print("File size: ");
        //Serial.println(mp3file.size());
        decoder->setStream(&mp3file);
        mixer->set(0,*decoder);
        while (mp3file.available()) delay(100); // block until copier did copy the whole stream
        mixer->set(0,*effects);
        mp3file.close();
        return true;
    } else
        return false;
}

bool I2S_Sidetone::isOn() {
    return false;
}

// ---- V9.0 async clip playback ---------------------------------------------------------
// Non-blocking MP3 clip playback for menu / voice announcements. The UI-facing calls below
// only post commands into the 1-deep mailbox; ALL pipeline mutation (file open/close, mixer
// routing, decoder reset) happens in audioLoop()/teardownClip() inside the audio task,
// strictly sequential with copy(). History: mutating the pipeline from the UI task while
// the audio task was copying caused freezes (result-queue wedge) and crashes (every
// cross-task decoder end()/begin() attempt) -- do not move any of it back to the UI side.

bool I2S_Sidetone::startClip(const char *filename) {
    if (!clipQueue || !SPIFFS.exists(filename)) return false;
    ClipCmd cmd; cmd.op = 1;
    strlcpy(cmd.path, filename, sizeof(cmd.path));
    clipBusy = true;                 // reflect the request until the audio task takes over
    xQueueOverwrite(clipQueue, &cmd);
    return true;
}

bool I2S_Sidetone::serviceClip() {
    // Poll from the main loop: true while a clip is pending or playing. The audio task does
    // all the work; this is a pure state query now.
    return clipBusy;
}

void I2S_Sidetone::stopClip() {
    if (!clipQueue || !clipBusy) return;
    ClipCmd cmd; cmd.op = 0; cmd.path[0] = '\0';
    xQueueOverwrite(clipQueue, &cmd);   // overwrites an unconsumed play request: latest wins
}

bool I2S_Sidetone::isClipPlaying() {
    return clipBusy;
}

uint32_t I2S_Sidetone::clipsPlayed() {
    return clipsDone;
}

void I2S_Sidetone::audioLoop() {
    // AUDIO-TASK CONTEXT ONLY. One command, clip progress bookkeeping, then one copy().
    // Everything here runs strictly sequentially with copy(), so the pipeline is never
    // mutated while it is being read.
    ClipCmd cmd;
    if (clipQueue && xQueueReceive(clipQueue, &cmd, 0) == pdTRUE) {
        if (clipPlaying) teardownClip();             // interrupt whatever is playing
        if (cmd.op == 1) {
            mp3file = SPIFFS.open(cmd.path, "r");
            if (mp3file) {
                decoder->setStream(&mp3file);
                mixer->set(0, *decoder);             // route the mixer to the decoder
                clipPlaying = true;
                clipBusy = true;
                wdLastPos = 0; wdLastMs = millis();
            } else
                clipBusy = false;                    // open failed -> back to idle
        } else
            clipBusy = false;                        // stop with nothing (left) playing
    }
    if (clipPlaying) {
        if (!mp3file.available()) {
            // File fully read -> hand the mixer back immediately (like the proven blocking
            // path); holding the mixer on the decoder past EOF is what wedged it before.
            teardownClip();
        } else {
            // Watchdog: if the file position stops advancing, the pipeline is stuck --
            // force-recover instead of hanging forever. Normal clips are ~1 s long.
            size_t pos = mp3file.position();
            if (pos != wdLastPos) { wdLastPos = pos; wdLastMs = millis(); }
            else if (millis() - wdLastMs > 2000) teardownClip();
        }
    }
    copier->copy();
}

void I2S_Sidetone::teardownClip() {
    // AUDIO-TASK CONTEXT ONLY. Resetting the decoder here cannot race the copier (same
    // task, sequential) -- this is the safe home for the end()/begin() reset that clears
    // whatever the reused Helix decoder / reader queue accumulates across many clips
    // (the cause of the freeze that only appeared after very many announcements).
    mixer->set(0, *effects);              // hand the mixer back to the sidetone path
    mp3file.close();
    decoder->end();
    decoder->begin(clipCfg);
    clipPlaying = false;
    clipsDone = clipsDone + 1;
    clipBusy = false;
}
