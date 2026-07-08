#include "config.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
RF24 radio(NRF_CE, NRF_CSN);
Preferences prefs;

bool jamming_active = false;
uint8_t current_menu_item = 0;
volatile uint8_t current_screen = 0;
volatile bool back_pressed = false;

CC1101Signal currentSignal;
bool hasSavedSignal = false;
SavedSignal savedSignals[MAX_SAVED_SIGNALS];
int selectedSignalSlot = 0;

void loadAllSignals() {
  hasSavedSignal = false;
  for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
    char keyName[16];
    snprintf(keyName, sizeof(keyName), "sig%d", i);
    size_t len = prefs.getBytesLength(keyName);
    if (len == sizeof(CC1101Signal)) {
      prefs.getBytes(keyName, &savedSignals[i].signal, sizeof(CC1101Signal));
      if (savedSignals[i].signal.frequency > 0 && savedSignals[i].signal.dataLength > 0) {
        savedSignals[i].active = true;
        snprintf(savedSignals[i].name, 16, "Sinal %d", i + 1);
        hasSavedSignal = true;
      } else {
        savedSignals[i].active = false;
        memset(savedSignals[i].name, 0, 16);
      }
    } else {
      savedSignals[i].active = false;
      memset(savedSignals[i].name, 0, 16);
    }
  }
  if (hasSavedSignal) {
    for (int i = 0; i < MAX_SAVED_SIGNALS; i++) {
      if (savedSignals[i].active) {
        currentSignal = savedSignals[i].signal;
        selectedSignalSlot = i;
        break;
      }
    }
  }
}

void setupHardware() {
  Serial.begin(115200);
  delay(100);
  
  if (!prefs.begin("nrfbox", false)) {
    Serial.println("NVS: Erro");
  }
  delay(10);
  
  selectedSignalSlot = prefs.getInt("selectedSlot", 0);
  if (selectedSignalSlot < 0 || selectedSignalSlot >= MAX_SAVED_SIGNALS) {
    selectedSignalSlot = 0;
  }
  
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(10);
  
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setContrast(128);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  delay(100);
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  delay(10);
  
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI);
  delay(10);
  
  if (!radio.begin()) {
    u8g2.drawStr(0, 25, "ERRO: nRF24L01");
    u8g2.sendBuffer();
    delay(500);
  } else {
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setAutoAck(false);
    radio.stopListening();
    u8g2.drawStr(0, 25, "nRF24: OK");
    u8g2.sendBuffer();
  }
  delay(100);
  
  ELECHOUSE_cc1101.setSpiPin(NRF_SCK, NRF_MISO, NRF_MOSI, CC_CSN);
  if (ELECHOUSE_cc1101.getCC1101()) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setGDO0(CC_GDO0);
    ELECHOUSE_cc1101.setGDO(CC_GDO0, CC_GDO2);
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setMHZ(433.92);
    u8g2.drawStr(0, 35, "CC1101: OK");
  } else {
    u8g2.drawStr(0, 35, "ERRO: CC1101");
  }
  u8g2.sendBuffer();
  delay(200);
  
  radio.powerDown();
  ELECHOUSE_cc1101.setSidle();
  delay(50);
  
  loadAllSignals();
  u8g2.clearBuffer();
}

bool buttonPressed(uint8_t pin) {
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
  static const uint8_t pinMap[4] = {BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK};
  
  uint8_t idx = 255;
  for (int i = 0; i < 4; i++) {
    if (pinMap[i] == pin) { idx = i; break; }
  }
  if (idx == 255) return false;
  
  bool reading = digitalRead(pin);
  
  if (reading == LOW && lastState[idx] == HIGH) {
    if ((millis() - lastDebounceTime[idx]) > 150) {
      lastDebounceTime[idx] = millis();
      lastState[idx] = reading;
      return true;
    }
  }
  
  if (reading == HIGH && lastState[idx] == LOW) {
    lastState[idx] = reading;
  }
  
  return false;
}

// BOTÃO BACK CORRIGIDO - Edge detection com tempo de pressão
void checkBackInterruptFlag() {
  static bool backWasPressed = false;
  static unsigned long backPressTime = 0;
  bool backNow = (digitalRead(BTN_BACK) == LOW);
  
  // Detecta borda de descida (pressionou)
  if (backNow && !backWasPressed) {
    backWasPressed = true;
    backPressTime = millis();
  }
  
  // Detecta borda de subida (soltou)
  if (!backNow && backWasPressed) {
    backWasPressed = false;
    // Só considera válido se pressionou por mais de 50ms e menos de 1s
    if ((millis() - backPressTime) > 50 && (millis() - backPressTime) < 1000) {
      back_pressed = true;
    }
  }
  
  // Executa ação do BACK
  if (back_pressed) {
    back_pressed = false;
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    resetCC1101State();
    drawMenu();
  }
}
