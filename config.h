#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <U8g2lib.h>
#include <RF24.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <string>

#define FIRMWARE_VERSION "MadCat OS v2.0"
#define FIRMWARE_SHORT   "v2.0"

#define OLED_SDA 21
#define OLED_SCL 22
#define NRF_CE   25
#define NRF_CSN  26
#define NRF_SCK  18
#define NRF_MISO 19
#define NRF_MOSI 23
#define CC_CSN   27
#define CC_GDO0  4
#define CC_GDO2  16
#define BTN_UP     17
#define BTN_DOWN   14
#define BTN_SELECT 32
#define BTN_BACK   33

#define MAX_SAVED_SIGNALS 5
#define MAX_RAW_DATA 500
#define RSSI_THRESHOLD -80
#define CAPTURE_TIMEOUT_MS 5000
#define BUTTON_DEBOUNCE_MS 180

struct CC1101Signal {
  uint32_t frequency;
  uint16_t timings[MAX_RAW_DATA];
  uint8_t levels[MAX_RAW_DATA];
  int dataLength;
  int modulation;
  int rssi;
  uint32_t timestamp;
};

struct SavedSignal {
  CC1101Signal signal;
  bool active;
  char name[16];
};

struct RCSwitchProtocol {
  const char* name;
  uint16_t pulseLength;
  uint8_t syncFactor[2];
  uint8_t zero[2];
  uint8_t one[2];
  bool inverted;
};

struct DecodedSignal {
  uint32_t code;
  int protocolIndex;
  int bits;
  bool valid;
};

struct SubGHzEntry {
  const char* name;
  float frequency;
  int protocolIndex;
  uint32_t code;
  int bits;
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

namespace cc1101 {
  extern bool rolljamActive;
  extern uint32_t rolljamCode1;
  extern uint32_t rolljamCode2;
  extern bool rolljamHasCode1;
  extern bool rolljamHasCode2;
  extern int rolljamStep;
  extern unsigned long rolljamTimer;
  extern int rolljamProtocol1;
  extern int rolljamProtocol2;
  extern int rolljamBits1;
  extern int rolljamBits2;
  extern bool rollingPwnActive;
  extern int rollingPwnStep;
  extern int rollingPwnCounter;
  extern unsigned long rollingPwnTimer;
  extern uint32_t capturedCodes[10];
  extern int capturedCodeCount;
  extern int capturedProtocols[10];
  extern int capturedBits[10];
  extern int32_t rollingPwnPattern;
  extern int rollingPwnPatternType;
  enum BFMode { BF_MODE_COMMON, BF_MODE_DEBRUIJN, BF_MODE_FULL, BF_MODE_ROLLJAM, BF_MODE_ROLLINGPWN, BF_MODE_SMART };
  extern BFMode bfMode;
  enum BFState { BF_IDLE, BF_RUNNING, BF_PAUSED };
  extern BFState bfState;
  extern int bfFreqIndex;
  extern int bfProtocolIndex;
  extern uint32_t bfCode;
  extern unsigned long bfStartTime;
  extern int bfCodesSent;
}

extern const uint32_t SCAN_FREQUENCIES[];
extern const int NUM_FREQS;
extern const uint32_t BRUTE_FREQS[];
extern const int NUM_BRUTE_FREQS;
extern const RCSwitchProtocol protocols[];
extern const int NUM_PROTOCOLS;
extern const uint32_t COMMON_CODES[];
extern const int NUM_COMMON_CODES;
extern const SubGHzEntry subGHzDatabase[];
extern const int SUBGHZ_DB_COUNT;

enum CCState { CC_IDLE, CC_SCANNING, CC_CAPTURING, CC_TRANSMITTING };

void setupHardware();
bool buttonPressed(uint8_t pin);
void checkBackInterruptFlag();
void loadAllSignals();
void saveSignalToSlot(int slot, const CC1101Signal& sig);
void clearSignalSlot(int slot);
void initMenuSystem();
void drawMenu();
void handleMenu();
void runCurrentFunction();
void drawFunctionHeader(const char* title);
void nrfScannerSetup();
void nrfScannerLoop();
void nrfJammerSetup();
void nrfJammerLoop();
void resetCC1101State();
void startCapture(uint32_t frequency);
void stopCapture();
void transmitRawSignal();
void transmitBit(const RCSwitchProtocol& p, bool bit);
void transmitSync(const RCSwitchProtocol& p);
void transmitCode(uint32_t code, int bits, const RCSwitchProtocol& p);
void jamChannel(uint32_t freqKHz);
uint32_t autoDetectFrequency();
CCState getCCState();
void setCCState(CCState state);
bool isTXPlaying();
void setTXPlaying(bool val);
int getTXProgress();
int getCaptureIndex();
uint32_t getCaptureStartTime();
uint32_t getDetectedFrequency();
void setDetectedFrequency(uint32_t freq);
DecodedSignal decodeCapturedSignal();
void analyzeProtocol();
void rollJamAttackStep();
void rollingPwnAttackStep();
void cc1101BruteForceSetup();
void cc1101BruteForceLoop();
void cc1101ScannerSetup();
void cc1101ScannerLoop();
void cc1101TransceiverSetup();
void cc1101TransceiverLoop();
void cc1101FreqSweepSetup();
void cc1101FreqSweepLoop();
void bleScanSetup();
void bleScanLoop();
void wifiDeauthSetup();
void wifiDeauthLoop();
void wifiDeauthAttackSetup();
void wifiDeauthAttackLoop();
void sourAppleSetup();
void sourAppleLoop();
void settingsSetup();
void settingsLoop();
void signalManagerSetup();
void signalManagerLoop();
void infoSetup();
void infoLoop();
void subGHzDBSetup();
void subGHzDBLoop();

#include "icons.h"

#endif
