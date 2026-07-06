#include "config.h"

namespace {
  uint8_t brightness = 128;
  bool neopixelActive = false;
}

void settingsSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Configuracoes");
  u8g2.drawStr(0, 25, "UP: + Brilho");
  u8g2.drawStr(0, 35, "DOWN: - Brilho");
  u8g2.drawStr(0, 45, "SEL: Salvar");
  u8g2.sendBuffer();
}

void settingsLoop() {
  if (buttonPressed(BTN_UP) && brightness < 255) {
    brightness += 25;
    u8g2.setContrast(brightness);
  }
  
  if (buttonPressed(BTN_DOWN) && brightness > 25) {
    brightness -= 25;
    u8g2.setContrast(brightness);
  }
  
  if (buttonPressed(BTN_SELECT)) {
    EEPROM.write(0, brightness);
    EEPROM.commit();
    u8g2.clearBuffer();
    u8g2.drawStr(0, 30, "Salvo!");
    u8g2.sendBuffer();
    delay(500);
  }
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Configuracoes");
  u8g2.setCursor(0, 30);
  u8g2.print("Brilho: ");
  u8g2.print(brightness);
  u8g2.drawStr(0, 50, "BACK: Menu");
  u8g2.sendBuffer();
}
