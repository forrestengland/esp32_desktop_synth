#pragma once

#include <Arduino.h>

#include "driver/i2s_std.h"

#include "moogladder.h"

constexpr int SAMPLE_RATE = 44100;

#include "wavetable_osc.h"
#include "ar_envelope.h"

#define DISPSAMPLECOUNT 128
float dispsamples[128];
int dispsamplehead = 0;


// -------- I2S wiring --------
constexpr int I2S_BCLK = 5;
constexpr int I2S_LRCK = 6;
constexpr int I2S_DOUT = 7;

// -------- Audio settings --------
constexpr int BLOCK_FRAMES = 128;
constexpr float MAX_AMPLITUDE = 0.5f;

// Protect shared audio parameters between tasks
portMUX_TYPE synthMux = portMUX_INITIALIZER_UNLOCKED;

int activeNote = -1;
float pitchBendSemitones = 0.0f;
float pitchBendRange = 12.0f;

// -------- I2S driver --------
i2s_chan_handle_t txHandle = nullptr;

// daisysp::MoogLadder flt;

// synth voice
typedef struct {
  wavetable_osc osc;
  ar_envelope env;
  daisysp::MoogLadder flt;
  float cutoff;
  int note;
} synthvoice;
#define VOICES 4
synthvoice voice[VOICES];
int nextvoice = 0;

void synthvoice_init(synthvoice * v) {
  wavetable_osc_init(&v->osc);
  ar_envelope_init(&v->env);
  v->flt.Init(SAMPLE_RATE);
  //  v->flt.SetFreq(500);  
  v->flt.SetRes(0.5);
  v->cutoff = 500;
  v->note = -1;
}

// compute phase increment from note + bend
inline float computePhaseIncrement(int note, float bendSemitones) {
  if (note < 0) return 0.0f;
  float effectiveNote = (float)note + bendSemitones;
  float frequency = 440.0f * powf(2.0f, (effectiveNote - 69.0f) / 12.0f);
  return (frequency * WAVETABLE_OSC_SIZE) / SAMPLE_RATE;
}

void audio_set_wavetable(int table) {
  for (int v=0; v<VOICES; v++) {
    wavetable_osc_set_table(&voice[v].osc, table);
  }
}

void audio_set_env_mode(int mode) {
  for (int v=0; v<VOICES; v++) {
    voice[v].env.mode = mode;
  }
}

// ------------ envelope controls ---------
void audio_set_attack(float seconds) {

  portENTER_CRITICAL(&synthMux);
  for (int v=0; v<VOICES; v++) {  
    ar_envelope_set_attack(&voice[v].env, seconds);
  }
  portEXIT_CRITICAL(&synthMux);
}

void audio_set_release(float seconds) {

  portENTER_CRITICAL(&synthMux);
  for (int v=0; v<VOICES; v++) {
    ar_envelope_set_release(&voice[v].env, seconds);
  }
  portEXIT_CRITICAL(&synthMux);
}

void audio_set_cutoff(float cutoff) {
  for (int v=0; v<VOICES; v++) {
    //    voice[v].flt.SetFreq(cutoff);
    voice[v].cutoff = cutoff;
  }
}

void audio_set_resonance(float resonance) {
  for (int v=0; v<VOICES; v++) {  
    voice[v].flt.SetRes(resonance);
  }
}

void audio_init() {

  //  flt.Init(SAMPLE_RATE);

  //  wavetable_osc_init(&osc);
  //  ar_envelope_init(&env);
  for (int v=0; v<VOICES; v++) {
    synthvoice_init(&voice[v]);
  }
  
  i2s_chan_config_t chanConfig =
      I2S_CHANNEL_DEFAULT_CONFIG(
          I2S_NUM_AUTO,
          I2S_ROLE_MASTER
      );

  ESP_ERROR_CHECK(
      i2s_new_channel(&chanConfig, &txHandle, nullptr)
  );

  i2s_std_config_t stdConfig = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),

      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_16BIT,
          I2S_SLOT_MODE_STEREO
      ),

      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = (gpio_num_t)I2S_BCLK,
          .ws = (gpio_num_t)I2S_LRCK,
          .dout = (gpio_num_t)I2S_DOUT,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false
          }
      }
  };

  ESP_ERROR_CHECK(
      i2s_channel_init_std_mode(txHandle, &stdConfig)
  );

  ESP_ERROR_CHECK(i2s_channel_enable(txHandle));

  Serial.println("I2S audio started.");
}

