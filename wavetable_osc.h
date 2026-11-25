#pragma once

#define WAVETABLE_OSC_SIZE 1024

float wavetable_osc_saw_table[WAVETABLE_OSC_SIZE];
float wavetable_osc_square_table[WAVETABLE_OSC_SIZE];

int wavetable_osc_tables_initialized = 0;

// wavetable osc
enum wavetable_osc_waveform {
    WAVETABLE_OSC_WAVEFORM_SAW,
    WAVETABLE_OSC_WAVEFORM_SQUARE
};

typedef struct {
  wavetable_osc_waveform current_waveform;
  float target_phase_increment;
  // audio task only oscillator state
  float phase;
  float phase_increment;
  float block_phase_increment;
} wavetable_osc;

void wavetable_osc_initialize_wavetables() {

  for (int i = 0; i < WAVETABLE_OSC_SIZE; i++) {
    float position = (float)i / WAVETABLE_OSC_SIZE;

    // Sawtooth: -1.0 to nearly +1.0
    wavetable_osc_saw_table[i] = (2.0f * position) - 1.0f;

    // Square: first half positive, second half negative
    wavetable_osc_square_table[i] = (position < 0.5f) ? 1.0f : -1.0f;
  }

  Serial.println("Wavetables initialized.");
}

void wavetable_osc_init(wavetable_osc * osc) {

  osc->current_waveform = WAVETABLE_OSC_WAVEFORM_SAW;
  osc->target_phase_increment = 0.0f;
  osc->phase = 0.0f;
  osc->phase_increment = 0.0f;
  osc->block_phase_increment = 0.0f;

  if (!wavetable_osc_tables_initialized) {
    wavetable_osc_initialize_wavetables();
    wavetable_osc_tables_initialized = 1;
  }
}

void wavetable_osc_set_phase_increment(wavetable_osc * osc, float phase_inc) {
  osc->target_phase_increment = phase_inc;
}

void wavetable_osc_audio_setup(wavetable_osc * osc) {
  osc->block_phase_increment = osc->target_phase_increment;
}

void wavetable_osc_sample_begin(wavetable_osc * osc) {
  // smooth pitch updates to prevent zipper noise
  osc->phase_increment += 0.01f * (osc->block_phase_increment - osc->phase_increment);

}

float wavetable_osc_get_sample(wavetable_osc * osc) {

  int index = (int)osc->phase;
  float fraction = osc->phase - index;

  int nextIndex = index + 1;

  if (nextIndex >= WAVETABLE_OSC_SIZE) {
    nextIndex = 0;
  }

  float* table =
      (osc->current_waveform == WAVETABLE_OSC_WAVEFORM_SAW)
      ? wavetable_osc_saw_table
      : wavetable_osc_square_table;

  float sample = table[index] +
      (table[nextIndex] - table[index]) * fraction;

  osc->phase += osc->phase_increment;

  while (osc->phase >= WAVETABLE_OSC_SIZE) {
    osc->phase -= WAVETABLE_OSC_SIZE;
  }

  return sample;
}
