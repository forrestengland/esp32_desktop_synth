#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define I2C_SDA 11
#define I2C_SCL 12
#define OLED_ADDRESS 0x3C

// ----------- screens ------------

enum oled_screens {
  screens_menu,  
  screens_scope
};

oled_screens screen = screens_scope;

// ------------ menu -------------

enum menu_items {
  menu_item_scope,
  menu_item_synth
};

typedef struct {
  char * label;
} oled_menu_item;

int menu_selected = 0;
#define MENU_ITEM_COUNT 2
oled_menu_item menu_items[MENU_ITEM_COUNT] = {{"Scope"}, {"Synth"}};

Adafruit_SSD1306 display(128, 64, &Wire, -1, 400000UL, 400000UL);

int oled_selected_menu() {
  return menu_selected;
}

void oled_set_selected_menu(int item) {
  menu_selected = item;
}

oled_screens oled_current_screen() {
  return screen;
}

void oled_set_current_screen(oled_screens newscr) {
  screen = newscr;
}

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

  display.clearDisplay();

  if (screen == screens_scope) {

    for (int x=0; x<128; x++) {
      int y = dispsamples[x] * 32 + 32;
      display.drawPixel(x, y, SSD1306_WHITE);
    }
    
  } else if (screen == screens_menu) {

    for (int m=0; m<MENU_ITEM_COUNT; m++) {
      if (m == menu_selected) {
	display.fillRect(0, m * 8, 128, 8, SSD1306_WHITE);
	display.setTextColor(SSD1306_BLACK);
      } else {
	display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(0, m * 8);
      display.print(menu_items[m].label);
    }
  }

  display.display();
}
