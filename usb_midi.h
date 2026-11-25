#pragma once

#include <Arduino.h>
#include <math.h>

#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;
static uint64_t lastEventIndex = 0;

typedef void (*usb_midi_note_callback)(uint8_t channel, uint8_t note, uint8_t vel);
typedef void (*usb_midi_pitch_callback)(uint8_t channel, float value);

usb_midi_note_callback usb_midi_noteon_cbk = 0;
usb_midi_pitch_callback usb_midi_pitchbend_cbk = 0;

void usb_midi_set_noteon_callback(usb_midi_note_callback cbk) {
  usb_midi_noteon_cbk = cbk;
}

void usb_midi_set_pitchbend_callback(usb_midi_pitch_callback cbk) {
  usb_midi_pitchbend_cbk = cbk;
}

void usb_midi_process() {

  midiHandler.task();

  const auto& queue = midiHandler.getQueue();

  for (const auto& ev : queue) {
    // Each queued event should be processed only once
    if (ev.index <= lastEventIndex) {
      continue;
    }
    lastEventIndex = ev.index;

    uint8_t status = ev.statusCode & 0xF0;

    // Ignore all events except Note On and Note Off
    if (status != 0x90 && status != 0x80 && status != 0xb0 && status != 0xe0) {
      continue;
    }

    int channel = (ev.statusCode & 0x0F) + 1;
    
    int note = ev.noteNumber;
    int velocity = ev.velocity7;

    if (note < 0 || note > 127) {
      continue;
    }

    if (status == 0x90) { // note on
      //      if (velocity > 0) {
	//      char str[128];
      //      sprintf(str, "usb midi note on: chan %d, note %d, vel %d\n",
      //	      channel, note, velocity);
      //      Serial.print(str);
      if (usb_midi_noteon_cbk) usb_midi_noteon_cbk(channel, note, velocity);
    } else if (status == 0x80) { // note off
      //      char str[128];
      //      sprintf(str, "usb midi note off: chan %d, note %d\n",
      //	      channel, note);
      //      Serial.print(str);
      if (usb_midi_noteon_cbk) usb_midi_noteon_cbk(channel, note, 0);
    } else if (status == 0xF0) { // control change
    } else if (status == 0xe0) { // pitch bend
      // low 4 bits of status byte are midi channel
      int channel = ev.statusCode & 0x0F;
      int msb = velocity;
      int lsb = note;

      // 2. Combine LSB and MSB into an unsigned 14-bit integer (0 .. 16383)
      uint16_t raw_bend = ((uint16_t)(msb & 0x7F) << 7) | (lsb & 0x7F);

      // 3. Center around 0 (-8192 .. +8191)
      int16_t signed_bend = (int16_t)raw_bend - 8192;

      // 4. Optional: Normalize to -1.0f .. +1.0f for DSP/synth pitch scaling
      float normalized_bend = (signed_bend >= 0)
	? (signed_bend / 8191.0f)
	: (signed_bend / 8192.0f);	  

      // have complete pitch bend message.
      //	  snprintf(din_midi_debug_message, DIN_MIDI_DEBUG_MESSAGE_SIZE,
      //		   "din_midi: got pitch bend: channel %d, val %f\n", channel, normalized_bend);
      //	  Serial.print(din_midi_debug_message);

      if (usb_midi_pitchbend_cbk) {
	usb_midi_pitchbend_cbk(channel, normalized_bend);
      }
    }
  }
}

void usb_midi_loop() {
  usb_midi_process();
}

void usb_midi_init() {

  midiHandler.addTransport(&usbHost);
  usbHost.begin();
  midiHandler.begin();

  Serial.println("usb midi ready. Press a key on your MIDI keyboard.");
}
