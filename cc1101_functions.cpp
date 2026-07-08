#include "config.h"

// ==================== CONFIGURAÇÕES ====================
#define CAPTURE_TIMEOUT_MS      10000
#define RSSI_THRESHOLD          -75
#define MAX_RAW_DATA            512
#define MIN_PULSE_US            100

const uint32_t SCAN_FREQUENCIES[] = {315000, 433920, 868350, 915000};
const int NUM_FREQS = 4;

// Frequências comuns para brute force (em kHz)
const uint32_t BRUTE_FREQS[] = {
  300000, 303000, 304000, 305000, 310000, 315000, 318000,
  330000, 390000, 418000, 433000, 433920, 434000, 868000, 868350, 915000
};
const int NUM_BRUTE_FREQS = sizeof(BRUTE_FREQS) / sizeof(BRUTE_FREQS[0]);

// Protocolos comuns de controles (RCSwitch)
struct RCSwitchProtocol {
  const char* name;
  uint16_t pulseLength;
  uint8_t syncFactor[2];
  uint8_t zero[2];
  uint8_t one[2];
  bool inverted;
};

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

// ==================== VARIÁVEIS LOCAIS ====================
namespace {
  enum CCState { CC_IDLE, CC_SCANNING, CC_CAPTURING, CC_TRANSMITTING, CC_PLAYING };
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
  
  // Brute force
  enum BFState { BF_IDLE, BF_RUNNING, BF_PAUSED };
  BFState bfState = BF_IDLE;
  int bfFreqIndex = 0;
  int bfProtocolIndex = 0;
  uint32_t bfCode = 0;
  int bfBitIndex = 0;
  unsigned long bfLastTransmit = 0;
  bool bfDirection = true;
  
  // Frequency sweep
  bool sweepRunning = false;
  float sweepFreq = 300.0;
  float sweepStep = 1.0;
  int sweepRssiHistory[128];
  int sweepIndex = 0;
}

// ==================== FUNÇÕES DE ACESSO ====================
CC1101Signal* getCurrentSignal() { 
  return &currentSignal;
}

void resetCC1101State() {
  ccState = CC_IDLE;
  txPlaying = false;
  capturing = false;
  captureIndex = 0;
  bfState = BF_IDLE;
  sweepRunning = false;
  ELECHOUSE_cc1101.setSidle();
}

// ==================== NVS - MEMÓRIA INTERNA ESP32 ====================
// Salva sinal no slot selecionado (0-4) na flash interna do ESP32
void saveSignalToSlot(int slot, const CC1101Signal& sig) {
  if (slot < 0 || slot >= MAX_SAVED_SIGNALS) return;
  
  char keyName[16];
  snprintf(keyName, sizeof(keyName), "sig%d", slot);
  
  // Salva na NVS (memória interna - NÃO precisa de cartão SD!)
  prefs.putBytes(keyName, &sig, sizeof(CC1101Signal));
  
  // Atualiza array em memória
  savedSignals[slot].signal = sig;
  savedSignals[slot].active = true;
  snprintf(savedSignals[slot].name, 16, "Sinal %d", slot + 1);
  
  hasSavedSignal = true;
  currentSignal = sig;
  
  Serial.printf("Sinal salvo no slot %d: %.3f MHz, %d pulsos\n", 
                slot, sig.frequency / 1000.0, sig.dataLength);
}

// Apaga sinal do slot
void clearSignalSlot(int slot) {
  if (slot < 0 || slot >= MAX_SAVED_SIGNALS) return;
  
  char keyName[16];
  snprintf(keyName, sizeof(keyName), "sig%d", slot);
  
  CC1101Signal emptySignal;
  memset(&emptySignal, 0, sizeof(CC1101Signal));
  
  prefs.putBytes(keyName, &emptySignal, sizeof(CC1101Signal));
  
  savedSignals[slot].active = false;
  memset(savedSignals[slot].name, 0, 16);
  
  // Verifica se ainda há sinais ativos
  hasSavedSignal = false;
  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    if (savedSignals[i].active) {
      hasSavedSignal = true;
      currentSignal = savedSignals[i].signal;
      break;
    }
  }
  
  Serial.printf("Sinal do slot %d apagado\n", slot);
}

// ==================== AUTO DETECÇÃO DE FREQUÊNCIA ====================
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

// ==================== CAPTURA COM INTERRUPÇÃO ====================
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

// ==================== TRANSMISSÃO CORRIGIDA ====================
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

// ==================== BRUTE FORCE FUNÇÕES AUXILIARES ====================
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

