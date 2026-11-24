#include <Arduino.h>
#include <math.h>
#include "din_midi.h"
#include "audio.h"
#include "pots.h"
#include "oled.h"

void setup() {

  Serial.begin(115200);

  Serial.println("ESP32-S3 MIDI test");

	din_midi_init();

	Serial.println("DIN MIDI input ready");

	audio_start();

	pots_init();

	oled_init();
}

void loop() {

	while (din_midi_available()) {

		unsigned char data = din_midi_read();

		Serial.print(data);
		Serial.print(" ");

		din_midi_write(data);
	}

	pots_loop();

	oled_loop();
}
