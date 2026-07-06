#include "config.h"

// ==================== CONFIGURAÇÕES ====================
#define CAPTURE_TIMEOUT_MS      10000
#define RSSI_THRESHOLD          -75
#define MAX_RAW_DATA            512
#define MIN_PULSE_US            100

const uint32_t SCAN_FREQUENCIES[] = {31500, 43392, 86835, 91500};
const int NUM_FREQS = 4;

// ==================== VARIÁVEIS LOCAIS (NAMESPACE) ====================
namespace {
  // REMOVIDO: CC1101Signal currentSignal; (já está no config.h como global)
  // REMOVIDO: bool hasSavedSignal = false; (já está no config.h como global)
  
  enum CCState { CC_IDLE, CC_SCANNING, CC_CAPTURING, CC_TRANSMITTING, CC_PLAYING };
  CCState ccState = CC_IDLE;
  
  volatile uint32_t lastInterruptTime = 0;
  volatile uint16_t captureIndex = 0;
  volatile bool capturing = false;
  uint16_t tempBuffer[MAX_RAW_DATA];
  
  bool txPlaying = false;
  int txProgress = 0;
  uint32_t captureStartTime = 0;
}

// ==================== FUNÇÕES DE ACESSO ====================
// Função para permitir acesso externo se necessário
CC1101Signal* getCurrentSignal() { 
  return &currentSignal;  // Agora referencia a global do config.h
}

// Função chamada pelo menu para resetar estado
void resetCC1101State() {
  ccState = CC_IDLE;
  txPlaying = false;
  capturing = false;
  captureIndex = 0;
  ELECHOUSE_cc1101.setSidle();
}

// ==================== EEPROM/NVS ====================
void initCC1101Storage() {
  // prefs já está declarado no config.h como extern
  if (!prefs.begin("cc1101sig", false)) {
    Serial.println("NVS: Erro ao abrir namespace");
  }
  
  size_t len = prefs.getBytesLength("signal");
  if (len == sizeof(CC1101Signal)) {
    prefs.getBytes("signal", &currentSignal, sizeof(CC1101Signal));
    // Verifica se é válido (frequência != 0)
    if (currentSignal.frequency > 0 && currentSignal.dataLength > 0) {
      hasSavedSignal = true;
      Serial.println("CC1101: Sinal anterior carregado");
    }
  }
}

void saveSignalToMemory() {
  prefs.putBytes("signal", &currentSignal, sizeof(CC1101Signal));
  hasSavedSignal = true;
  Serial.println("CC1101: Sinal salvo na memória");
}

void clearSavedSignal() {
  currentSignal.frequency = 0;
  currentSignal.dataLength = 0;
  prefs.putBytes("signal", &currentSignal, sizeof(CC1101Signal));
  hasSavedSignal = false;
  Serial.println("CC1101: Sinal apagado");
}

