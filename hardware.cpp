#include "config.h"

// Inicialização dos objetos
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
RF24 radio(NRF_CE, NRF_CSN);
Preferences prefs;

// Variáveis globais do sistema
bool jamming_active = false;
uint8_t current_menu_item = 0;
volatile uint8_t current_screen = 0;
volatile bool back_pressed = false;

// Variáveis do CC1101
CC1101Signal currentSignal;
bool hasSavedSignal = false;
SavedSignal savedSignals[MAX_SAVED_SIGNALS];
int selectedSignalSlot = 0;

// REMOVIDO: Interrupção de hardware que causava boot loop
// void IRAM_ATTR handleBackInterrupt() { ... }

// Carrega todos os sinais da NVS (memória interna ESP32)
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
        snprintf(savedSignals[i].name, sizeof(savedSignals[i].name), "Sinal %d", i + 1);
        hasSavedSignal = true;
        Serial.printf("Slot %d: Sinal carregado (%.3f MHz, %d pulsos)\n", 
                      i, savedSignals[i].signal.frequency / 1000.0, savedSignals[i].signal.dataLength);
      } else {
        savedSignals[i].active = false;
        memset(savedSignals[i].name, 0, sizeof(savedSignals[i].name));
      }
    } else {
      savedSignals[i].active = false;
      memset(savedSignals[i].name, 0, sizeof(savedSignals[i].name));
    }
  }
  
  // Seleciona o primeiro slot ativo como currentSignal
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
  delay(100); // Aguarda serial estabilizar
  
  Serial.println("\n========================================");
  Serial.println("nRF-BOX Pro - Inicializando...");
  Serial.println("========================================");
  
  // Verifica memória livre
  Serial.printf("Heap livre antes: %d bytes\n", ESP.getFreeHeap());
  
  // Inicializa NVS (memória interna do ESP32)
  if (!prefs.begin("nrfbox", false)) {
    Serial.println("NVS: Erro ao abrir namespace nrfbox");
  }
  delay(10);
  
  // Carrega slot selecionado das configurações
  selectedSignalSlot = prefs.getInt("selectedSlot", 0);
  if (selectedSignalSlot < 0 || selectedSignalSlot >= MAX_SAVED_SIGNALS) {
    selectedSignalSlot = 0;
  }
  Serial.printf("Slot selecionado: %d\n", selectedSignalSlot);
  
  // Inicializa I2C para OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(10);
  
  // Inicializa OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setContrast(128);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  delay(100);
  
  // Configura pinos dos botões com PULLUP
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  delay(10);
  
  // REMOVIDO: Interrupção que causava boot loop
  // Usar polling no loop principal é mais seguro
  
  // Inicializa SPI
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI);
  delay(10);
  
  // Inicializa nRF24
  if (!radio.begin()) {
    Serial.println("Falha nRF24!");
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
    Serial.println("nRF24: OK");
  }
  delay(100);
  
  // Inicializa CC1101
  ELECHOUSE_cc1101.setSpiPin(NRF_SCK, NRF_MISO, NRF_MOSI, CC_CSN);
  
  if (ELECHOUSE_cc1101.getCC1101()) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setGDO0(CC_GDO0);
    ELECHOUSE_cc1101.setGDO(CC_GDO0, CC_GDO2);
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setMHZ(433.92);
    u8g2.drawStr(0, 35, "CC1101: OK");
    Serial.println("CC1101: OK");
  } else {
    u8g2.drawStr(0, 35, "ERRO: CC1101");
    Serial.println("CC1101: ERRO");
  }
  u8g2.sendBuffer();
  delay(200);
  
  // Desliga rádios inicialmente
  radio.powerDown();
  ELECHOUSE_cc1101.setSidle();
  delay(50);
  
  // Carrega sinais salvos da NVS (memória interna)
  loadAllSignals();
  
  Serial.printf("Heap livre apos init: %d bytes\n", ESP.getFreeHeap());
  Serial.println("Inicializacao completa!");
  
  u8g2.clearBuffer();
}

// Debounce otimizado para botões
bool buttonPressed(uint8_t pin) {
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
  static const uint8_t pinMap[4] = {BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK};
  
  uint8_t idx = 255;
  for (int i = 0; i < 4; i++) {
    if (pinMap[i] == pin) {
      idx = i;
      break;
    }
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

// Função para verificar botão BACK via polling (sem interrupção)
void checkBackInterruptFlag() {
  static bool backWasPressed = false;
  bool backNow = (digitalRead(BTN_BACK) == LOW);
  
  if (backNow && !backWasPressed) {
    back_pressed = true;
    backWasPressed = true;
  }
  
  if (!backNow) {
    backWasPressed = false;
  }
  
  if (back_pressed) {
    back_pressed = false;
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    
    resetCC1101State();
    drawMenu();
  }
}
