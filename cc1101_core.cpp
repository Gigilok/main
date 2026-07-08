#include "config.h"

// ============================================
// FREQUENCIAS E PROTOCOLOS
// ============================================

const uint32_t SCAN_FREQUENCIES[] = {315000, 433920, 868350, 915000};
const int NUM_FREQS = 4;

const uint32_t BRUTE_FREQS[] = {
  300000, 303000, 304000, 305000, 310000, 315000, 318000,
  330000, 390000, 418000, 433000, 433920, 434000, 868000, 868350, 915000
};
const int NUM_BRUTE_FREQS = sizeof(BRUTE_FREQS) / sizeof(BRUTE_FREQS[0]);

const RCSwitchProtocol protocols[] = {
  {"PT2262",   350, {1,31}, {1,3}, {3,1}, false},
  {"PT2260",   350, {1,31}, {1,3}, {3,1}, false},
  {"EV1527",   350, {1,31}, {1,3}, {3,1}, false},
  {"Kaku",     650, {1,10}, {1,2}, {2,1}, false},
  {"HT12E",    450, {1,23}, {1,3}, {3,1}, false},
  {"V2",       650, {1,10}, {1,2}, {2,1}, false},
  {"Nice",     700, {1,20}, {1,2}, {2,1}, false},
};
const int NUM_PROTOCOLS = sizeof(protocols) / sizeof(protocols[0]);

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
  0xF00000, 0x555555, 0xCCCCCC, 0x333333, 0x0F0F0F, 0xF0F0F0,
  0xFF00FF, 0x00FF00, 0xFFFF00, 0x00FFFF, 0x123123, 0x321321,
  0x456456, 0x654654, 0x789789, 0x987987, 0x135135, 0x246246,
  0x357357, 0x468468, 0x579579, 0x680680, 0x791791, 0x802802,
  0x913913, 0x024024, 0x135246, 0x246135, 0x357468, 0x468357,
  0x579680, 0x680579, 0x791802, 0x802791, 0x913024, 0x024913
};
const int NUM_COMMON_CODES = sizeof(COMMON_CODES) / sizeof(COMMON_CODES[0]);

// ============================================
// VARIAVEIS GLOBAIS DO CC1101
// ============================================

namespace {
  CCState ccState = CC_IDLE;
  volatile uint32_t lastInterruptTime = 0;
  volatile uint16_t captureIndex = 0;
  volatile bool capturing = false;
  volatile uint16_t tempBuffer[MAX_RAW_DATA];
  volatile uint8_t tempLevels[MAX_RAW_DATA];
  bool txPlaying = false;
  int txProgress = 0;
  uint32_t captureStartTime = 0;
  uint32_t detectedFrequency = 0;
}

// Variaveis dos ataques (acessadas por cc1101_attacks.cpp)
namespace cc1101 {
  bool rolljamActive = false;
  uint32_t rolljamCode1 = 0;
  uint32_t rolljamCode2 = 0;
  bool rolljamHasCode1 = false;
  bool rolljamHasCode2 = false;
  int rolljamStep = 0;
  unsigned long rolljamTimer = 0;

  bool rollingPwnActive = false;
  int rollingPwnStep = 0;
  int rollingPwnCounter = 0;
  unsigned long rollingPwnTimer = 0;
  uint32_t capturedCodes[10];
  int capturedCodeCount = 0;

  BFMode bfMode = BF_MODE_COMMON;
  BFState bfState = BF_IDLE;
  int bfFreqIndex = 0;
  int bfProtocolIndex = 0;
  uint32_t bfCode = 0;
  unsigned long bfStartTime = 0;
  int bfCodesSent = 0;
}

// ============================================
// FUNCOES BASICAS DO CC1101
// ============================================

void resetCC1101State() {
  ccState = CC_IDLE;
  txPlaying = false;
  capturing = false;
  captureIndex = 0;
  cc1101::rolljamActive = false;
  cc1101::rollingPwnActive = false;
  ELECHOUSE_cc1101.setSidle();
}

void drawFunctionHeader(const char* title) {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 7);
  u8g2.print("nRF-BOX Pro");
  u8g2.setCursor(95, 7);
  u8g2.print(FIRMWARE_VERSION);
  u8g2.drawHLine(0, 9, 128);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 20);
  u8g2.print(title);
}