void updateSynthVoice(int note, int velocity) {

  portENTER_CRITICAL(&synthMux);

  if (velocity > 0) {

    int vn = -1;
    for (int v=0; v<VOICES; v++) {
      if (voice[v].note == note || voice[v].env.stage == ENV_IDLE) {
	vn = v;
	break;
      }
    }

    if (vn < 0) {
      vn = nextvoice;
      nextvoice = (nextvoice + 1) % VOICES;
    }
    
    float targetPhaseIncrement = computePhaseIncrement(note, pitchBendSemitones);
    wavetable_osc_set_phase_increment(&voice[vn].osc, targetPhaseIncrement);
    voice[vn].env.target_amplitude = MAX_AMPLITUDE * (velocity / 127.0f);
    ar_envelope_set_gate(&voice[vn].env, 1);
    voice[vn].note = note;
  } else {
    for (int v=0; v<VOICES; v++) {
      if (note == voice[v].note && voice[v].env.stage != ENV_IDLE) {
	ar_envelope_set_gate(&voice[v].env, 0);
	//	activeNote = -1;
      }
    }
  }

  portEXIT_CRITICAL(&synthMux);
}

void audio_set_note(int note, int velocity) {
  updateSynthVoice(note, velocity);
}

void audio_note_off(int note) {
  updateSynthVoice(note, 0);
}

void audio_set_pitchbend_range(float semitones) {
  portENTER_CRITICAL(&synthMux);
  pitchBendRange = semitones;
  portEXIT_CRITICAL(&synthMux);
}

void audio_set_pitchbend(float value) {
  
  if (value > 1.0f) value = 1.0f;
  if (value < -1.0f) value = -1.0f;

  portENTER_CRITICAL(&synthMux);

  pitchBendSemitones = value * pitchBendRange;
  //  if (activeNote >= 0) {
  for (int v=0; v<VOICES; v++) {
    if (voice[v].note >= 0) {
      float targetPhaseIncrement = computePhaseIncrement(voice[v].note, pitchBendSemitones);
      wavetable_osc_set_phase_increment(&voice[v].osc, targetPhaseIncrement);
    }
  }

  portEXIT_CRITICAL(&synthMux);
}

void audioTask(void* parameter) {
	
  int16_t buffer[BLOCK_FRAMES * 2];

  for (;;) {

    portENTER_CRITICAL(&synthMux);

    for (int v=0; v<VOICES; v++) {
      wavetable_osc_audio_setup(&voice[v].osc);
      ar_envelope_audio_setup(&voice[v].env);
    }

    portEXIT_CRITICAL(&synthMux);

    for (int i = 0; i < BLOCK_FRAMES; i++) {
      
      for (int v=0; v<VOICES; v++) {
	wavetable_osc_sample_begin(&voice[v].osc);
	ar_envelope_sample_begin(&voice[v].env);
      }
      
      float sample = 0.0f;

      for (int v=0; v<VOICES; v++) {
	if (voice[v].env.level > 0.0f) {
	  float s = wavetable_osc_get_sample(&voice[v].osc) * voice[v].env.level;
	  voice[v].flt.SetFreq(voice[v].cutoff * voice[v].env.level);
	  sample += voice[v].flt.Process(s);
	}
      }

      if (sample > 1.0) sample = 1.0;
      if (sample < -1.0) sample = -1.0;

      int16_t pcm = (int16_t)(sample * 32767.0f);

      dispsamples[dispsamplehead] = sample;
      dispsamplehead = (dispsamplehead + 1) % DISPSAMPLECOUNT;

      // Duplicate mono signal to left and right.
      buffer[i * 2] = pcm;
      buffer[i * 2 + 1] = pcm;

    }

    // Write back stage transition if release finished mid-block
    portENTER_CRITICAL(&synthMux);
    for (int v=0; v<VOICES; v++) {
      ar_envelope_audio_cleanup(&voice[v].env);
    }
    portEXIT_CRITICAL(&synthMux);    

    size_t bytesWritten = 0;

    esp_err_t result = i2s_channel_write(txHandle, buffer,
					 sizeof(buffer), &bytesWritten,
					 1000);

    if (result != ESP_OK) {
      Serial.printf("I2S write error: %d\n", result);
    }
  }
}

void audio_start() {

  //  initializeWavetables();
  audio_init();
  //  updateSynthVoice(60, 100);

  BaseType_t taskResult = xTaskCreatePinnedToCore(audioTask, "AudioTask",
						  4096, nullptr, 3,
						  nullptr, 0);

  if (taskResult != pdPASS) {
    Serial.println("ERROR: Failed to create audio task!");

    while (true) {
      delay(1000);
    }
  }
}
