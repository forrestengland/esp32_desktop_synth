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

int have_status = 0;
int have_data1 = 0;
int have_data2 = 0;

uint8_t last_status_byte = 0;
uint8_t last_data1_byte = 0;
uint8_t last_data2_byte = 0;

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
  if (is_status_byte) {
    //    snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
    //	     "din_midi: got status byte 0x%02x\n", midi_byte);
    last_status_byte = midi_byte;
    have_status = 1;
    have_data1 = 0;
    have_data2 = 0;
    last_data1_byte = 0;
    last_data2_byte = 0;
  } else {

    // not status byte. check if last status byte was note on
    if (have_status) {
      
      // note on
      if ((last_status_byte & 0xF0) == 0x90) {

	if (!have_data1) {

	  last_data1_byte = midi_byte;
	  have_data1 = 1;
	} else if (!have_data2) {

	  last_data2_byte = midi_byte;
	  have_data2 = 1;

	  // low 4 bits of status byte are midi channel
	  int channel = last_status_byte & 0x0F;

	  // have complete note on message.
	  snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
		   "din_midi: got note on: channel %d, note %d, vel %d\n", channel, last_data1_byte, last_data2_byte);
	  Serial.print(din_midi_debug_message);
	  last_status_byte = 0;
	  last_data1_byte = 0;
	  last_data2_byte = 0;
	  have_status = 0;
	  have_data1 = 0;
	  have_data2 = 0;
	}
      } else if ((last_status_byte & 0xF0) == 0x80) { // note off

	if (!have_data1) {

	  last_data1_byte = midi_byte;
	  have_data1 = 1;
	  
	} else if (!have_data2) {

	  last_data2_byte = midi_byte;
	  have_data2 = 1;

	  // low 4 bits of status byte are midi channel
	  int channel = last_status_byte & 0x0F;

	  // have complete note off message.
	  snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
		   "din_midi: got note off: channel %d, note %d, vel %d\n", channel, last_data1_byte, last_data2_byte);
	  Serial.print(din_midi_debug_message);
	  last_status_byte = 0;
	  last_data1_byte = 0;
	  last_data2_byte = 0;
	  have_status = 0;
	  have_data1 = 0;
	  have_data2 = 0;
	}
      } else if ((last_status_byte & 0xF0) == 0xb0) { // control change

	if (!have_data1) {

	  last_data1_byte = midi_byte;
	  have_data1 = 1;
	  
	} else if (!have_data2) {

	  last_data2_byte = midi_byte;
	  have_data2 = 1;

	  // low 4 bits of status byte are midi channel
	  int channel = last_status_byte & 0x0F;

	  // have complete control change message.
	  snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
		   "din_midi: got control change: channel %d, control %d, val %d\n", channel, last_data1_byte, last_data2_byte);
	  Serial.print(din_midi_debug_message);
	  last_status_byte = 0;
	  last_data1_byte = 0;
	  last_data2_byte = 0;
	  have_status = 0;
	  have_data1 = 0;
	  have_data2 = 0;
	}
      } else if ((last_status_byte & 0xF0) == 0xe0) { // pitch bend

	if (!have_data1) {

	  last_data1_byte = midi_byte;
	  have_data1 = 1;
	  
	} else if (!have_data2) {

	  last_data2_byte = midi_byte;
	  have_data2 = 1;

	  // low 4 bits of status byte are midi channel
	  int channel = last_status_byte & 0x0F;
	  int msb = last_data2_byte;
	  int lsb = last_data1_byte;

	  // 2. Combine LSB and MSB into an unsigned 14-bit integer (0 .. 16383)
	  uint16_t raw_bend = ((uint16_t)(msb & 0x7F) << 7) | (lsb & 0x7F);

	  // 3. Center around 0 (-8192 .. +8191)
	  int16_t signed_bend = (int16_t)raw_bend - 8192;

	  // 4. Optional: Normalize to -1.0f .. +1.0f for DSP/synth pitch scaling
	  float normalized_bend = (signed_bend >= 0)
	    ? (signed_bend / 8191.0f)
	    : (signed_bend / 8192.0f);	  

	  // have complete pitch bend message.
	  snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
		   "din_midi: got pitch bend: channel %d, val %f\n", channel, normalized_bend);
	  Serial.print(din_midi_debug_message);
	  last_status_byte = 0;
	  last_data1_byte = 0;
	  last_data2_byte = 0;
	  have_status = 0;
	  have_data1 = 0;
	  have_data2 = 0;
	}
      }
    }
  }
  
  return midi_byte;
}

// write an outgoing midi byte
void din_midi_write(unsigned char data) {
  Serial2.write(data);
}