uint32_t autoDetectFrequency() {
  int bestRssi = -1000;
  uint32_t bestFreq = 0;
  for (int i = 0; i < NUM_FREQS; i++) {
    uint32_t freq = SCAN_FREQUENCIES[i];
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
    ELECHOUSE_cc1101.SetRx();
    delay(80);
    int maxRssi = -1000;
    for (int j = 0; j < 30; j++) {
      byte rssiRaw = ELECHOUSE_cc1101.getRssi();
      int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
      if (rssi > maxRssi) maxRssi = rssi;
      delay(8);
    }
    if (maxRssi > bestRssi) {
      bestRssi = maxRssi;
      bestFreq = freq;
    }
    u8g2.setCursor(0, 40);
    u8g2.print("Testando ");
    u8g2.print(freq / 1000.0, 3);
    u8g2.print("MHz");
    u8g2.setCursor(0, 52);
    u8g2.print("RSSI: ");
    u8g2.print(maxRssi);
    u8g2.print(" dBm");
    u8g2.sendBuffer();
    delay(100);
  }
  return (bestRssi > RSSI_THRESHOLD) ? bestFreq : 0;
}

void IRAM_ATTR onGDO0Interrupt() {
  if (!capturing || captureIndex >= MAX_RAW_DATA) return;
  uint32_t now = micros();
  uint8_t level = digitalRead(CC_GDO0);
  if (captureIndex > 0) {
    tempBuffer[captureIndex - 1] = (uint16_t)(now - lastInterruptTime);
    tempLevels[captureIndex - 1] = level;
  }
  lastInterruptTime = now;
  captureIndex++;
}

void startCapture(uint32_t frequency) {
  memset((void*)tempBuffer, 0, sizeof(tempBuffer));
  memset((void*)tempLevels, 0, sizeof(tempLevels));
  captureIndex = 0;
  capturing = true;
  captureStartTime = millis();
  detectedFrequency = frequency;
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(frequency / 1000.0);
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.SetRx();
  delay(15);
  pinMode(CC_GDO0, INPUT);
  attachInterrupt(digitalPinToInterrupt(CC_GDO0), onGDO0Interrupt, CHANGE);
  lastInterruptTime = micros();
}

void stopCapture() {
  detachInterrupt(digitalPinToInterrupt(CC_GDO0));
  capturing = false;
  if (captureIndex > 10) {
    currentSignal.frequency = detectedFrequency;
    currentSignal.dataLength = min((int)captureIndex, MAX_RAW_DATA);
    currentSignal.modulation = 0;
    currentSignal.timestamp = millis();
    for (int i = 0; i < currentSignal.dataLength; i++) {
      currentSignal.timings[i] = tempBuffer[i];
      currentSignal.levels[i] = tempLevels[i];
    }
    byte rssiRaw = ELECHOUSE_cc1101.getRssi();
    currentSignal.rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
  }
}

void transmitRawSignal() {
  if (!hasSavedSignal || currentSignal.dataLength == 0) return;
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(currentSignal.frequency / 1000.0);
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.setPA(12);
  ELECHOUSE_cc1101.SetTx();
  delay(5);
  pinMode(CC_GDO0, OUTPUT);
  for (int rep = 0; rep < 3; rep++) {
    for (int i = 0; i < currentSignal.dataLength; i++) {
      bool level = (i < currentSignal.dataLength && currentSignal.levels[i] > 0) ?
                   (currentSignal.levels[i] == 1) : (i % 2 == 0);
      digitalWrite(CC_GDO0, level ? HIGH : LOW);
      delayMicroseconds(currentSignal.timings[i]);
      txProgress = (i * 100) / currentSignal.dataLength;
    }
    digitalWrite(CC_GDO0, LOW);
    delayMicroseconds(10000);
  }
  digitalWrite(CC_GDO0, LOW);
  ELECHOUSE_cc1101.setSidle();
  pinMode(CC_GDO0, INPUT);
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

void jamChannel(uint32_t freqKHz) {
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(freqKHz / 1000.0);
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.setPA(12);
  ELECHOUSE_cc1101.SetTx();
  delay(2);
  pinMode(CC_GDO0, OUTPUT);
  for (int i = 0; i < 50; i++) {
    digitalWrite(CC_GDO0, HIGH);
    delayMicroseconds(100);
    digitalWrite(CC_GDO0, LOW);
    delayMicroseconds(100);
  }
  digitalWrite(CC_GDO0, LOW);
}

// ============================================
// GETTERS/SETTERS DO ESTADO
// ============================================

CCState getCCState() { return ccState; }
void setCCState(CCState state) { ccState = state; }
bool isTXPlaying() { return txPlaying; }
void setTXPlaying(bool val) { txPlaying = val; }
int getTXProgress() { return txProgress; }
uint32_t getCaptureStartTime() { return captureStartTime; }
uint32_t getDetectedFrequency() { return detectedFrequency; }
void setDetectedFrequency(uint32_t freq) { detectedFrequency = freq; }
