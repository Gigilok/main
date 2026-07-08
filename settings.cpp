#include "config.h"

namespace {
  uint8_t brightness = 128;
  int settingsTab = 0;
}

void settingsSetup() {
  brightness = prefs.getUChar("brightness", 128);
  selectedSignalSlot = prefs.getInt("selectedSlot", 0);
  if (selectedSignalSlot < 0 || selectedSignalSlot >= MAX_SAVED_SIGNALS) {
    selectedSignalSlot = 0;
  }
  settingsTab = 0;
  u8g2.clearBuffer();
  drawFunctionHeader("Configuracoes");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("UP/DOWN: Ajustar");
  u8g2.setCursor(0, 40);
  u8g2.print("SEL: Mudar aba");
  u8g2.setCursor(0, 52);
  u8g2.print("BACK: Salvar e sair");
  u8g2.sendBuffer();
}

void settingsLoop() {
  if (buttonPressed(BTN_BACK)) {
    prefs.putUChar("brightness", brightness);
    prefs.putInt("selectedSlot", selectedSignalSlot);
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    settingsTab = (settingsTab + 1) % 2;
    delay(200);
  }
  if (settingsTab == 0) {
    if (buttonPressed(BTN_UP)) {
      brightness += 25;
      if (brightness > 255) brightness = 255;
      u8g2.setContrast(brightness);
    }
    if (buttonPressed(BTN_DOWN)) {
      if (brightness >= 25) brightness -= 25;
      else brightness = 0;
      u8g2.setContrast(brightness);
    }
  } else {
    if (buttonPressed(BTN_UP) && selectedSignalSlot < MAX_SAVED_SIGNALS - 1) {
      selectedSignalSlot++;
      delay(150);
    }
    if (buttonPressed(BTN_DOWN) && selectedSignalSlot > 0) {
      selectedSignalSlot--;
      delay(150);
    }
  }
  u8g2.clearBuffer();
  drawFunctionHeader("Configuracoes");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 24);
  u8g2.print(settingsTab == 0 ? "[Brilho]" : " Brilho ");
  u8g2.print(" ");
  u8g2.print(settingsTab == 1 ? "[Slot]" : " Slot ");
  if (settingsTab == 0) {
    u8g2.setCursor(0, 38);
    u8g2.print("Brilho: ");
    u8g2.print(brightness);
    u8g2.print("/255");
    u8g2.drawFrame(10, 46, 108, 8);
    u8g2.drawBox(12, 48, (brightness * 104) / 255, 4);
  } else {
    u8g2.setCursor(0, 38);
    u8g2.print("Slot Ativo: ");
    u8g2.print(selectedSignalSlot + 1);
    u8g2.print("/");
    u8g2.print(MAX_SAVED_SIGNALS);
    if (savedSignals[selectedSignalSlot].active) {
      u8g2.setCursor(0, 50);
      u8g2.print("Sinal: ");
      u8g2.print(savedSignals[selectedSignalSlot].name);
      u8g2.setCursor(0, 62);
      u8g2.print(savedSignals[selectedSignalSlot].signal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
    } else {
      u8g2.setCursor(0, 50);
      u8g2.print("Slot: Vazio");
      u8g2.setCursor(0, 62);
      u8g2.print("UP/DOWN: Mudar slot");
    }
  }
  u8g2.setCursor(0, 62);
  if (settingsTab == 0) u8g2.print("SEL:Ir p/ Slot");
  else u8g2.print("SEL:Ir p/ Brilho");
  u8g2.sendBuffer();
}
