#include "config.h"

void setup() {
  setupHardware();
  initMenuSystem();
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(15, 20, "nRF-BOX Pro");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(10, 35, "RF Hacking Tool v2.2");
  u8g2.drawStr(15, 50, "CC1101 | nRF24 | BLE");
  u8g2.sendBuffer();
  delay(1500);
  
  drawMenu();
  Serial.println("Setup OK");
}

void loop() {
  checkBackInterruptFlag();
  
  if (current_screen == 0) {
    handleMenu();
  } else {
    runCurrentFunction();
  }
  
  delay(10);
}
