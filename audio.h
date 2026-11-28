#pragma once

#include <Arduino.h>

#include "driver/i2s_std.h"

#define CONTROL_RATE 60.0f

float noteFreq[128];

int samplecounter = 0;

constexpr int SAMPLE_RATE = 44100;

#include "wavetable_osc.h"
#include "ar_envelope.h"
#include "resonant_lpf.h"

#define DISPSAMPLECOUNT 128
float dispsamples[128];
int dispsamplehead = 0;

// -------- I2S wiring --------
constexpr int I2S_BCLK = 5;
constexpr int I2S_LRCK = 6;
constexpr int I2S_DOUT = 7;

// -------- Audio settings --------
constexpr int BLOCK_FRAMES = 128;
constexpr float MAX_AMPLITUDE = 0.07f;

// Protect shared audio parameters between tasks
portMUX_TYPE synthMux = portMUX_INITIALIZER_UNLOCKED;

int activeNote = -1;
float pitchBendSemitones = 0.0f;
float pitchBendRange = 12.0f;

// -------- I2S driver --------
i2s_chan_handle_t txHandle = nullptr;

wavetable_osc lfo;

// synth voice
typedef struct {
  wavetable_osc osc;
  ar_envelope env;
  resonant_lpf filter;
  float cutoff;
  int note;
} synthvoice;
#define VOICES 8
synthvoice voice[VOICES];
int nextvoice = 0;

float lfo_pitch_depth = 0.0f;
float lfo_filter_depth = 0.0f;
float env_filter_depth = 0.0f;
#define FILTER_ENV_RANGE 8000.0f;

#define OUTPUT_DRIVE 1.0f

// Cubic soft clipper: smooth, no divide, output is exactly +/-1 for |x| >= 1.5
static inline float soft_clip(float x) {
  if (x >= 1.5f)  return 1.0f;
  if (x <= -1.5f) return -1.0f;
  return x - (x * x * x) * (4.0f / 27.0f);
}

void audio_set_env_filter_depth(float amount) {
  if (amount < 0.0f) amount = 0.0f;
  if (amount > 1.0f) amount = 1.0f;
  env_filter_depth = amount;
}

void audio_set_lfo_filter_depth(float depth) {
  lfo_filter_depth = depth;
}

float calculate_modulated_cutoff(float baseCutoff, float lfoValue, float amount) {

  float modulation = lfoValue * amount * 4000.0f;

    float cutoff = baseCutoff + modulation;

    if (cutoff < 20.0f) cutoff = 20.0f;
    if (cutoff > SAMPLE_RATE * 0.45f) {
        cutoff = SAMPLE_RATE * 0.45f;
    }

    return cutoff;
}

void audio_set_lfo_freq(float freq) {
  wavetable_osc_set_freq(&lfo, freq);
}

void audio_set_lfo_pitch_depth(float depth) {
  lfo_pitch_depth = depth;
}

void synthvoice_init(synthvoice * v) {

  wavetable_osc_init(&v->osc);
  ar_envelope_init(&v->env);
  resonant_lpf_init(&v->filter);
  
  v->cutoff = 500;
  v->note = -1;
}

void audio_set_cutoff(float cutoff) {
  for (int v = 0; v < VOICES; v++) {
    //    resonant_lpf_set_frequency(&voice[v].filter, cutoff);
    voice[v].cutoff = cutoff;
  }
}

void audio_set_resonance(float resonance) {
  for (int v = 0; v < VOICES; v++) {
    resonant_lpf_set_resonance(&voice[v].filter, resonance);
  }
}

float computeFrequency(int note, float bendSemitones, float lfoval) {
  if (note < 0) return 0.0f;
  float effectiveNote = (float)note + bendSemitones + lfoval;
  float frequency = 440.0f * powf(2.0f, (effectiveNote - 69.0f) / 12.0f);
  return frequency;
}

void audio_set_wavetable(int table) {
  for (int v=0; v<VOICES; v++) {
    wavetable_osc_set_table(&voice[v].osc, table);
  }
}

void audio_set_env_mode(int mode) {
}

