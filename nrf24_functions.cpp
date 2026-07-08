#include "config.h"

namespace {
  uint8_t values[128];
  bool scanning = false;
  int displayOffset = 0;
}

void nrfScannerSetup() {
  u8g2.clearBuffer();
  drawFunctionHeader("nRF24 Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Iniciando...");
  u8g2.sendBuffer();
  radio.powerUp();
  radio.startListening();
  radio.stopListening();
  memset(values, 0, sizeof(values));
  scanning = true;
  displayOffset = 0;
}

void nrfScannerLoop() {
  if (buttonPressed(BTN_BACK)) {
    scanning = false;
    radio.powerDown();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (!scanning) return;
  for (int i = 0; i < 128; i++) {
    radio.setChannel(i);
    radio.startListening();
    delayMicroseconds(128);
    radio.stopListening();
    if (radio.testCarrier()) {
      values[i] = min((int)values[i] + 8, 63);
    } else {
      values[i] = max((int)values[i] - 1, 0);
    }
  }
  u8g2.clearBuffer();
  drawFunctionHeader("nRF24 Scanner");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 22);
  u8g2.print("2.4GHz Spectrum");
  int barWidth = 1;
  for (int i = 0; i < 64; i++) {
    int ch = displayOffset + i;
    if (ch >= 128) break;
    int h = values[ch] / 2;
    if (h > 0) {
      u8g2.drawVLine(i * 2, 54 - h, h);
    }
  }
  u8g2.drawHLine(0, 54, 128);
  u8g2.setCursor(0, 62);
  u8g2.print(displayOffset);
  u8g2.setCursor(55, 62);
  u8g2.print(displayOffset + 32);
  u8g2.setCursor(110, 62);
  u8g2.print(displayOffset + 63);
  u8g2.sendBuffer();
  if (buttonPressed(BTN_UP) && displayOffset > 0) {
    displayOffset -= 16;
    if (displayOffset < 0) displayOffset = 0;
  }
  if (buttonPressed(BTN_DOWN) && displayOffset < 64) {
    displayOffset += 16;
    if (displayOffset > 64) displayOffset = 64;
  }
}

namespace {
  bool jamming = false;
  const byte jamText[32] = {
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
}

void nrfJammerSetup() {
  jamming = false;
  u8g2.clearBuffer();
  drawFunctionHeader("nRF24 Jammer");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("SEL: Iniciar/Parar");
  u8g2.setCursor(0, 42);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
  radio.powerUp();
  radio.stopListening();
  radio.setAutoAck(false);
}

void nrfJammerLoop() {
  if (buttonPressed(BTN_BACK)) {
    jamming = false;
    radio.powerDown();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    jamming = !jamming;
    delay(200);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("nRF24 Jammer");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print(jamming ? "Status: ATIVO" : "Status: PARADO");
  u8g2.setCursor(0, 42);
  u8g2.print("Canais: 0-125");
  u8g2.setCursor(0, 56);
  u8g2.print("BACK: Parar e sair");
  u8g2.sendBuffer();
  if (jamming) {
    for (int i = 0; i < 126; i += 7) {
      radio.setChannel(i);
      radio.writeFast(jamText, 32);
    }
    radio.txStandBy();
  }
}
