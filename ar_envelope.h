#pragma once

// #define SAMPLE_RATE 44100

enum ar_envelope_stage {
  ENV_IDLE,
  ENV_ATTACK,
  ENV_RELEASE
};

typedef struct {
  ar_envelope_stage stage;
  float level;
  float attack_time_sec;
  float release_time_sec;
  float attack_step;
  float release_step;
  float target_amplitude;
  float block_target_amplitude;
  float block_attack_step;
  float block_release_step;
  ar_envelope_stage block_stage;
} ar_envelope;

void ar_envelope_init(ar_envelope * env) {
  env->stage = ENV_IDLE;
  env->level = 0.0f;
  env->attack_time_sec = 0.01f;
  env->release_time_sec = 0.20f;
  env->attack_step = 1.0f / (0.01f * SAMPLE_RATE);
  env->release_step = 1.0f / (0.2f * SAMPLE_RATE);
  env->target_amplitude = 0.0f;
}

void ar_envelope_set_attack(ar_envelope * env, float seconds) {

  if (seconds < 0.001f) seconds = 0.001f;

  env->attack_time_sec = seconds;
  env->attack_step = 1.0f / (env->attack_time_sec * SAMPLE_RATE);
  
}

void ar_envelope_set_release(ar_envelope * env, float seconds) {

  if (seconds < 0.001f) seconds = 0.001f;

  env->release_time_sec = seconds;
  env->release_step = 1.0f / (env->release_time_sec * SAMPLE_RATE);

}

void ar_envelope_set_gate(ar_envelope * env, int gate) {
  if (gate) env->stage = ENV_ATTACK;
  else env->stage = ENV_RELEASE;
}

void ar_envelope_audio_setup(ar_envelope * env) {
    env->block_target_amplitude = env->target_amplitude;
    env->block_attack_step = env->attack_step;
    env->block_release_step = env->release_step;
    env->block_stage = env->stage;
}

void ar_envelope_sample_begin(ar_envelope * env) {

  if (env->stage == ENV_ATTACK) {
    env->level += env->block_attack_step * env->block_target_amplitude;
    if (env->level >= env->block_target_amplitude) {
      env->level = env->block_target_amplitude;
    }
  } else if (env->stage == ENV_RELEASE) {
    env->level -= env->block_release_step * env->block_target_amplitude;
    if (env->level <= 0.0f) {
      env->level = 0.0f;
      env->block_stage = ENV_IDLE;
    }
  }
}

void ar_envelope_audio_cleanup(ar_envelope * env) {
  if (env->stage == ENV_RELEASE && env->block_stage == ENV_IDLE) {
    env->stage = ENV_IDLE;
  }
}
