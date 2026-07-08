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

// Interrupção de hardware para o botão Voltar
void IRAM_ATTR handleBackInterrupt() {
  back_pressed = true;
  current_screen = 0;
}

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
  
  // Inicializa NVS (memória interna do ESP32)
  if (!prefs.begin("nrfbox", false)) {
    Serial.println("NVS: Erro ao abrir namespace nrfbox");
  }
  
  // Carrega slot selecionado das configurações
  selectedSignalSlot = prefs.getInt("selectedSlot", 0);
  if (selectedSignalSlot < 0 || selectedSignalSlot >= MAX_SAVED_SIGNALS) {
    selectedSignalSlot = 0;
  }
  
  // Inicializa I2C para OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Inicializa OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setContrast(128);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  
  // Configura pinos dos botões com PULLUP
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  // Anexa interrupção ao pino BACK
  attachInterrupt(digitalPinToInterrupt(BTN_BACK), handleBackInterrupt, FALLING);
  
  // Inicializa SPI
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI);
  
  // Inicializa nRF24
  if (!radio.begin()) {
    Serial.println("Falha nRF24!");
    u8g2.drawStr(0, 25, "ERRO: nRF24L01");
    u8g2.sendBuffer();
    delay(2000);
  } else {
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setAutoAck(false);
    radio.stopListening();
    u8g2.drawStr(0, 25, "nRF24: OK");
    u8g2.sendBuffer();
  }
  
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
  } else {
    u8g2.drawStr(0, 35, "ERRO: CC1101");
  }
  u8g2.sendBuffer();
  delay(800);
  
  // Desliga rádios inicialmente
  radio.powerDown();
  ELECHOUSE_cc1101.setSidle();
  
  // Carrega sinais salvos da NVS (memória interna)
  loadAllSignals();
  
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

// Função para verificar flag de interrupção
void checkBackInterruptFlag() {
  if (back_pressed) {
    back_pressed = false;
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    
    resetCC1101State();
    drawMenu();
  }
}
