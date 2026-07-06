#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RF24.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <Preferences.h>

// ==================== PINOS OLED (I2C) ====================
#define OLED_SDA            21
#define OLED_SCL            22

// ==================== PINOS BOTÕES ====================
#define BTN_UP              17  // Cinza - Cima
#define BTN_DOWN            14  // Vermelho - Baixo  
#define BTN_SELECT          32  // Marrom - Selecionar/Enter
#define BTN_BACK            33  // Preto - Voltar/Menu (CORRIGIDO)

// ==================== PINOS nRF24L01 ====================
#define NRF_CE              25  // Amarelo
#define NRF_CSN             26  // Azul
#define NRF_SCK             18  // Verde (SPI)
#define NRF_MOSI            23  // Roxo (SPI)
#define NRF_MISO            19  // Cinza (SPI)

// ==================== PINOS CC1101 ====================
#define CC_CSN              27  // Roxo
#define CC_GDO0             4   // Azul (Interrupção/Recepção)
#define CC_GDO2             16  // Laranja (Opcional/Status)

// ==================== CONFIGURAÇÕES ====================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64

// Estrutura para sinais CC1101
struct CC1101Signal {
  uint32_t frequency;       // em kHz (ex: 433920)
  uint16_t dataLength;      // Número de transições
  int16_t rssi;             // RSSI no momento da captura
  uint32_t timestamp;       // millis() quando capturou
  uint8_t modulation;       // 0=ASK/OOK, 1=FSK, 2=GFSK
  uint16_t timings[512];    // Timings RAW em microssegundos
};

// Instâncias Globais
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern RF24 radio;
extern Preferences prefs;

// Variáveis globais do sistema
extern bool jamming_active;
extern uint8_t current_menu_item;
extern uint8_t current_screen;
extern volatile bool back_pressed;  // Flag para botão back

// CC1101 Signal global
extern CC1101Signal currentSignal;
extern bool hasSavedSignal;

// ==================== DECLARAÇÃO DE FUNÇÕES ====================
bool buttonPressed(uint8_t pin); // <--- CORREÇÃO ADICIONADA AQUI

#endif
