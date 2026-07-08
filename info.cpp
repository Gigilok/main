#include "config.h"

namespace {
  int infoPage = 0;
}

void infoSetup() {
  infoPage = 0;
  u8g2.clearBuffer();
  drawFunctionHeader("Info/Sobre");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("MadCat OS v2.0");
  u8g2.setCursor(0, 40);
  u8g2.print("RF Hacking Tool");
  u8g2.setCursor(0, 52);
  u8g2.print("ESP32 + CC1101 + nRF24");
  u8g2.sendBuffer();
  delay(500);
}

void infoLoop() {
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    infoPage = (infoPage + 1) % 4;
    delay(200);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("Info/Sobre");
  u8g2.setFont(u8g2_font_6x10_tr);
  if (infoPage == 0) {
    u8g2.setCursor(0, 24);
    u8g2.print("MadCat OS v2.0");
    u8g2.setCursor(0, 36);
    u8g2.print("RF Hacking Tool");
    u8g2.setCursor(0, 48);
    u8g2.print("ESP32 + CC1101");
    u8g2.setCursor(0, 60);
    u8g2.print("+ nRF24 + BLE + WiFi");
  } else if (infoPage == 1) {
    u8g2.setCursor(0, 24);
    u8g2.print("Hardware Info:");
    u8g2.setCursor(0, 36);
    u8g2.print("CPU: ESP32 240MHz");
    u8g2.setCursor(0, 48);
    u8g2.print("Flash: 4MB");
    u8g2.setCursor(0, 60);
    u8g2.print("RAM: 520KB");
  } else if (infoPage == 2) {
    u8g2.setCursor(0, 24);
    u8g2.print("Funcoes:");
    u8g2.setCursor(0, 36);
    u8g2.print("RF: 300-928 MHz");
    u8g2.setCursor(0, 48);
    u8g2.print("2.4GHz: nRF24/BLE");
    u8g2.setCursor(0, 60);
    u8g2.print("WiFi 2.4GHz Scan/Deauth");
  } else {
    u8g2.setCursor(0, 24);
    u8g2.print("Novidades v2.0:");
    u8g2.setCursor(0, 36);
    u8g2.print("RollJam Real");
    u8g2.setCursor(0, 48);
    u8g2.print("Rolling-PWN + Deauth");
    u8g2.setCursor(0, 60);
    u8g2.print("Sub-GHz DB + Smart BF");
  }
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(90, 62);
  u8g2.print(infoPage + 1);
  u8g2.print("/4");
  u8g2.sendBuffer();
}