// ------------ envelope controls ---------
void audio_set_attack(float seconds) {

  for (int v=0; v<VOICES; v++) {  
    ar_envelope_set_attack(&voice[v].env, seconds);
  }
}

void audio_set_release(float seconds) {

    for (int v=0; v<VOICES; v++) {
      ar_envelope_set_release(&voice[v].env, seconds);
    }
}

void audio_init() {

  for (int n = 0; n < 128; n++) {
    noteFreq[n] = 440.0f * powf(2.0f, (n - 69) / 12.0f);
  }

  wavetable_osc_init(&lfo);
  wavetable_osc_set_freq(&lfo, 1.0f);
  wavetable_osc_set_table(&lfo, 2);

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
    
    voice[vn].env.target_amplitude = MAX_AMPLITUDE * (velocity / 127.0f);
    ar_envelope_set_gate(&voice[vn].env, 1);
    voice[vn].note = note;
    
  } else { // note off
    
    for (int v=0; v<VOICES; v++) {
      if (note == voice[v].note && voice[v].env.stage != ENV_IDLE) {
	ar_envelope_set_gate(&voice[v].env, 0);
      }
    }
  }

}

void audio_set_note(int note, int velocity) {
  updateSynthVoice(note, velocity);
}

void audio_note_off(int note) {
  updateSynthVoice(note, 0);
}

void audio_set_pitchbend_range(float semitones) {
  pitchBendRange = semitones;
}

void audio_set_pitchbend(float value) {
  
  if (value > 1.0f) value = 1.0f;
  if (value < -1.0f) value = -1.0f;

  pitchBendSemitones = value * pitchBendRange;
}

void audioTask(void* parameter) {
	
  int16_t buffer[BLOCK_FRAMES * 2];

  static int samples_per_control_rate = SAMPLE_RATE / CONTROL_RATE;

  for (;;) {

    for (int v=0; v<VOICES; v++) {
      resonant_lpf_process_setup(&voice[v].filter);
    }

    for (int i = 0; i < BLOCK_FRAMES; i++) {

      wavetable_osc_process(&lfo);
      float lfoval = wavetable_osc_get_sample(&lfo);

      // control rate
      if (samplecounter % samples_per_control_rate == 0) {

	float pitchMul = exp2f((pitchBendSemitones + lfoval * lfo_pitch_depth) / 12.0f);

	for (int v=0; v<VOICES; v++) {

	  if (voice[v].env.stage == ENV_IDLE) continue;

	  wavetable_osc_set_freq(&voice[v].osc, noteFreq[voice[v].note] * pitchMul);

	  // envelope normalised to 0..1, independent of velocity
	  float target = voice[v].env.target_amplitude;
	  float envn = (target > 0.0f) ? voice[v].env.level / target : 0.0f;

	  float base = voice[v].cutoff + env_filter_depth * envn * FILTER_ENV_RANGE;
	  float mc = calculate_modulated_cutoff(base, lfoval, lfo_filter_depth);
	  resonant_lpf_set_frequency(&voice[v].filter, mc);
	}
      }

      float sample = 0.0f;

      for (int v=0; v<VOICES; v++) {
	if (voice[v].env.stage == ENV_IDLE) continue;
	wavetable_osc_process(&voice[v].osc);
	ar_envelope_process(&voice[v].env);
	float s = wavetable_osc_get_sample(&voice[v].osc) * voice[v].env.level;
	sample += resonant_lpf_process(&voice[v].filter, s);
      }

      sample = soft_clip(sample * OUTPUT_DRIVE);

      if (sample > 1.0) sample = 1.0;
      if (sample < -1.0) sample = -1.0;

      int16_t pcm = (int16_t)(sample * 32767.0f);

      dispsamples[dispsamplehead] = sample;
      dispsamplehead = (dispsamplehead + 1) % DISPSAMPLECOUNT;

      // Duplicate mono signal to left and right.
      buffer[i * 2] = pcm;
      buffer[i * 2 + 1] = pcm;

      samplecounter++;
    }

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

  audio_init();

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