// ==================== SETUP E LOOP PRINCIPAL (CAPTURE/TX) ====================
void cc1101TransceiverSetup() {
  ccState = CC_IDLE;
  txPlaying = false;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "CC1101 Capture");
  
  if (hasSavedSignal) {
    u8g2.setCursor(0, 22);
    u8g2.print("Slot: ");
    u8g2.print(selectedSignalSlot + 1);
    u8g2.print("/");
    u8g2.print(MAX_SAVED_SIGNALS);
    
    u8g2.setCursor(0, 33);
    u8g2.print("Freq: ");
    u8g2.print(currentSignal.frequency / 1000.0, 3);
    u8g2.print(" MHz");
    
    u8g2.setCursor(0, 44);
    u8g2.print("RSSI: ");
    u8g2.print(currentSignal.rssi);
    u8g2.print(" dBm");
    
    u8g2.setCursor(0, 55);
    u8g2.print("UP:Play SEL:Rec B:Menu");
  } else {
    u8g2.drawStr(0, 22, "Nenhum sinal salvo");
    u8g2.drawStr(0, 33, "SEL: Gravar novo");
    u8g2.drawStr(0, 44, "DOWN: BruteForce");
    u8g2.drawStr(0, 55, "BACK: Voltar ao menu");
  }
  
  u8g2.sendBuffer();
}

void cc1101TransceiverLoop() {
  if (buttonPressed(BTN_BACK)) {
    resetCC1101State();
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    delay(200);
    return;
  }
  
  if (ccState == CC_IDLE) {
    if (buttonPressed(BTN_SELECT)) {
      ccState = CC_SCANNING;
      
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "Auto-Scan...");
      u8g2.drawStr(0, 25, "Detectando freq...");
      u8g2.sendBuffer();
      
      uint32_t detectedFreq = autoDetectFrequency();
      
      if (detectedFreq == 0) {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 25, "Nenhum sinal");
        u8g2.drawStr(0, 40, "encontrado!");
        u8g2.sendBuffer();
        delay(2000);
        ccState = CC_IDLE;
        cc1101TransceiverSetup();
        return;
      }
      
      detectedFrequency = detectedFreq;
      u8g2.clearBuffer();
      u8g2.setCursor(0, 10);
      u8g2.print("Freq: ");
      u8g2.print(detectedFreq / 1000.0, 3);
      u8g2.print(" MHz");
      u8g2.drawStr(0, 25, "Capturando em 10s");
      u8g2.drawStr(0, 40, "Aperte o controle!");
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
    
    u8g2.setCursor(0, 40);
    u8g2.print("Tempo: ");
    u8g2.print(remaining);
    u8g2.print("s   ");
    
    u8g2.setCursor(0, 52);
    u8g2.print("Pulsos: ");
    u8g2.print(captureIndex);
    u8g2.sendBuffer();
    
    if (elapsed >= CAPTURE_TIMEOUT_MS || captureIndex >= MAX_RAW_DATA - 10) {
      stopCapture();
      
      if (captureIndex > 10) {
        // Salva no slot selecionado na NVS (memória interna!)
        saveSignalToSlot(selectedSignalSlot, currentSignal);
        
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Sinal Capturado!");
        u8g2.setCursor(0, 25);
        u8g2.print("Freq: ");
        u8g2.print(currentSignal.frequency / 1000.0, 3);
        u8g2.print(" MHz");
        u8g2.setCursor(0, 40);
        u8g2.print("Pulsos: ");
        u8g2.print(currentSignal.dataLength);
        u8g2.setCursor(0, 55);
        u8g2.print("SALVO no slot ");
        u8g2.print(selectedSignalSlot + 1);
        u8g2.sendBuffer();
        
        uint32_t waitStart = millis();
        while (millis() - waitStart < 3000) {
          if (buttonPressed(BTN_BACK)) break;
          delay(10);
        }
      } else {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 30, "Sinal muito curto");
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
      u8g2.setFont(u8g2_font_6x10_tr);
      
      u8g2.setCursor(0, 10);
      u8g2.print("TX ");
      u8g2.print(currentSignal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
      
      int waveY = 30;
      for (int x = 0; x < 128; x += 2) {
        int idx = (x * currentSignal.dataLength) / 128;
        if (idx < currentSignal.dataLength) {
          int h = min(currentSignal.timings[idx] / 100, 20);
          if (idx % 2 == 0) {
            u8g2.drawVLine(x, waveY - h/2, h);
          }
        }
      }
      
      u8g2.drawFrame(10, 45, 108, 10);
      u8g2.drawBox(12, 47, (txProgress * 104) / 100, 6);
      
      u8g2.setCursor(0, 62);
      u8g2.print("Transmitindo...");
      u8g2.sendBuffer();
      
      transmitRawSignal();
      
      txPlaying = false;
      
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "Transmissao OK");
      u8g2.setCursor(0, 30);
      u8g2.print("Freq: ");
      u8g2.print(currentSignal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
      u8g2.drawStr(0, 50, "UP:Repetir B:Menu");
      u8g2.sendBuffer();
    }
    
    if (buttonPressed(BTN_UP)) {
      txPlaying = true;
    }
  }
}

// ==================== SCANNER SIMPLES ====================
void cc1101ScannerSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "CC1101 Scanner");
  u8g2.sendBuffer();
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetRx();
}