// ==================== AUTO DETECÇÃO DE FREQUÊNCIA ====================
uint32_t autoDetectFrequency() {
  int bestRssi = -1000;
  uint32_t bestFreq = 0;
  
  for (int i = 0; i < NUM_FREQS; i++) {
    uint32_t freq = SCAN_FREQUENCIES[i];
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(freq / 100.0);
    ELECHOUSE_cc1101.SetRx();
    delay(50);
    
    int maxRssi = -1000;
    for (int j = 0; j < 20; j++) {
      byte rssiRaw = ELECHOUSE_cc1101.getRssi();
      int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
      if (rssi > maxRssi) maxRssi = rssi;
      delay(5);
    }
    
    if (maxRssi > bestRssi) {
      bestRssi = maxRssi;
      bestFreq = freq;
    }
    
    u8g2.setCursor(0, 40);
    u8g2.print("Testando ");
    u8g2.print(freq / 100.0, 2);
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
  if (captureIndex > 0 && captureIndex < MAX_RAW_DATA) {
    tempBuffer[captureIndex - 1] = (uint16_t)(now - lastInterruptTime);
  }
  lastInterruptTime = now;
  captureIndex++;
}

void startCapture(uint32_t frequency) {
  memset(tempBuffer, 0, sizeof(tempBuffer));
  captureIndex = 0;
  capturing = true;
  captureStartTime = millis();
  
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(frequency / 100.0);
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.SetRx();
  delay(10);
  
  attachInterrupt(digitalPinToInterrupt(CC_GDO0), onGDO0Interrupt, CHANGE);
  lastInterruptTime = micros();
}

void stopCapture() {
  detachInterrupt(digitalPinToInterrupt(CC_GDO0));
  capturing = false;
  
  if (captureIndex > 10) {
    currentSignal.frequency = 0; // Será definido depois
    currentSignal.dataLength = min((int)captureIndex, MAX_RAW_DATA);
    currentSignal.modulation = 0;
    
    for (int i = 0; i < currentSignal.dataLength; i++) {
      currentSignal.timings[i] = tempBuffer[i];
    }
    
    byte rssiRaw = ELECHOUSE_cc1101.getRssi();
    currentSignal.rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
  }
}

// ==================== TRANSMISSÃO ====================
void transmitRawSignal() {
  if (!hasSavedSignal || currentSignal.dataLength == 0) return;
  
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(currentSignal.frequency / 100.0);
  ELECHOUSE_cc1101.setModulation(0);
  ELECHOUSE_cc1101.SetTx();
  
  // Transmite 3 vezes (padrão de controles remotos)
  for (int rep = 0; rep < 3; rep++) {
    for (int i = 0; i < currentSignal.dataLength; i++) {
      delayMicroseconds(currentSignal.timings[i]);
      txProgress = (i * 100) / currentSignal.dataLength;
    }
    delayMicroseconds(10000); // Gap entre repetições
  }
  
  ELECHOUSE_cc1101.setSidle();
}

// ==================== SETUP E LOOP PRINCIPAL ====================
void cc1101TransceiverSetup() {
  initCC1101Storage();
  ccState = CC_IDLE;
  txPlaying = false;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "CC1101 Capture");
  
  if (hasSavedSignal) {
    u8g2.setCursor(0, 25);
    u8g2.print("Freq: ");
    u8g2.print(currentSignal.frequency / 1000.0, 3);
    u8g2.print(" MHz");
    u8g2.setCursor(0, 37);
    u8g2.print("RSSI: ");
    u8g2.print(currentSignal.rssi);
    u8g2.print(" dBm");
    u8g2.setCursor(0, 49);
    u8g2.print("Pulsos: ");
    u8g2.print(currentSignal.dataLength);
    u8g2.setCursor(0, 62);
    u8g2.print("UP:Play SEL:Rec B:Menu");
  } else {
    u8g2.drawStr(0, 25, "Nenhum sinal salvo");
    u8g2.drawStr(0, 40, "SEL: Gravar novo");
    u8g2.drawStr(0, 55, "BACK: Voltar ao menu");
  }
  
  u8g2.sendBuffer();
}

void cc1101TransceiverLoop() {
  // Verifica BACK primeiro (usando a função do main.ino ou local)
  extern bool buttonPressed(uint8_t pin);
  if (buttonPressed(BTN_BACK)) {
    resetCC1101State();
    extern uint8_t current_screen;
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    delay(200);
    return;
  }
  
  // Estado IDLE
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
      
      currentSignal.frequency = detectedFreq;
      u8g2.clearBuffer();
      u8g2.setCursor(0, 10);
      u8g2.print("Freq: ");
      u8g2.print(detectedFreq / 100.0, 2);
      u8g2.print(" MHz");
      u8g2.drawStr(0, 25, "Capturando em 10s");
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
    
    else if (buttonPressed(BTN_DOWN) && hasSavedSignal) {
      clearSavedSignal();
      u8g2.clearBuffer();
      u8g2.drawStr(0, 30, "Sinal apagado!");
      u8g2.sendBuffer();
      delay(1000);
      cc1101TransceiverSetup();
    }
  }
  
  // Estado CAPTURING
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
        saveSignalToMemory();
        
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, "Sinal Capturado!");
        u8g2.setCursor(0, 25);
        u8g2.print("Freq: ");
        u8g2.print(currentSignal.frequency / 100.0, 2);
        u8g2.print(" MHz");
        u8g2.setCursor(0, 40);
        u8g2.print("Pulsos: ");
        u8g2.print(currentSignal.dataLength);
        u8g2.setCursor(0, 55);
        u8g2.print("SALVO! B:Menu");
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
  
  // Estado TRANSMITTING
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
  extern bool buttonPressed(uint8_t pin);
  if (buttonPressed(BTN_BACK)) {
    ELECHOUSE_cc1101.setSidle();
    extern uint8_t current_screen;
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
  u8g2.sendBuffer();
  
  delay(100);
}
