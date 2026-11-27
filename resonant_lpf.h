
#pragma once

#include <math.h>

typedef struct {
  float target_cutoff;
  float target_resonance;

  // Audio-task-owned state
  float cutoff;
  float resonance;
  float low;
  float band;
  float frequency;
  float damping;
} resonant_lpf;

void resonant_lpf_init(resonant_lpf * filter) {
  filter->target_cutoff = 1000.0f;
  filter->target_resonance = 0.0f;

  filter->cutoff = 1000.0f;
  filter->resonance = 0.0f;

  filter->low = 0.0f;
  filter->band = 0.0f;

  filter->frequency = 0.0f;
  filter->damping = 1.0f;
}

void resonant_lpf_set_frequency(resonant_lpf * filter, float frequency) {
  if (frequency < 20.0f) frequency = 20.0f;
  if (frequency > SAMPLE_RATE * 0.45f) {
    frequency = SAMPLE_RATE * 0.45f;
  }

  filter->target_cutoff = frequency;
}

void resonant_lpf_set_resonance(resonant_lpf * filter, float resonance) {
  if (resonance < 0.0f) resonance = 0.0f;
  if (resonance > 1.0f) resonance = 1.0f;

  filter->target_resonance = resonance;
}

void resonant_lpf_process_setup(resonant_lpf * filter) {
  // Smooth parameter changes
  filter->cutoff += 0.05f *
    (filter->target_cutoff - filter->cutoff);

  filter->resonance += 0.05f *
    (filter->target_resonance - filter->resonance);

  // Map resonance to damping.
  // Higher resonance produces a stronger resonant peak.
  filter->damping = 2.0f * (1.0f - filter->resonance);
  if (filter->damping < 0.05f) filter->damping = 0.05f;

  // Normalized SVF frequency coefficient
  filter->frequency = 2.0f * sinf(
    3.14159265359f * filter->cutoff / SAMPLE_RATE
  );

  if (filter->frequency > 0.99f) filter->frequency = 0.99f;
}

float resonant_lpf_process(resonant_lpf * filter, float input) {

  // State-variable filter
  filter->low += filter->frequency * filter->band;

  float high = input
    - filter->low
    - filter->damping * filter->band;

  filter->band += filter->frequency * high;

  return filter->low;
}
