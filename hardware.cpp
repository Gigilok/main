#include "config.h"

// Inicialização dos objetos
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);
RF24 radio(NRF_CE, NRF_CSN);

// Variáveis globais
bool jamming_active = false;
uint8_t current_menu_item = 0;
uint8_t current_screen = 0;
CapturedSignal saved_signals[10];
int signal_count = 0;

void setupHardware() {
  Serial.begin(115200);
  
  // Inicializa I2C para OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Inicializa OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  
  // Configura pinos dos botões
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
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
    ELECHOUSE_cc1101.setCCMode(1); // Modo alta taxa
    ELECHOUSE_cc1101.setModulation(0); // ASK/OOK
    ELECHOUSE_cc1101.setMHZ(433.92); // Frequência padrão
    u8g2.drawStr(0, 35, "CC1101: OK");
  } else {
    u8g2.drawStr(0, 35, "ERRO: CC1101");
  }
  u8g2.sendBuffer();
  delay(1000);
  
  // Desliga rádios inicialmente
  radio.powerDown();
  ELECHOUSE_cc1101.setSidle();
  
  u8g2.clearBuffer();
}

// Debounce para botões corrigido
bool buttonPressed(uint8_t pin) {
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t pinMap[4] = {BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK};
  
  uint8_t idx = 0;
  for (int i = 0; i < 4; i++) {
    if (pinMap[i] == pin) {
      idx = i;
      break;
    }
  }
  
  bool reading = digitalRead(pin);
  bool pressed = false;
  
  // Se o estado mudou e já passou o tempo de debounce (150ms)
  if (reading != lastState[idx] && (millis() - lastDebounceTime[idx]) > 150) {
    lastDebounceTime[idx] = millis(); // Atualiza o cronômetro
    lastState[idx] = reading;         // Salva o novo estado
    
    // Como os botões usam INPUT_PULLUP, a leitura de um clique é LOW
    if (reading == LOW) {
      pressed = true;
    }
  }
  
  return pressed;
}
