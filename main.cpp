#include "config.h"

void bootAnimation() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "MadCat OS");
  u8g2.sendBuffer();
  delay(300);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_open);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "MadCat OS");
  u8g2.sendBuffer();
  delay(600);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_blink);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "MadCat OS");
  u8g2.sendBuffer();
  delay(150);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_open);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "MadCat OS");
  u8g2.sendBuffer();
  delay(400);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_blink);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "MadCat OS");
  u8g2.sendBuffer();
  delay(150);

  u8g2.clearBuffer();
  u8g2.drawXBM(32, 0, 64, 64, cat_open);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(35, 62, "MadCat OS");
  u8g2.drawFrame(20, 52, 88, 6);
  u8g2.sendBuffer();
  delay(200);

  for (int i = 0; i <= 84; i += 4) {
    u8g2.drawBox(22, 54, i, 2);
    u8g2.sendBuffer();
    delay(25);
  }

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(95, 62);
  u8g2.print(FIRMWARE_SHORT);
  u8g2.sendBuffer();
  delay(400);
}

void setup() {
  setupHardware();
  initMenuSystem();
  bootAnimation();
  drawMenu();
  Serial.println("MadCat OS v1.0 - Iniciado");
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
