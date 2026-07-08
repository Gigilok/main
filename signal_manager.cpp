#include "config.h"

namespace {
  int sigSelected = 0;
  int sigAction = 0;
}

void signalManagerSetup() {
  sigSelected = 0;
  sigAction = 0;
  u8g2.clearBuffer();
  drawFunctionHeader("Gerenciar Sinais");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("UP/DOWN: Navegar");
  u8g2.setCursor(0, 40);
  u8g2.print("SEL: Acao");
  u8g2.setCursor(0, 52);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
}

void signalManagerLoop() {
  if (buttonPressed(BTN_BACK)) {
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_UP) && sigSelected > 0) {
    sigSelected--;
    delay(150);
  }
  if (buttonPressed(BTN_DOWN) && sigSelected < MAX_SAVED_SIGNALS - 1) {
    sigSelected++;
    delay(150);
  }
  if (buttonPressed(BTN_SELECT)) {
    if (savedSignals[sigSelected].active) {
      sigAction = (sigAction + 1) % 3;
      if (sigAction == 1) {
        currentSignal = savedSignals[sigSelected].signal;
        selectedSignalSlot = sigSelected;
        hasSavedSignal = true;
        u8g2.clearBuffer();
        drawFunctionHeader("Gerenciar Sinais");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(10, 35);
        u8g2.print("Sinal carregado!");
        u8g2.sendBuffer();
        delay(800);
      } else if (sigAction == 2) {
        clearSignalSlot(sigSelected);
        u8g2.clearBuffer();
        drawFunctionHeader("Gerenciar Sinais");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(10, 35);
        u8g2.print("Sinal apagado!");
        u8g2.sendBuffer();
        delay(800);
      }
    } else {
      u8g2.clearBuffer();
      drawFunctionHeader("Gerenciar Sinais");
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(10, 35);
      u8g2.print("Slot vazio!");
      u8g2.sendBuffer();
      delay(800);
    }
    sigAction = 0;
  }
  u8g2.clearBuffer();
  drawFunctionHeader("Gerenciar Sinais");
  u8g2.setFont(u8g2_font_6x10_tr);
  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    int y = 24 + (i * 10);
    u8g2.setCursor(0, y);
    if (i == sigSelected) u8g2.print("> ");
    else u8g2.print("  ");
    u8g2.print(i + 1);
    u8g2.print(": ");
    if (savedSignals[i].active) {
      u8g2.print(savedSignals[i].name);
      u8g2.print(" ");
      u8g2.print(savedSignals[i].signal.frequency / 1000.0, 0);
      u8g2.print("MHz");
    } else {
      u8g2.print("[Vazio]");
    }
  }
  u8g2.setCursor(0, 62);
  if (savedSignals[sigSelected].active) {
    u8g2.print("SEL: ");
    if (sigAction == 0) u8g2.print("Ver");
    else if (sigAction == 1) u8g2.print("Carregar");
    else u8g2.print("Apagar");
  } else {
    u8g2.print("Slot vazio");
  }
  u8g2.sendBuffer();
}

