#pragma once

#include <Arduino.h>
#include <math.h>

#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;
static uint64_t lastEventIndex = 0;

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
    if (status != 0x90 && status != 0x80) {
      continue;
    }

    int note = ev.noteNumber;
    int velocity = ev.velocity7;

    if (note < 0 || note > 127) {
      continue;
    }

    int channel = (ev.statusCode & 0x0F) + 1;

    if (status == 0x90 && velocity > 0) {
      char str[128];
      sprintf(str, "usb midi note on: chan %d, note %d, vel %d\n",
	      channel, note, velocity);
      Serial.print(str);
    } else {
      char str[128];
      sprintf(str, "usb midi note off: chan %d, note %d\n",
	      channel, note);
      Serial.print(str);
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
