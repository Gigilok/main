#include "config.h"
#include "icons.h"

#define MENU_ITEMS 10

const char* menu_names[MENU_ITEMS] = {
  "nRF24 Scanner",
  "nRF24 Jammer",
  "CC1101 Scanner",
  "CC1101 Capture",
  "CC1101 BruteForce",
  "CC1101 FreqSweep",
  "BLE Scan",
  "WiFi Scan",
  "Sour Apple",
  "Configuracoes"
};

void (*menu_setup_funcs[MENU_ITEMS])();
void (*menu_loop_funcs[MENU_ITEMS])();

void checkBackButton() {
  if (buttonPressed(BTN_BACK)) {
    back_pressed = true;
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    resetCC1101State();
    drawMenu();
    delay(200);
  }
}

void initMenuSystem() {
  menu_setup_funcs[0] = nrfScannerSetup;  menu_loop_funcs[0] = nrfScannerLoop;
  menu_setup_funcs[1] = nrfJammerSetup;   menu_loop_funcs[1] = nrfJammerLoop;
  menu_setup_funcs[2] = cc1101ScannerSetup; menu_loop_funcs[2] = cc1101ScannerLoop;
  menu_setup_funcs[3] = cc1101TransceiverSetup; menu_loop_funcs[3] = cc1101TransceiverLoop;
  menu_setup_funcs[4] = cc1101BruteForceSetup; menu_loop_funcs[4] = cc1101BruteForceLoop;
  menu_setup_funcs[5] = cc1101FreqSweepSetup; menu_loop_funcs[5] = cc1101FreqSweepLoop;
  menu_setup_funcs[6] = bleScanSetup;     menu_loop_funcs[6] = bleScanLoop;
  menu_setup_funcs[7] = wifiDeauthSetup;  menu_loop_funcs[7] = wifiDeauthLoop;
  menu_setup_funcs[8] = sourAppleSetup;    menu_loop_funcs[8] = sourAppleLoop;
  menu_setup_funcs[9] = settingsSetup;     menu_loop_funcs[9] = settingsLoop;
}

void drawMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(25, 10, "nRF-BOX Pro");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(95, 10);
  u8g2.print(FIRMWARE_VERSION);
  u8g2.drawHLine(0, 12, 128);
  
  int start = (current_menu_item / 4) * 4;
  for (int i = 0; i < 4; i++) {
    int idx = start + i;
    if (idx >= MENU_ITEMS) break;
    int y = 25 + (i * 12);
    if (idx < 8) {
      u8g2.drawXBMP(2, y - 8, 16, 16, menu_icons[idx]);
    }
    if (idx == current_menu_item) {
      u8g2.drawBox(20, y - 9, 108, 11);
      u8g2.setDrawColor(0);
    }
    u8g2.setCursor(23, y);
    u8g2.print(menu_names[idx]);
    u8g2.setDrawColor(1);
  }
  
  int total_pages = (MENU_ITEMS + 3) / 4;
  int current_page = current_menu_item / 4;
  u8g2.setCursor(105, 62);
  u8g2.print(current_page + 1);
  u8g2.print("/");
  u8g2.print(total_pages);
  
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 63);
  u8g2.print("SEL:Entrar B:Voltar");
  u8g2.sendBuffer();
}

void handleMenu() {
  checkBackButton();
  
  if (buttonPressed(BTN_UP)) {
    if (current_menu_item > 0) {
      current_menu_item--;
      drawMenu();
      delay(80);
    }
  }
  
  if (buttonPressed(BTN_DOWN)) {
    if (current_menu_item < MENU_ITEMS - 1) {
      current_menu_item++;
      drawMenu();
      delay(80);
    }
  }
  
  if (buttonPressed(BTN_SELECT)) {
    current_screen = 1;
    back_pressed = false;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(10, 30, "Carregando...");
    u8g2.sendBuffer();
    if (menu_setup_funcs[current_menu_item]) {
      menu_setup_funcs[current_menu_item]();
    }
  }
}

void runCurrentFunction() {
  checkBackButton();
  if (current_screen == 0) return;
  if (menu_loop_funcs[current_menu_item]) {
    menu_loop_funcs[current_menu_item]();
  }
}
