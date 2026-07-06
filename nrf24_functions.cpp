#include "config.h"

// ==================== NRF24 SCANNER ====================
namespace {
  uint8_t values[128];
  bool scanning = false;
  int current_channel = 0;
}

void nrfScannerSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "nRF24 Scanner");
  u8g2.drawStr(0, 25, "Iniciando...");
  u8g2.sendBuffer();
  
  radio.powerUp();
  radio.startListening();
  radio.stopListening();
  
  memset(values, 0, sizeof(values));
  scanning = true;
}

void nrfScannerLoop() {
  if (!scanning) return;
  
  // Scan 2.4GHz band (canais 0-127)
  for (int i = 0; i < 128; i++) {
    radio.setChannel(i);
    radio.startListening();
    delayMicroseconds(100);
    radio.stopListening();
    
    if (radio.testCarrier()) {
      values[i] = min(values[i] + 5, 63);
    } else {
      values[i] = max(values[i] - 1, 0);
    }
  }
  
  // Desenha gráfico
  u8g2.clearBuffer();
  u8g2.drawStr(0, 8, "2.4GHz Spectrum");
  
  int barWidth = 128 / 64; // Mostra 64 canais por vez
  for (int i = 0; i < 64; i++) {
    int h = values[i] / 2;
    u8g2.drawVLine(i * 2, 54 - h, h);
  }
  
  u8g2.drawStr(0, 62, "0       32       64");
  u8g2.sendBuffer();
  
  // Botões de navegação
  if (buttonPressed(BTN_UP)) {
    // Ajusta sensibilidade ou modo
  }
}

// ==================== NRF24 JAMMER ====================
namespace {
  bool jamming = false;
  const byte jamText[16] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
                            0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
}

void nrfJammerSetup() {
  jamming = false;
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "nRF24 Jammer");
  u8g2.drawStr(0, 25, "Pressione SEL");
  u8g2.drawStr(0, 35, "para iniciar");
  u8g2.sendBuffer();
  
  radio.powerUp();
  radio.stopListening();
  radio.setAutoAck(false);
}

void nrfJammerLoop() {
  if (buttonPressed(BTN_SELECT)) {
    jamming = !jamming;
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "nRF24 Jammer");
    u8g2.setCursor(0, 30);
    u8g2.print(jamming ? "Status: ATIVO" : "Status: PARADO");
    u8g2.sendBuffer();
  }
  
  if (jamming) {
    // Jamming em múltiplos canais rapidamente
    for (int i = 0; i < 100; i += 5) {
      radio.setChannel(i);
      radio.write(jamText, 16, false);
    }
  }
}
