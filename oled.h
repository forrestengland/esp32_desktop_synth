#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define I2C_SDA 11
#define I2C_SCL 12
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(128, 64, &Wire, -1, 400000UL, 400000UL);

void scanI2C() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("  nothing found, check SDA/SCL wiring and pull-ups");
}

void oled_init() {

  //  Wire.begin(I2C_SDA, I2C_SCL);
  //  Wire.setClock(400000);
  //  scanI2C();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED init FAILED");
    while (true) delay(1000);
  }
  Serial.println("OLED init OK");

  // Static test: border, corner pixels, text
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.print("OLED TEST");
  display.setCursor(10, 22);
  display.print("128x64 @ 0x3C");
  display.display();
  delay(2000);
}

void oled_loop() {

  static uint32_t frames = 0;
  static uint32_t lastFpsMs = 0;
  static int x = 0, dx = 2;
  static uint32_t counter = 0;

  display.clearDisplay();

  for (x=0; x<128; x++) {
    int y = dispsamples[x] * 32 + 32;
    display.drawPixel(x, y, SSD1306_WHITE);
  }

  display.display();
  frames++;
}
