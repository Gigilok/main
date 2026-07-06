#include "config.h"
#include <Preferences.h>

// ==================== CONFIGURAÇÕES AVANÇADAS CC1101 ====================
#define CAPTURE_TIMEOUT_MS      10000  // 10 segundos escutando
#define RSSI_THRESHOLD          -75     // dBm para considerar sinal detectado
#define MAX_RAW_DATA          512     // Máximo de transições ( HIGH/LOW )
#define MIN_PULSE_US          100     // Mínimo 100us para considerar válido

// Frequências a escanear (MHz * 100 para evitar float)
const uint32_t SCAN_FREQUENCIES[] = {31500, 43392, 86835, 91500};
const int NUM_FREQS = 4;

// Estrutura otimizada para EEPROM/Flash (NVS)
struct SignalData {
  uint32_t magic;           // 0xDEADBEEF para validar
  uint32_t frequency;       // Frequência em kHz (ex: 433920)
  uint16_t dataLength;      // Comprimento em transições
  uint8_t modulation;       // 0=ASK/OOK, 1=FSK
  int16_t rssi;             // RSSI no momento da captura
  uint32_t timestamp;       // millis quando capturou
  uint16_t timings[MAX_RAW_DATA]; // Durações em microssegundos (alternando HIGH/LOW)
};

// Namespace para variáveis do CC1101
namespace {
  Preferences prefs;
  SignalData currentSignal;
  bool hasSavedSignal = false;
  
  // Estados do transceptor
  enum CCState { CC_IDLE, CC_SCANNING, CC_CAPTURING, CC_TRANSMITTING };
  CCState ccState = CC_IDLE;
  
  // Variáveis de captura
  volatile uint32_t lastInterruptTime = 0;
  volatile uint16_t captureIndex = 0;
  volatile bool capturing = false;
  uint16_t tempBuffer[MAX_RAW_DATA];
  
  // Transmissão
  bool txPlaying = false;
  int txProgress = 0;
  uint32_t txStartTime = 0;
}

// ==================== FUNÇÕES EEPROM/NVS ====================

void initCC1101Storage() {
  prefs.begin("cc1101sig", false); // Namespace "cc1101sig"
  
  // Verifica se há sinal salvo
  if (prefs.getBytesLength("signal") == sizeof(SignalData)) {
    prefs.getBytes("signal", &currentSignal, sizeof(SignalData));
    if (currentSignal.magic == 0xDEADBEEF) {
      hasSavedSignal = true;
    }
  }
}

void saveSignalToMemory() {
  // Sobrescreve sempre (apenas último sinal)
  currentSignal.magic = 0xDEADBEEF;
  currentSignal.timestamp = millis();
  prefs.putBytes("signal", &currentSignal, sizeof(SignalData));
  hasSavedSignal = true;
}

void clearSavedSignal() {
  currentSignal.magic = 0;
  prefs.putBytes("signal", &currentSignal, sizeof(SignalData));
  hasSavedSignal = false;
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
    delay(50); // Tempo para estabilizar
    
    // Lê RSSI várias vezes e pega o maior
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
    
    // Mostra progresso no display
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
  if (captureIndex > 0) {
    tempBuffer[captureIndex - 1] = (uint16_t)(now - lastInterruptTime);
  }
  lastInterruptTime = now;
  captureIndex++;
}

void startCapture(uint32_t frequency) {
  memset(tempBuffer, 0, sizeof(tempBuffer));
  captureIndex = 0;
  capturing = true;
  
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(frequency / 100.0);
  ELECHOUSE_cc1101.setModulation(0); // ASK/OOK
  ELECHOUSE_cc1101.SetRx();
  
  delay(10);
  
  // Configura interrupção no GDO0 (borda de subida e descida)
  attachInterrupt(digitalPinToInterrupt(CC_GDO0), onGDO0Interrupt, CHANGE);
  
  lastInterruptTime = micros();
}

void stopCapture() {
  detachInterrupt(digitalPinToInterrupt(CC_GDO0));
  capturing = false;
  
  // Processa dados capturados
  if (captureIndex > 10) { // Mínimo de transições válidas
    currentSignal.frequency = 0; // Será definido depois
    currentSignal.dataLength = min((int)captureIndex, MAX_RAW_DATA);
    currentSignal.modulation = 0;
    
    // Copia dados
    for (int i = 0; i < currentSignal.dataLength; i++) {
      currentSignal.timings[i] = tempBuffer[i];
    }
    
    // Lê RSSI final
    byte rssiRaw = ELECHOUSE_cc1101.getRssi();
    currentSignal.rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
  }
}

// ==================== TRANSMISSÃO ====================

