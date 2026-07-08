#include "config.h"

// ============================================
// CC1101 SCANNER
// ============================================

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

// ============================================
// CC1101 CAPTURE / TRANSCEIVER
// ============================================

// DECLARACAO ANTECIPADA (prototipo) para uso no loop
void cc1101TransceiverSetup();

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
  
  CCState ccState = getCCState();
  
  if (ccState == CC_IDLE) {
    if (buttonPressed(BTN_SELECT)) {
      setCCState(CC_SCANNING);
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
        setCCState(CC_IDLE);
        cc1101TransceiverSetup();
        return;
      }
      setDetectedFrequency(detectedFreq);
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
      setCCState(CC_CAPTURING);
    }
    else if (buttonPressed(BTN_UP) && hasSavedSignal) {
      setCCState(CC_TRANSMITTING);
      setTXPlaying(true);
    }
    else if (buttonPressed(BTN_DOWN)) {
      current_menu_item = 4;
      current_screen = 1;
      cc1101BruteForceSetup();
      return;
    }
  }
  else if (ccState == CC_CAPTURING) {
    uint32_t elapsed = millis() - getCaptureStartTime();
    int remaining = (CAPTURE_TIMEOUT_MS - elapsed) / 1000;
    int progressPercent = (elapsed * 100) / CAPTURE_TIMEOUT_MS;
    u8g2.setCursor(0, 42);
    u8g2.print("Tempo: ");
    u8g2.print(remaining);
    u8g2.print("s   ");
    u8g2.setCursor(0, 55);
    u8g2.print("Prog: ");
    u8g2.print(progressPercent);
    u8g2.print("%");
    u8g2.sendBuffer();
    if (elapsed >= CAPTURE_TIMEOUT_MS) {
      stopCapture();
      if (currentSignal.dataLength > 10) {
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
      setCCState(CC_IDLE);
      cc1101TransceiverSetup();
    }
  }
  else if (ccState == CC_TRANSMITTING) {
    if (isTXPlaying()) {
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
      u8g2.drawBox(12, 54, (getTXProgress() * 104) / 100, 4);
      u8g2.setCursor(0, 62);
      u8g2.print("Transmitindo...");
      u8g2.sendBuffer();
      transmitRawSignal();
      setTXPlaying(false);
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
    if (buttonPressed(BTN_UP)) setTXPlaying(true);
  }
}

// DEFINICAO da funcao (depois do loop que a usa)
void cc1101TransceiverSetup() {
  setCCState(CC_IDLE);
  setTXPlaying(false);
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

// ============================================
// CC1101 BRUTE FORCE SETUP
// ============================================

void cc1101BruteForceSetup() {
  cc1101::bfState = cc1101::BF_IDLE;
  cc1101::bfFreqIndex = 0;
  cc1101::bfProtocolIndex = 0;
  cc1101::bfCode = 0;
  cc1101::bfCodesSent = 0;
  cc1101::rolljamActive = false;
  cc1101::rollingPwnActive = false;
  cc1101::rolljamStep = 0;
  cc1101::rolljamHasCode1 = false;
  cc1101::rolljamHasCode2 = false;
  cc1101::rollingPwnStep = 0;
  cc1101::capturedCodeCount = 0;
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

// ============================================
// CC1101 FREQUENCY SWEEP
// ============================================

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
