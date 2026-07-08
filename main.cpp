#include "config.h"

void bootAnimation() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(30, 10, "nRF-BOX");
  u8g2.drawHLine(0, 32, 128);
  u8g2.sendBuffer();
  delay(200);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(15, 20, "nRF-BOX Pro");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(10, 35, "RF Hacking Tool");
  u8g2.drawHLine(0, 40, 128);
  u8g2.sendBuffer();
  delay(300);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(15, 20, "nRF-BOX Pro");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(15, 35, "CC1101 | nRF24 | BLE");
  u8g2.drawStr(25, 45, "WiFi | SourApple");
  u8g2.drawHLine(0, 52, 128);
  u8g2.sendBuffer();
  delay(400);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(15, 18, "nRF-BOX Pro");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(15, 32, "CC1101 | nRF24 | BLE");
  u8g2.drawStr(25, 42, "WiFi | SourApple");
  u8g2.drawFrame(20, 50, 88, 8);
  for (int i = 0; i <= 84; i += 4) {
    u8g2.drawBox(22, 52, i, 4);
    u8g2.sendBuffer();
    delay(30);
  }
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(90, 62);
  u8g2.print(FIRMWARE_VERSION);
  u8g2.sendBuffer();
  delay(500);
}

void setup() {
  setupHardware();
  initMenuSystem();
  bootAnimation();
  drawMenu();
  Serial.println("Setup OK - nRF-BOX Pro v2.3");
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
