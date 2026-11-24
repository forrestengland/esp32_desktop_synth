#include <Arduino.h>
#include <math.h>
#include "din_midi.h"

void setup() {

  Serial.begin(115200);

  Serial.println("ESP32-S3 MIDI test");

	din_midi_init();

	Serial.println("DIN MIDI input ready");
}

void loop() {

	while (din_midi_available()) {

		unsigned char data = din_midi_read();

		Serial.print(data);
		Serial.print(" ");

		din_midi_write(data);
	}
}
