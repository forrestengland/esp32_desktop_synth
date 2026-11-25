#pragma once

#include <Arduino.h>

#include "driver/i2s_std.h"

// -------- I2S wiring --------
constexpr int I2S_BCLK = 5;
constexpr int I2S_LRCK = 6;
constexpr int I2S_DOUT = 7;

// -------- Audio settings --------
constexpr int SAMPLE_RATE = 44100;
constexpr int BLOCK_FRAMES = 128;
constexpr float MAX_AMPLITUDE = 0.25f;

// wavetable osc
enum Waveform {
    WAVE_SAW,
    WAVE_SQUARE
};

Waveform currentWaveform = WAVE_SAW;

// -------- Wavetable settings --------
#define WAVETABLE_SIZE 1024

float sawTable[WAVETABLE_SIZE];
float squareTable[WAVETABLE_SIZE];

// ---------AR envelope -------
enum EnvStage {
  ENV_IDLE,
  ENV_ATTACK,
  ENV_RELEASE
};

// Protect shared audio parameters between tasks
portMUX_TYPE synthMux = portMUX_INITIALIZER_UNLOCKED;

int activeNote = -1;
float pitchBendSemitones = 0.0f;
float pitchBendRange = 12.0f;

float targetPhaseIncrement = 0.0f;
float targetAmplitude = 0.0f;

// envelope state & rates (per-sample linear step)
EnvStage envStage = ENV_IDLE;
float envLevel = 0.0f;
float attackTimeSec = 0.01f;
float releaseTimeSec = 0.20f;
float attackStep = 1.0f / (0.01f * SAMPLE_RATE);
float releaseStep = 1.0f / (0.2f * SAMPLE_RATE);

// audio task only oscillator state
float phase = 0.0f;
float phaseIncrement = 0.0f;

// -------- I2S driver --------
i2s_chan_handle_t txHandle = nullptr;

// compute phase increment from note + bend
inline float computePhaseIncrement(int note, float bendSemitones) {
  if (note < 0) return 0.0f;
  float effectiveNote = (float)note + bendSemitones;
  float frequency = 440.0f * powf(2.0f, (effectiveNote - 69.0f) / 12.0f);
  return (frequency * WAVETABLE_SIZE) / SAMPLE_RATE;
}

float getOscillatorSample() {

  int index = (int)phase;
  float fraction = phase - index;

  int nextIndex = index + 1;

  if (nextIndex >= WAVETABLE_SIZE) {
    nextIndex = 0;
  }

  float* table =
      (currentWaveform == WAVE_SAW)
      ? sawTable
      : squareTable;

  float sample = table[index] +
      (table[nextIndex] - table[index]) * fraction;

  phase += phaseIncrement;

  while (phase >= WAVETABLE_SIZE) {
    phase -= WAVETABLE_SIZE;
  }

  return sample;
}

// ------------ envelope controls ---------
void audio_set_attack(float seconds) {

  if (seconds < 0.001f) seconds = 1.001f;

  portENTER_CRITICAL(&synthMux);
  attackTimeSec = seconds;
  attackStep = 1.0f / (attackTimeSec * SAMPLE_RATE);
  portEXIT_CRITICAL(&synthMux);
}

void audio_set_release(float seconds) {

  if (seconds < 0.001f) seconds = 0.001f;

  portENTER_CRITICAL(&synthMux);
  releaseTimeSec = seconds;
  releaseStep = 1.0f / (releaseTimeSec * SAMPLE_RATE);
  portEXIT_CRITICAL(&synthMux);
}

void audio_init() {
  
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
    activeNote = note;
    targetPhaseIncrement = computePhaseIncrement(activeNote, pitchBendSemitones);
    targetAmplitude = MAX_AMPLITUDE * (velocity / 127.0f);
    envStage = ENV_ATTACK;
  } else {
    // note off (velocity == 0)
    // only release if matches current note
    if (note == activeNote && envStage != ENV_IDLE) {
      envStage = ENV_RELEASE;
      activeNote = -1;
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
  if (activeNote >= 0) {
    targetPhaseIncrement = computePhaseIncrement(activeNote, pitchBendSemitones);
  }

  portEXIT_CRITICAL(&synthMux);
}

void audioTask(void* parameter) {
	
  int16_t buffer[BLOCK_FRAMES * 2];

	for (;;) {

    float blockTargetAmplitude;
    float blockPhaseIncrement;
    float blockAttackStep;
    float blockReleaseStep;
    EnvStage stage;

    portENTER_CRITICAL(&synthMux);

    blockTargetAmplitude = targetAmplitude;
    blockPhaseIncrement = targetPhaseIncrement;
    blockAttackStep = attackStep;
    blockReleaseStep = releaseStep;
    stage = envStage;

    portEXIT_CRITICAL(&synthMux);

    for (int i = 0; i < BLOCK_FRAMES; i++) {

      //      phaseIncrement = blockPhaseIncrement;
      // smooth pitch updates to prevent zipper noise
      phaseIncrement += 0.01f * (blockPhaseIncrement - phaseIncrement);

      if (stage == ENV_ATTACK) {
	envLevel += blockAttackStep * blockTargetAmplitude;
	if (envLevel >= blockTargetAmplitude) {
	  envLevel = blockTargetAmplitude;
	}
      } else if (stage == ENV_RELEASE) {
	envLevel -= blockReleaseStep * blockTargetAmplitude;
	if (envLevel <= 0.0f) {
	  envLevel = 0.0f;
	  stage = ENV_IDLE;
	}
      }
      
      float sample = 0.0f;
      if (envLevel > 0.0f) {
	sample = getOscillatorSample() * envLevel;
      }

      int16_t pcm = (int16_t)(sample * 32767.0f);

      // Duplicate mono signal to left and right.
      buffer[i * 2] = pcm;
      buffer[i * 2 + 1] = pcm;

    }

    // Write back stage transition if release finished mid-block
    portENTER_CRITICAL(&synthMux);
    if (envStage == ENV_RELEASE && stage == ENV_IDLE) {
      envStage = ENV_IDLE;
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

void initializeWavetables() {

  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    float position = (float)i / WAVETABLE_SIZE;

    // Sawtooth: -1.0 to nearly +1.0
    sawTable[i] = (2.0f * position) - 1.0f;

    // Square: first half positive, second half negative
    squareTable[i] = (position < 0.5f) ? 1.0f : -1.0f;
  }

  Serial.println("Wavetables initialized.");
}

void audio_start() {

  initializeWavetables();

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

  updateSynthVoice(60, 100);
}

/* void audio_loop() {

  delay(2000);

  portENTER_CRITICAL(&synthMux);
  currentWaveform = (currentWaveform == WAVE_SAW) ? WAVE_SQUARE : WAVE_SAW;
  portEXIT_CRITICAL(&synthMux);

  Serial.println(currentWaveform == WAVE_SAW ? "SAW" : "SQUARE");
  } */
