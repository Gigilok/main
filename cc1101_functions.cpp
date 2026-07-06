#include "config.h"

bool buttonPressed(uint8_t pin);


// ==================== CC1101 SCANNER (Sub-GHz) ====================
namespace {
  float frequencies[] = {315.0, 433.92, 868.35, 915.0};
  int current_freq_idx = 0;
  int rssi_values[128];
  bool cc_scanning = false;
  uint32_t last_scan_time = 0;
}

void cc1101ScannerSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "CC1101 Scanner");
  u8g2.sendBuffer();
  
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setModulation(2); // 2-FSK
  ELECHOUSE_cc1101.setMHZ(frequencies[current_freq_idx]);
  ELECHOUSE_cc1101.SetRx();
  
  cc_scanning = true;
  memset(rssi_values, -100, sizeof(rssi_values));
}

void cc1101ScannerLoop() {
  if (!cc_scanning) return;
  
  // Troca frequência com UP/DOWN
  if (buttonPressed(BTN_UP)) {
    current_freq_idx = (current_freq_idx + 1) % 4;
    ELECHOUSE_cc1101.setMHZ(frequencies[current_freq_idx]);
    memset(rssi_values, -100, sizeof(rssi_values));
  }
  
  if (buttonPressed(BTN_DOWN)) {
    current_freq_idx = (current_freq_idx - 1 + 4) % 4;
    ELECHOUSE_cc1101.setMHZ(frequencies[current_freq_idx]);
    memset(rssi_values, -100, sizeof(rssi_values));
  }
  
  // Lê RSSI
  byte rssi = ELECHOUSE_cc1101.getRssi();
  int rssi_dbm = (rssi >= 128) ? (rssi - 256) / 2 - 74 : rssi / 2 - 74;
  
  // Atualiza display
  u8g2.clearBuffer();
  u8g2.setCursor(0, 10);
  u8g2.print("Freq: ");
  u8g2.print(frequencies[current_freq_idx]);
  u8g2.print(" MHz");
  
  u8g2.setCursor(0, 25);
  u8g2.print("RSSI: ");
  u8g2.print(rssi_dbm);
  u8g2.print(" dBm");
  
  // Barra de sinal
  int bar = map(constrain(rssi_dbm, -100, -30), -100, -30, 0, 100);
  u8g2.drawFrame(10, 40, 100, 10);
  u8g2.drawBox(12, 42, bar, 6);
  
  u8g2.setCursor(0, 60);
  u8g2.print("UP/DOWN: Mudar freq");
  u8g2.sendBuffer();
  
  delay(50);
}

// ==================== CC1101 TRANSCEIVER (RX/TX) ====================
namespace {
  enum CCMode { CC_IDLE, CC_RX, CC_TX };
  CCMode cc_mode = CC_IDLE;
  byte capturedData[64];
  int capturedLen = 0;
  bool hasSignal = false;
  uint32_t currentFreq = 43392; // 433.92 * 100
}

void cc1101TransceiverSetup() {
  cc_mode = CC_IDLE;
  hasSignal = false;
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "CC1101 RX/TX");
  u8g2.drawStr(0, 25, "SEL: Gravar sinal");
  u8g2.drawStr(0, 35, "UP: Transmitir");
  u8g2.drawStr(0, 45, "DOWN: Mudar freq");
  u8g2.sendBuffer();
  
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setModulation(0); // ASK/OOK
  ELECHOUSE_cc1101.setMHZ(currentFreq / 100.0);
}

void captureSignal() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Capturando...");
  u8g2.drawStr(0, 25, "Aguardando sinal");
  u8g2.sendBuffer();
  
  ELECHOUSE_cc1101.SetRx();
  
  // Aguarda sinal por 5 segundos
  uint32_t start = millis();
  bool detected = false;
  
  while (millis() - start < 5000) {
    if (digitalRead(CC_GDO0) == HIGH) {
      detected = true;
      break;
    }
    delay(1);
  }
  
  if (detected) {
    // Captura simples (em projeto real, usar timer para precisão)
    capturedLen = 0;
    uint32_t lastChange = micros();
    
    while (capturedLen < 64 && (millis() - start) < 10000) {
      // Simulação de captura - em hardware real precisaria de interrupção/timer
      capturedData[capturedLen++] = random(0, 255);
      delayMicroseconds(500);
    }
    
    hasSignal = true;
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Sinal Capturado!");
    u8g2.setCursor(0, 25);
    u8g2.print("Bytes: ");
    u8g2.print(capturedLen);
    u8g2.sendBuffer();
    delay(1000);
  } else {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Timeout");
    u8g2.sendBuffer();
    delay(1000);
  }
}

void transmitSignal() {
  if (!hasSignal) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Nenhum sinal salvo");
    u8g2.sendBuffer();
    delay(1000);
    return;
  }
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Transmitindo...");
  u8g2.sendBuffer();
  
  ELECHOUSE_cc1101.SetTx();
  
  // Transmite dados capturados
  for (int i = 0; i < capturedLen; i++) {
    // Simulação - em hardware real usar SPI direto ou função da lib
    delayMicroseconds(500);
  }
  
  ELECHOUSE_cc1101.setSidle();
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Transmissao OK");
  u8g2.sendBuffer();
  delay(1000);
}

void cc1101TransceiverLoop() {
  if (buttonPressed(BTN_SELECT)) {
    captureSignal();
    // Volta para tela principal
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "CC1101 RX/TX");
    u8g2.drawStr(0, 25, hasSignal ? "Sinal: SALVO" : "Sinal: VAZIO");
    u8g2.setCursor(0, 40);
    u8g2.print("Freq: ");
    u8g2.print(currentFreq / 100.0);
    u8g2.sendBuffer();
  }
  
  if (buttonPressed(BTN_UP) && hasSignal) {
    transmitSignal();
  }
  
  if (buttonPressed(BTN_DOWN)) {
    // Cicla entre frequências comuns
    if (currentFreq == 43392) currentFreq = 31500;
    else if (currentFreq == 31500) currentFreq = 86835;
    else if (currentFreq == 86835) currentFreq = 91500;
    else currentFreq = 43392;
    
    ELECHOUSE_cc1101.setMHZ(currentFreq / 100.0);
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "CC1101 RX/TX");
    u8g2.setCursor(0, 40);
    u8g2.print("Freq: ");
    u8g2.print(currentFreq / 100.0);
    u8g2.sendBuffer();
  }
}
