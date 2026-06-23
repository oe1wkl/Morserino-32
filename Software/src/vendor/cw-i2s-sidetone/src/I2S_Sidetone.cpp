#include "I2S_Sidetone.hpp"

static void audio_task(void *userData)
{
  StreamCopy *copier = (StreamCopy*)userData;
  // Serial.println("copier task running");
  // Serial.print("Stream Copier status:");
  // Serial.println(copier->isActive());
  while (1) {
    copier->copy();
    // taskYIELD();
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
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, (void*)copier, configMAX_PRIORITIES - 1, nullptr, 1);
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
// Non-blocking MP3 clip playback for menu / voice announcements. Unlike playSPIFFSFile()
// (which blocks and never drains the decoder, so rapid clips overflow the bounded result
// queue and stall the copier task), these reset the decoder per clip via end()+begin() so
// the queue can never accumulate -- enabling smooth, interruptible announcements.

bool I2S_Sidetone::startClip(const char *filename) {
    if (clipPlaying) stopClip();                 // interrupt any current clip first
    if (!SPIFFS.exists(filename)) return false;
    mp3file = SPIFFS.open(filename, "r");
    if (!mp3file) return false;
    decoder->setStream(&mp3file);
    mixer->set(0, *decoder);                     // route the mixer to the decoder
    fileDoneAt = 0;
    clipPlaying = true;
    return true;
}

bool I2S_Sidetone::serviceClip() {
    // Poll from the main loop. Returns true while a clip is still playing.
    if (!clipPlaying) return false;
    if (mp3file.available()) {                    // still feeding the decoder from the file
        fileDoneAt = 0;
        return true;
    }
    // File fully read: keep the mixer on the decoder briefly so the copier task drains the
    // decoded tail to the I2S before we hand the mixer back to the sidetone.
    if (fileDoneAt == 0) fileDoneAt = millis();
    if (millis() - fileDoneAt < 300)
        return true;
    stopClip();
    return false;
}

void I2S_Sidetone::stopClip() {
    if (!clipPlaying) return;
    mixer->set(0, *effects);                      // hand the mixer back to the sidetone path
    decoder->end();                               // reader.end() clears the result queue ...
    decoder->begin(clipCfg);                      // ... and re-begin leaves it clean + ready
    mp3file.close();
    clipPlaying = false;
    fileDoneAt = 0;
}

bool I2S_Sidetone::isClipPlaying() {
    return clipPlaying;
}
