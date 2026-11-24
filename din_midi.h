#pragma once

#include <Arduino.h>

// midi pin definitions
#define MIDI_RX_PIN 16
#define MIDI_TX_PIN 41

void din_midi_init() {
  Serial2.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
}

int din_midi_available() {
  return Serial2.available();
}

unsigned char din_midi_read() {
  return Serial2.read();
}

void din_midi_write(unsigned char data) {
  Serial2.write(data);
}
