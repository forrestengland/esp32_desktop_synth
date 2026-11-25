#pragma once

/* din midi read / write using Serial2

   We have incoming bytes. need to find out when we have a full midi message.
   We can find out if a byte is a status byte by testing the most significant
   bit. if it's 1 it's a status byte, otherwise it's a data byte.
*/

#include <Arduino.h>

// midi pin definitions
#define MIDI_RX_PIN 16
#define MIDI_TX_PIN 41

int din_midi_have_status = 0;
#define DIN_MIDI_DEBUG_MESSAGE_SIZE 128
char din_midi_debug_message[DIN_MIDI_DEBUG_MESSAGE_SIZE];

// start midi din
void din_midi_init() {
  Serial2.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
}

// returns true if midi byte is available, otherwise false
int din_midi_available() {
  return Serial2.available();
}

// read an incoming midi byte
unsigned char din_midi_read() {

  unsigned char midi_byte = Serial2.read();

  int is_status_byte = (midi_byte & (1U << 7)) != 0;
  snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
	   "din_midi: got status byte 0x%02x\n", midi_byte);
  
  return midi_byte;
}

// write an outgoing midi byte
void din_midi_write(unsigned char data) {
  Serial2.write(data);
}
