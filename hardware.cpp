#include "config.h"

// Inicialização dos objetos
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8X2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
RF24 radio(NRF_CE, NRF_CSN);
Preferences prefs;  // Adicionado para NVS

// Variáveis globais do sistema
bool jamming_active = false;
uint8_t current_menu_item = 0;
uint8_t current_screen = 0;
volatile bool back_pressed = false;  // Flag para comunicação entre ISRs

// Variáveis do CC1101 (novo sistema)
CC1101Signal currentSignal;
bool hasSavedSignal = false;

// Interrupção de hardware para o botão Voltar (modo emergência)
// Usa flag ao invés de restart brusco
void IRAM_ATTR handleBackInterrupt() {
  back_pressed = true;
  current_screen = 0;  // Força volta ao menu
}

void setupHardware() {
  Serial.begin(115200);
  
  // Inicializa NVS (memória persistente)
  prefs.begin("nrfbox", false);
  
  // Inicializa I2C para OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Inicializa OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  
  // Configura pinos dos botões com PULLUP
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  // Anexa interrupção ao pino BACK (modo emergência/fallback)
  // FALLING porque com PULLUP, pressionar = LOW
  attachInterrupt(digitalPinToInterrupt(BTN_BACK), handleBackInterrupt, FALLING);
  
  // Inicializa SPI
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI);
  
  // Inicializa nRF24 (1 módulo apenas)
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
    ELECHOUSE_cc1101.setGDO(CC_GDO0, CC_GDO2);  // Configura ambos GDOs
    ELECHOUSE_cc1101.setCCMode(1);              // Modo alta taxa
    ELECHOUSE_cc1101.setModulation(0);          // ASK/OOK
    ELECHOUSE_cc1101.setMHZ(433.92);            // Frequência padrão
    u8g2.drawStr(0, 35, "CC1101: OK");
  } else {
    u8g2.drawStr(0, 35, "ERRO: CC1101");
  }
  u8g2.sendBuffer();
  delay(1000);
  
  // Desliga rádios inicialmente
  radio.powerDown();
  ELECHOUSE_cc1101.setSidle();
  
  // Carrega sinal salvo (se existir)
  size_t len = prefs.getBytesLength("signal");
  if (len == sizeof(CC1101Signal)) {
    prefs.getBytes("signal", &currentSignal, sizeof(CC1101Signal));
    if (currentSignal.frequency > 0 && currentSignal.dataLength > 0) {
      hasSavedSignal = true;
    }
  }
  
  u8g2.clearBuffer();
}

// Debounce otimizado para botões
bool buttonPressed(uint8_t pin) {
  static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
  static uint8_t pinMap[4] = {BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK};
  
  uint8_t idx = 255;
  for (int i = 0; i < 4; i++) {
    if (pinMap[i] == pin) {
      idx = i;
      break;
    }
  }
  
  if (idx == 255) return false;
  
  bool reading = digitalRead(pin);
  
  // Detecta borda de descida (LOW com PULLUP = pressionado)
  if (reading == LOW && lastState[idx] == HIGH) {
    if ((millis() - lastDebounceTime[idx]) > 150) {  // Debounce 150ms
      lastDebounceTime[idx] = millis();
      lastState[idx] = reading;
      
      // Se for o botão BACK, desativa interrupção temporariamente para evitar duplo trigger
      if (pin == BTN_BACK) {
        detachInterrupt(digitalPinToInterrupt(BTN_BACK));
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_BACK), handleBackInterrupt, FALLING);
      }
      
      return true;
    }
  }
  
  // Atualiza estado quando solta
  if (reading == HIGH && lastState[idx] == LOW) {
    lastState[idx] = reading;
  }
  
  return false;
}

// Função para verificar flag de interrupção (chamar no loop principal)
void checkBackInterruptFlag() {
  if (back_pressed) {
    back_pressed = false;
    current_screen = 0;
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    
    // Redesenha menu se necessário
    extern void drawMenu();
    drawMenu();
  }
}
