#include "config.h"

const SubGHzEntry subGHzDatabase[] = {
  {"Peccinin 433", 433.92, 0, 0x123456, 24},
  {"PPA 433", 433.92, 0, 0x654321, 24},
  {"Garen 433", 433.92, 2, 0x111111, 24},
  {"RCG 433", 433.92, 0, 0x222222, 24},
  {"Pado 433", 433.92, 2, 0x333333, 24},
  {"Seg 433", 433.92, 0, 0x444444, 24},
  {"ECP 433", 433.92, 2, 0x555555, 24},
  {"Intelbras 433", 433.92, 0, 0x666666, 24},
  {"Omega 433", 433.92, 2, 0x777777, 24},
  {"Stilus 433", 433.92, 0, 0x888888, 24},
  {"Rossi 433", 433.92, 2, 0x999999, 24},
  {"UniMotor 433", 433.92, 0, 0xAAAAAA, 24},
  {"Peccinin 315", 315.00, 0, 0x123456, 24},
  {"PPA 315", 315.00, 0, 0x654321, 24},
  {"Garen 315", 315.00, 2, 0x111111, 24},
  {"RCG 315", 315.00, 0, 0x222222, 24},
  {"Pado 315", 315.00, 2, 0x333333, 24},
  {"Seg 315", 315.00, 0, 0x444444, 24},
  {"ECP 315", 315.00, 2, 0x555555, 24},
  {"Intelbras 315", 315.00, 0, 0x666666, 24},
  {"Omega 315", 315.00, 2, 0x777777, 24},
  {"Stilus 315", 315.00, 0, 0x888888, 24},
  {"Rossi 315", 315.00, 2, 0x999999, 24},
  {"UniMotor 315", 315.00, 0, 0xAAAAAA, 24},
};

const int SUBGHZ_DB_COUNT = sizeof(subGHzDatabase) / sizeof(subGHzDatabase[0]);

namespace {
  int dbSelected = 0;
  bool dbTransmitting = false;
  unsigned long dbTxTimer = 0;
}

void subGHzDBSetup() {
  dbSelected = 0;
  dbTransmitting = false;
  u8g2.clearBuffer();
  drawFunctionHeader("Sub-GHz DB");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Banco de dados");
  u8g2.setCursor(0, 40);
  u8g2.print("Marcas: ");
  u8g2.print(SUBGHZ_DB_COUNT);
  u8g2.sendBuffer();
  delay(500);
}

void subGHzDBLoop() {
  if (buttonPressed(BTN_BACK)) {
    dbTransmitting = false;
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  
  if (!dbTransmitting) {
    if (buttonPressed(BTN_SELECT)) {
      dbTransmitting = true;
      dbTxTimer = millis();
      delay(200);
    }
    if (buttonPressed(BTN_UP) && dbSelected > 0) dbSelected--;
    if (buttonPressed(BTN_DOWN) && dbSelected < SUBGHZ_DB_COUNT - 1) dbSelected++;
    
    u8g2.clearBuffer();
    drawFunctionHeader("Sub-GHz DB");
    u8g2.setFont(u8g2_font_6x10_tr);
    int start = (dbSelected / 4) * 4;
    for (int i = 0; i < 4 && (start + i) < SUBGHZ_DB_COUNT; i++) {
      int idx = start + i;
      int y = 24 + (i * 10);
      u8g2.setCursor(0, y);
      if (idx == dbSelected) u8g2.print("> ");
      else u8g2.print("  ");
      u8g2.print(subGHzDatabase[idx].name);
    }
    u8g2.setCursor(0, 62);
    u8g2.print("SEL:TX  B:Menu");
    u8g2.sendBuffer();
  } else {
    const SubGHzEntry& entry = subGHzDatabase[dbSelected];
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(entry.frequency);
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setPA(12);
    ELECHOUSE_cc1101.SetTx();
    delay(5);
    pinMode(CC_GDO0, OUTPUT);
    
    const RCSwitchProtocol& p = protocols[entry.protocolIndex];
    transmitCode(entry.code, entry.bits, p);
    delayMicroseconds(10000);
    digitalWrite(CC_GDO0, LOW);
    ELECHOUSE_cc1101.setSidle();
    
    u8g2.clearBuffer();
    drawFunctionHeader("Sub-GHz TX");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("TX: ");
    u8g2.print(entry.name);
    u8g2.setCursor(0, 40);
    u8g2.print("Freq: ");
    u8g2.print(entry.frequency, 2);
    u8g2.print(" MHz");
    u8g2.setCursor(0, 52);
    u8g2.print("Code: 0x");
    u8g2.print(entry.code, HEX);
    u8g2.setCursor(0, 62);
    u8g2.print("SEL:Parar B:Menu");
    u8g2.sendBuffer();
    
    if (buttonPressed(BTN_SELECT) || millis() - dbTxTimer > 5000) {
      dbTransmitting = false;
      delay(200);
    }
    delay(200);
  }
}

