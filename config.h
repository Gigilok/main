#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RF24.h>
#include <ELECHOUSE_CC1101.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <EEPROM.h>

// ==================== PINOS OLED (I2C) ====================
#define OLED_SDA            21  // Padrão ESP32 - ajuste se necessário
#define OLED_SCL            22  // Padrão ESP32 - ajuste se necessário

// ==================== PINOS BOTÕES ====================
#define BTN_UP              17  // Cinza - Cima
#define BTN_DOWN            14  // Vermelho - Baixo  
#define BTN_SELECT          32  // Marrom - Selecionar/Enter
#define BTN_BACK            33  // Preto - Voltar/Menu

// ==================== PINOS nRF24L01 (1 Módulo) ====================
#define NRF_CE              25  // Amarelo
#define NRF_CSN             26  // Azul
#define NRF_SCK             18  // Verde (SPI)
#define NRF_MOSI            23  // Roxo (SPI)
#define NRF_MISO            19  // Cinza (SPI)

// ==================== PINOS CC1101 ====================
#define CC_CSN              27  // Roxo
#define CC_GDO0             4   // Azul (Interrupção/Recepção)
#define CC_GDO2             16  // Laranja (Opcional/Status)
// CC1101 compartilha SCK(18), MOSI(23), MISO(19) com nRF24

// ==================== CONFIGURAÇÕES ====================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64

// Instâncias Globais
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern RF24 radio;

// Estados do sistema
extern bool jamming_active;
extern uint8_t current_menu_item;
extern uint8_t current_screen; // 0=Menu, 1=Função Ativa

// Estrutura para sinais Sub-GHz capturados
struct CapturedSignal {
  uint32_t frequency;
  byte data[64];
  int length;
  int rssi;
  unsigned long timestamp;
  bool valid;
};

extern CapturedSignal saved_signals[10];
extern int signal_count;

#endif
