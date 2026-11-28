#include <Arduino.h>
#include <math.h>

#include "din_midi.h"
#include "audio.h"
#include "pots.h"
#include "oled.h"
#include "encoders.h"
#include "buttons.h"
#include "usb_midi.h"

void on_enc_turn(int enc, int pos) {
	Serial.printf("encoder turned: %d, %d\n", enc, pos);
	if (oled_current_screen() == screens_menu) {
		int item = oled_selected_menu() + pos;
		if (item < 0) item = 0;
		if (item >= MENU_ITEM_COUNT) {
			item = MENU_ITEM_COUNT - 1;
		}
		oled_set_selected_menu(item);
	}
}

void on_enc_btn(int enc, int state) {
	Serial.printf("encoder btn: %d, %d\n", enc, state);
	if (enc == 0 && state == 1) {
		if (oled_current_screen() != screens_menu) {
			oled_set_current_screen(screens_menu);
		} else {
			if (menu_selected == menu_item_scope) {
				oled_set_current_screen(screens_scope);
			}
		}
	}
}

void on_din_noteon(uint8_t channel, uint8_t note, uint8_t vel) {
	audio_set_note(note, vel);
}

void on_din_pitchbend(uint8_t channel, float val) {
	audio_set_pitchbend(val);
}

void on_usb_noteon(uint8_t channel, uint8_t note, uint8_t vel) {
	Serial.printf("got usb note on %d %d\n", note, vel);
	audio_set_note(note, vel);
}

void on_usb_pitchbend(uint8_t channel, float val) {
	//	Serial.printf("got pitchbend: %f\n", val);
 	audio_set_pitchbend(val);
}

void on_usb_cc(uint8_t channel, uint8_t cc, uint8_t val) {
	Serial.printf("got usb midi cc %d %d\n", cc, val);
	if (cc == 1) {
		audio_set_lfo_pitch_depth((float)val / 127.0f);
	} else if (cc == 12) {
		audio_set_lfo_freq((float)val / 127.0f * 100.0f);
	} else if (cc == 13) {
		audio_set_lfo_filter_depth((float)val / 127.0f);
	} else if (cc == 14) {
		audio_set_env_filter_depth((float)val / 127.0f);
	} else if (cc == 7) {
		audio_set_amp((float)val / 127.0f);
	} else if (cc == 15) {
		audio_set_portamento((float)val / 127.0f);
	}
}

void on_button(int button, int state) {
	Serial.printf("button %d: %d\n", button, state);
	//	audio_set_wavetable(state);
}

void on_pots(int pot, float value) {
	if (pot == 0) {
		audio_set_attack(1.0 - value);
	} else if (pot == 1) {
		audio_set_release(1.0 - value);
	} else if (pot == 2) {
		audio_set_cutoff((1.0 - value) * 10000.0f);
	} else if (pot == 3) {
		audio_set_resonance(1.0 - value);
	}
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

	usb_midi_set_noteon_callback(on_usb_noteon);
	usb_midi_set_pitchbend_callback(on_usb_pitchbend);
	usb_midi_set_control_callback(on_usb_cc);

	pots_set_callback(on_pots);
	buttons_set_callback(on_button);
	encoders_set_rotation_callback(on_enc_turn);
	encoders_set_button_callback(on_enc_btn);

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