void cc1101ScannerLoop() {
  if (buttonPressed(BTN_BACK)) {
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    delay(200);
    return;
  }
  
  byte rssi = ELECHOUSE_cc1101.getRssi();
  int rssi_dbm = (rssi >= 128) ? (rssi - 256) / 2 - 74 : rssi / 2 - 74;
  
  u8g2.clearBuffer();
  u8g2.setCursor(0, 10);
  u8g2.print("RSSI: ");
  u8g2.print(rssi_dbm);
  u8g2.print(" dBm");
  
  int bar = map(constrain(rssi_dbm, -100, -30), -100, -30, 0, 100);
  u8g2.drawFrame(10, 40, 100, 10);
  u8g2.drawBox(12, 42, bar, 6);
  
  u8g2.setCursor(0, 60);
  u8g2.print("Freq: 433.92 MHz");
  u8g2.sendBuffer();
  
  delay(100);
}

// ==================== BRUTE FORCE DE BRUIJN ====================
void cc1101BruteForceSetup() {
  bfState = BF_IDLE;
  bfFreqIndex = 0;
  bfProtocolIndex = 0;
  bfCode = 0;
  bfBitIndex = 0;
  bfDirection = true;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "CC1101 BruteForce");
  u8g2.drawStr(0, 22, "SEL: Iniciar/Parar");
  u8g2.drawStr(0, 34, "UP: Prox. Protocolo");
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
    bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
    bfCode = 0;
    delay(150);
  }
  
  if (buttonPressed(BTN_DOWN) && bfState != BF_RUNNING) {
    bfFreqIndex = (bfFreqIndex + 1) % NUM_BRUTE_FREQS;
    bfCode = 0;
    delay(150);
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "BruteForce");
  
  u8g2.setCursor(0, 22);
  u8g2.print(bfState == BF_RUNNING ? "Status: ATIVO" : (bfState == BF_PAUSED ? "Status: PAUSADO" : "Status: PARADO"));
  
  u8g2.setCursor(0, 34);
  u8g2.print("Freq: ");
  u8g2.print(BRUTE_FREQS[bfFreqIndex] / 1000.0, 2);
  u8g2.print(" MHz");
  
  u8g2.setCursor(0, 46);
  u8g2.print("Proto: ");
  u8g2.print(protocols[bfProtocolIndex].name);
  
  u8g2.setCursor(0, 58);
  u8g2.print("Code: 0x");
  u8g2.print(bfCode, HEX);
  u8g2.sendBuffer();
  
  if (bfState == BF_RUNNING) {
    const RCSwitchProtocol& p = protocols[bfProtocolIndex];
    
    transmitCode(bfCode, 24, p);
    
    delayMicroseconds(10000);
    
    bfCode++;
    
    if (bfCode > 0xFFFFFF) {
      bfCode = 0;
      bfProtocolIndex = (bfProtocolIndex + 1) % NUM_PROTOCOLS;
    }
    
    if (bfCode % 100 == 0) {
      u8g2.setCursor(80, 58);
      u8g2.print("(");
      u8g2.print(bfCode % 1000);
      u8g2.print(")");
      u8g2.sendBuffer();
    }
  }
}

// ==================== FREQUENCY SWEEP ====================
void cc1101FreqSweepSetup() {
  sweepRunning = false;
  sweepFreq = 300.0;
  sweepStep = 1.0;
  sweepIndex = 0;
  memset(sweepRssiHistory, 0, sizeof(sweepRssiHistory));
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "Freq Sweep");
  u8g2.drawStr(0, 22, "SEL: Iniciar/Parar");
  u8g2.drawStr(0, 34, "UP: +Step  DOWN: -Step");
  u8g2.drawStr(0, 46, "Range: 300-928 MHz");
  u8g2.drawStr(0, 58, "BACK: Menu");
  u8g2.sendBuffer();
}

void cc1101FreqSweepLoop() {
  if (buttonPressed(BTN_BACK)) {
    sweepRunning = false;
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
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
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "Freq Sweep");
  
  u8g2.setCursor(0, 22);
  u8g2.print(sweepRunning ? "Status: SCANNING" : "Status: PARADO");
  
  u8g2.setCursor(0, 34);
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
    
    for (int i = 0; i < 127; i++) {
      sweepRssiHistory[i] = sweepRssiHistory[i + 1];
    }
    sweepRssiHistory[127] = h;
    
    for (int x = 0; x < 128; x++) {
      if (sweepRssiHistory[x] > 0) {
        u8g2.drawVLine(x, 64 - sweepRssiHistory[x], sweepRssiHistory[x]);
      }
    }
    
    sweepFreq += sweepStep;
    if (sweepFreq > 928.0) {
      sweepFreq = 300.0;
    }
    ELECHOUSE_cc1101.setMHZ(sweepFreq);
    
    u8g2.setCursor(80, 58);
    u8g2.print("RSSI:");
    u8g2.print(currentRssi);
  }
  
  u8g2.sendBuffer();
  delay(50);
}