void transmitRawSignal(bool continuous = false) {
  if (!hasSavedSignal || currentSignal.dataLength == 0) return;
  
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(currentSignal.frequency / 100.0);
  ELECHOUSE_cc1101.setModulation(currentSignal.modulation);
  
  // Para ASK/OOK, usamos controle direto do pino ou modo TX
  ELECHOUSE_cc1101.SetTx();
  
  // Simulação de transmissão RAW (em hardware real, usar timer para precisão)
  // Nota: A biblioteca ELECHOUSE não tem função nativa para TX RAW preciso
  // Implementação usando digitalWrite no CC_GDO0 se possível, ou SPI burst
  
  for (int repeat = 0; repeat < (continuous ? 0 : 5); repeat++) { // 5 repetições ou contínuo
    for (int i = 0; i < currentSignal.dataLength; i++) {
      // Alterna estado (simplificado - em implementação real usar timer hardware)
      delayMicroseconds(currentSignal.timings[i]);
      
      // Atualiza progresso visual
      if (i % 10 == 0) {
        txProgress = (i * 100) / currentSignal.dataLength;
      }
    }
    delay(10); // Gap entre repetições
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
  u8g2.drawStr(0, 10, "CC1101 Advanced");
  
  if (hasSavedSignal) {
    u8g2.setCursor(0, 25);
    u8g2.print("Sinal salvo: ");
    u8g2.print(currentSignal.frequency / 1000.0, 3);
    u8g2.print(" MHz");
    u8g2.setCursor(0, 37);
    u8g2.print("RSSI: ");
    u8g2.print(currentSignal.rssi);
    u8g2.print(" dBm");
    u8g2.setCursor(0, 49);
    u8g2.print("Pulsos: ");
    u8g2.print(currentSignal.dataLength);
  } else {
    u8g2.drawStr(0, 25, "Nenhum sinal salvo");
  }
  
  u8g2.drawStr(0, 62, "SEL:Gravar UP:Play");
  u8g2.sendBuffer();
}

void cc1101TransceiverLoop() {
  // Estado IDLE - Menu principal
  if (ccState == CC_IDLE) {
    if (buttonPressed(BTN_SELECT)) {
      // Inicia modo de gravação com auto-detect
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
        cc1101TransceiverSetup(); // Volta para tela inicial
        return;
      }
      
      // Frequência detectada, inicia captura
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
      // Inicia reprodução
      ccState = CC_TRANSMITTING;
      txPlaying = true;
      txProgress = 0;
    }
    
    else if (buttonPressed(BTN_DOWN) && hasSavedSignal) {
      // Apaga sinal salvo
      clearSavedSignal();
      u8g2.clearBuffer();
      u8g2.drawStr(0, 30, "Sinal apagado!");
      u8g2.sendBuffer();
      delay(1000);
      cc1101TransceiverSetup();
    }
  }
  
  // Estado CAPTURING - Gravando sinal
  else if (ccState == CC_CAPTURING) {
    static uint32_t captureStartTime = 0;
    static bool firstRun = true;
    
    if (firstRun) {
      captureStartTime = millis();
      firstRun = false;
    }
    
    uint32_t elapsed = millis() - captureStartTime;
    int remaining = (CAPTURE_TIMEOUT_MS - elapsed) / 1000;
    
    // Atualiza display
    u8g2.setCursor(0, 40);
    u8g2.print("Tempo: ");
    u8g2.print(remaining);
    u8g2.print("s   ");
    
    u8g2.setCursor(0, 52);
    u8g2.print("Pulsos: ");
    u8g2.print(captureIndex);
    u8g2.sendBuffer();
    
    // Verifica timeout ou buffer cheio
    if (elapsed >= CAPTURE_TIMEOUT_MS || captureIndex >= MAX_RAW_DATA - 10) {
      stopCapture();
      firstRun = true;
      
      if (captureIndex > 10) {
        // Salva na memória (sobrescreve anterior)
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
        u8g2.print("SALVO NA MEMORIA");
        u8g2.sendBuffer();
        delay(3000);
      } else {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 30, "Sinal muito curto");
        u8g2.drawStr(0, 45, "ou timeout");
        u8g2.sendBuffer();
        delay(2000);
      }
      
      ccState = CC_IDLE;
      cc1101TransceiverSetup();
    }
  }
  
  // Estado TRANSMITTING - Reproduzindo
  else if (ccState == CC_TRANSMITTING) {
    if (txPlaying) {
      // Tela de transmissão
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      
      // Cabeçalho
      u8g2.setCursor(0, 10);
      u8g2.print("TX ");
      u8g2.print(currentSignal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
      
      // Visualização do sinal (onda simplificada)
      int waveY = 30;
      for (int x = 0; x < 128; x += 4) {
        int idx = (x * currentSignal.dataLength) / 128;
        if (idx < currentSignal.dataLength) {
          int h = min(currentSignal.timings[idx] / 50, 20); // Escala
          if (idx % 2 == 0) {
            u8g2.drawVLine(x, waveY - h/2, h);
          }
        }
      }
      
      // Barra de progresso
      u8g2.drawFrame(10, 45, 108, 10);
      u8g2.drawBox(12, 47, (txProgress * 104) / 100, 6);
      
      // Status
      u8g2.setCursor(0, 62);
      u8g2.print("DOWN:Stop");
      u8g2.sendBuffer();
      
      // Executa transmissão (não bloqueante simulado)
      // Em implementação real, usar timer ou enviar em chunks
      transmitRawSignal(false);
      
      txPlaying = false; // Terminou uma sequência
      
      u8g2.clearBuffer();
      u8g2.drawStr(0, 10, "Transmissao OK");
      u8g2.setCursor(0, 30);
      u8g2.print("Freq: ");
      u8g2.print(currentSignal.frequency / 1000.0, 3);
      u8g2.print(" MHz");
      u8g2.setCursor(0, 45);
      u8g2.print("Pulsos: ");
      u8g2.print(currentSignal.dataLength);
      u8g2.drawStr(0, 62, "UP:Repetir B:Menu");
      u8g2.sendBuffer();
    }
    
    // Controles após transmissão
    if (buttonPressed(BTN_UP)) {
      txPlaying = true; // Repete
    }
    if (buttonPressed(BTN_DOWN)) {
      // Para e volta para idle
      txPlaying = false;
      ccState = CC_IDLE;
      cc1101TransceiverSetup();
    }
  }
}

// ==================== SCANNER SIMPLES (OPCIONAL) ====================

void cc1101ScannerSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "CC1101 Scanner");
  u8g2.sendBuffer();
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetRx();
}

void cc1101ScannerLoop() {
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
