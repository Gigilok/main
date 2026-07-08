#include "config.h"

namespace {
  uint8_t brightness = 128;
}

void settingsSetup() {
  brightness = prefs.getUChar("brightness", 128);
  selectedSignalSlot = prefs.getInt("selectedSlot", 0);
  if (selectedSignalSlot < 0 || selectedSignalSlot >= MAX_SAVED_SIGNALS) {
    selectedSignalSlot = 0;
  }
  u8g2.clearBuffer();
  drawFunctionHeader("Configuracoes");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("UP: + Brilho");
  u8g2.setCursor(0, 40);
  u8g2.print("DOWN: - Brilho");
  u8g2.setCursor(0, 52);
  u8g2.print("SEL: Salvar");
  u8g2.setCursor(0, 62);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
}

void settingsLoop() {
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_UP) && brightness < 255) {
    brightness += 25;
    if (brightness > 255) brightness = 255;
    u8g2.setContrast(brightness);
  }
  if (buttonPressed(BTN_DOWN) && brightness > 0) {
    brightness -= 25;
    if (brightness > 255) brightness = 0;
    u8g2.setContrast(brightness);
  }
  if (buttonPressed(BTN_SELECT)) {
    prefs.putUChar("brightness", brightness);
    prefs.putInt("selectedSlot", selectedSignalSlot);
    u8g2.clearBuffer();
    drawFunctionHeader("Configuracoes");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(30, 35);
    u8g2.print("Salvo!");
    u8g2.sendBuffer();
    delay(500);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("Configuracoes");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Brilho: ");
  u8g2.print(brightness);
  u8g2.setCursor(0, 40);
  u8g2.print("Slot Ativo: ");
  u8g2.print(selectedSignalSlot + 1);
  u8g2.print("/");
  u8g2.print(MAX_SAVED_SIGNALS);
  if (savedSignals[selectedSignalSlot].active) {
    u8g2.setCursor(0, 52);
    u8g2.print("Sinal: ");
    u8g2.print(savedSignals[selectedSignalSlot].name);
    u8g2.setCursor(0, 62);
    u8g2.print(savedSignals[selectedSignalSlot].signal.frequency / 1000.0, 3);
    u8g2.print(" MHz");
  } else {
    u8g2.setCursor(0, 52);
    u8g2.print("Slot: Vazio");
    u8g2.setCursor(0, 62);
    u8g2.print("UP/DOWN: Mudar slot");
  }
  u8g2.sendBuffer();
}
