#include <Arduino.h>
#include <math.h>

#include "din_midi.h"
#include "audio.h"
#include "pots.h"
#include "oled.h"
#include "encoders.h"
#include "buttons.h"
#include "usb_midi.h"

void on_din_noteon(uint8_t channel, uint8_t note, uint8_t vel) {
	audio_set_note(note, vel);
}

void on_din_pitchbend(uint8_t channel, float val) {
	audio_set_pitchbend(val);
}

void on_usb_noteon(uint8_t channel, uint8_t note, uint8_t vel) {
	audio_set_note(note, vel);
}

void on_usb_pitchbend(uint8_t channel, float val) {
	audio_set_pitchbend(val);
}

void on_button(int button, int state) {
	audio_set_wavetable(state);
}

void setup() {

  Serial.begin(115200);

  Serial.println("ESP32-S3 MIDI test");

	din_midi_init();

	Serial.println("DIN MIDI input ready");

	audio_start();

	pots_init();

	oled_init();

	encoders_init();

	buttons_init();

	usb_midi_init();

	din_midi_set_noteon_callback(on_din_noteon);
	din_midi_set_pitchbend_callback(on_din_pitchbend);
	buttons_set_callback(on_button);

	usb_midi_set_noteon_callback(on_usb_noteon);
	usb_midi_set_pitchbend_callback(on_usb_pitchbend);

	audio_set_attack(0.3f);
	audio_set_release(0.7f);
}

void loop() {

	while (din_midi_available()) {

		unsigned char data = din_midi_read();

		din_midi_write(data);
	}

	pots_loop();

	oled_loop();

	encoders_loop();

	buttons_loop();

	usb_midi_loop();
}
