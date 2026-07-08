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

// ==================== VERSÃO DO FIRMWARE ====================
#define FIRMWARE_VERSION    "v2.1"
#define FIRMWARE_DATE       "2026-07-08"

// ==================== PINOS OLED (I2C) ====================
#define OLED_SDA            21
#define OLED_SCL            22

// ==================== PINOS BOTÕES ====================
#define BTN_UP              17
#define BTN_DOWN            14
#define BTN_SELECT          32
#define BTN_BACK            33

// ==================== PINOS nRF24L01 ====================
#define NRF_CE              25
#define NRF_CSN             26
#define NRF_SCK             18
#define NRF_MOSI            23
#define NRF_MISO            19

// ==================== PINOS CC1101 ====================
#define CC_CSN              27
#define CC_GDO0             4
#define CC_GDO2             16

// ==================== CONFIGURAÇÕES DE DISPLAY ====================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64

// ==================== CONFIGURAÇÕES CC1101 ====================
#define MAX_RAW_DATA        512
#define CAPTURE_TIMEOUT_MS  10000
#define RSSI_THRESHOLD      -75
#define MIN_PULSE_US        100

// Estrutura para sinais CC1101
struct CC1101Signal {
  uint32_t frequency;       // em kHz (ex: 433920)
  uint16_t dataLength;      // Número de transições
  int16_t rssi;             // RSSI no momento da captura
  uint32_t timestamp;       // millis() quando capturou
  uint8_t modulation;       // 0=ASK/OOK, 1=FSK, 2=GFSK
  uint16_t timings[MAX_RAW_DATA];  // Timings RAW em microssegundos
  uint8_t levels[MAX_RAW_DATA];    // Nível lógico (0=LOW/space, 1=HIGH/mark)
};

// Estrutura para múltiplos sinais salvos na NVS (memória interna ESP32)
#define MAX_SAVED_SIGNALS   5
struct SavedSignal {
  char name[16];
  CC1101Signal signal;
  bool active;
};

// Instâncias Globais
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern RF24 radio;
extern Preferences prefs;

// Variáveis globais do sistema
extern bool jamming_active;
extern uint8_t current_menu_item;
extern volatile uint8_t current_screen;
extern volatile bool back_pressed;

// CC1101 Signal global
extern CC1101Signal currentSignal;
extern bool hasSavedSignal;
extern SavedSignal savedSignals[MAX_SAVED_SIGNALS];
extern int selectedSignalSlot;

// ==================== DECLARAÇÃO DE FUNÇÕES ====================
bool buttonPressed(uint8_t pin);
void checkBackInterruptFlag(void);
void setupHardware(void);
void drawMenu(void);
void handleMenu(void);
void runCurrentFunction(void);
void initMenuSystem(void);
void resetCC1101State(void);

// nRF24
void nrfScannerSetup(void);
void nrfScannerLoop(void);
void nrfJammerSetup(void);
void nrfJammerLoop(void);

// CC1101
void cc1101ScannerSetup(void);
void cc1101ScannerLoop(void);
void cc1101TransceiverSetup(void);
void cc1101TransceiverLoop(void);
void cc1101BruteForceSetup(void);
void cc1101BruteForceLoop(void);
void cc1101FreqSweepSetup(void);
void cc1101FreqSweepLoop(void);

// BLE/WiFi
void bleScanSetup(void);
void bleScanLoop(void);
void wifiDeauthSetup(void);
void wifiDeauthLoop(void);
void sourAppleSetup(void);
void sourAppleLoop(void);

// Settings
void settingsSetup(void);
void settingsLoop(void);

#endif
