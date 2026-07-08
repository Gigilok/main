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
#include <Preferences.h>

#define FIRMWARE_VERSION    "v2.3"
#define FIRMWARE_DATE       "2026-07-08"

#define OLED_SDA            21
#define OLED_SCL            22
#define BTN_UP              17
#define BTN_DOWN            14
#define BTN_SELECT          32
#define BTN_BACK            33
#define NRF_CE              25
#define NRF_CSN             26
#define NRF_SCK             18
#define NRF_MOSI            23
#define NRF_MISO            19
#define CC_CSN              27
#define CC_GDO0             4
#define CC_GDO2             16

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define MAX_RAW_DATA        512
#define CAPTURE_TIMEOUT_MS  10000
#define RSSI_THRESHOLD      -75
#define MIN_PULSE_US        100

struct CC1101Signal {
  uint32_t frequency;
  uint16_t dataLength;
  int16_t rssi;
  uint32_t timestamp;
  uint8_t modulation;
  uint16_t timings[MAX_RAW_DATA];
  uint8_t levels[MAX_RAW_DATA];
};

#define MAX_SAVED_SIGNALS   5
struct SavedSignal {
  char name[16];
  CC1101Signal signal;
  bool active;
};

struct RCSwitchProtocol {
  const char* name;
  uint16_t pulseLength;
  uint8_t syncFactor[2];
  uint8_t zero[2];
  uint8_t one[2];
  bool inverted;
};

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern RF24 radio;
extern Preferences prefs;

extern bool jamming_active;
extern uint8_t current_menu_item;
extern volatile uint8_t current_screen;
extern volatile bool back_pressed;

extern CC1101Signal currentSignal;
extern bool hasSavedSignal;
extern SavedSignal savedSignals[MAX_SAVED_SIGNALS];
extern int selectedSignalSlot;

bool buttonPressed(uint8_t pin);
void checkBackInterruptFlag(void);
void setupHardware(void);
void drawMenu(void);
void handleMenu(void);
void runCurrentFunction(void);
void initMenuSystem(void);
void resetCC1101State(void);
void loadAllSignals(void);
void saveSignalToSlot(int slot, const CC1101Signal& sig);
void clearSignalSlot(int slot);

void nrfScannerSetup(void);
void nrfScannerLoop(void);
void nrfJammerSetup(void);
void nrfJammerLoop(void);
void cc1101ScannerSetup(void);
void cc1101ScannerLoop(void);
void cc1101TransceiverSetup(void);
void cc1101TransceiverLoop(void);
void cc1101BruteForceSetup(void);
void cc1101BruteForceLoop(void);
void cc1101FreqSweepSetup(void);
void cc1101FreqSweepLoop(void);
void bleScanSetup(void);
void bleScanLoop(void);
void wifiDeauthSetup(void);
void wifiDeauthLoop(void);
void sourAppleSetup(void);
void sourAppleLoop(void);
void settingsSetup(void);
void settingsLoop(void);

#endif
