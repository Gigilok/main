/*
 * nRF-BOX Pro - Main Firmware
 * 
 * Hardware: ESP32 + nRF24L01 + CC1101 + OLED 128x64 + 4 Botões
 * 
 * Funcionalidades:
 * - nRF24 Scanner (2.4GHz)
 * - nRF24 Jammer
 * - CC1101 Scanner (Sub-GHz)
 * - CC1101 Auto-Capture/Playback (Auto-detecta frequência)
 * - BLE Scanner
 * - WiFi Deauther
 * - Sour Apple (BLE Spam)
 * 
 * Botões:
 * UP (17)    - Navegar cima / Ajustar valores
 * DOWN (14)  - Navegar baixo / Ajustar valores  
 * SELECT (32)- Entrar / Confirmar / Gravar
 * BACK (33)  - Voltar ao menu (funciona em qualquer estado)
 */

// ============== INCLUDES NA ORDEM CORRETA ==============
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <RF24.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>  // Watchdog

// ============== CONFIGURAÇÕES DE PINOS ==============
// OLED I2C
#define OLED_SDA            21
#define OLED_SCL            22

// Botões (INPUT_PULLUP - ativo em LOW)
#define BTN_UP              17  // Cinza
#define BTN_DOWN            14  // Vermelho
#define BTN_SELECT          32  // Marrom
#define BTN_BACK            33  // Preto

// nRF24L01 (SPI)
#define NRF_CE              25  // Amarelo
#define NRF_CSN             26  // Azul
#define NRF_SCK             18  // Verde
#define NRF_MOSI            23  // Roxo
#define NRF_MISO            19  // Cinza

// CC1101 (SPI - compartilha SCK/MOSI/MISO)
#define CC_CSN              27  // Roxo
#define CC_GDO0             4   // Azul (Interrupção)
#define CC_GDO2             16  // Laranja

// ============== ESTRUTURAS DE DADOS ==============
struct CC1101Signal {
  uint32_t frequency;       // Frequência em kHz (ex: 433920)
  uint16_t dataLength;    // Número de transições
  int16_t rssi;           // RSSI no momento da captura
  uint32_t timestamp;      // millis() quando capturou
  uint16_t timings[512];   // Timings RAW em microssegundos
};

// ============== INSTÂNCIAS GLOBAIS ==============
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
RF24 radio(NRF_CE, NRF_CSN);
Preferences prefs;

// ============== VARIÁVEIS GLOBAIS ==============
bool jamming_active = false;
uint8_t current_menu_item = 0;
uint8_t current_screen = 0;      // 0=Menu, 1=Função ativa
volatile bool back_pressed = false;  // Flag de interrupção BACK
volatile bool system_reset_requested = false;

CC1101Signal currentSignal;
bool hasSavedSignal = false;

// ============== DECLARAÇÕES DE FUNÇÕES ==============
// Hardware
void setupHardware();
bool buttonPressed(uint8_t pin);
void IRAM_ATTR handleBackInterrupt();
void checkBackInterruptFlag();
void resetAllRadios();

// Menu
void initMenuSystem();
void drawMenu();
void handleMenu();
void runCurrentFunction();
void drawSplashScreen();

// nRF24
void nrfScannerSetup();
void nrfScannerLoop();
void nrfJammerSetup();
void nrfJammerLoop();

// CC1101
void cc1101ScannerSetup();
void cc1101ScannerLoop();
void cc1101TransceiverSetup();
void cc1101TransceiverLoop();
void resetCC1101State();

// BLE/WiFi
void bleScanSetup();
void bleScanLoop();
void wifiDeauthSetup();
void wifiDeauthLoop();
void sourAppleSetup();
void sourAppleLoop();

// Configurações
void settingsSetup();
void settingsLoop();

// ============== INTERRUPÇÕES ==============
void IRAM_ATTR handleBackInterrupt() {
  // Usa flag ao invés de código pesado na ISR
  back_pressed = true;
}

// ============== FUNÇÕES DE HARDWARE ==============
void resetAllRadios() {
  radio.powerDown();
  ELECHOUSE_cc1101.setSidle();
  delay(50);
}

void checkBackInterruptFlag() {
  if (back_pressed) {
    back_pressed = false;
    current_screen = 0;
    resetAllRadios();
    
    // Reseta estados específicos
    jamming_active = false;
    resetCC1101State();
    
    // Limpa display e volta ao menu
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    drawMenu();
    
    Serial.println("[BACK] Retornando ao menu");
  }
}

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
  
  // Detecta borda de descida (pressionamento com PULLUP)
  if (reading == LOW && lastState[idx] == HIGH) {
    if ((millis() - lastDebounceTime[idx]) > 150) {  // Debounce 150ms
      lastDebounceTime[idx] = millis();
      lastState[idx] = reading;
      
      // Se for BACK, desativa interrupção temporariamente
      if (pin == BTN_BACK) {
        detachInterrupt(digitalPinToInterrupt(BTN_BACK));
        back_pressed = false;  // Limpa flag da ISR
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

void drawSplashScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_10x20_tr);
  u8g2.drawStr(15, 20, "nRF-BOX");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(35, 35, "Pro Edition");
  u8g2.drawLine(0, 45, 128, 45);
  
  u8g2.setCursor(0, 58);
  u8g2.print("nRF24: ");
  u8g2.print(radio.isChipConnected() ? "OK" : "ERRO");
  
  u8g2.setCursor(65, 58);
  u8g2.print("CC1101: ");
  u8g2.print(ELECHOUSE_cc1101.getCC1101() ? "OK" : "ERRO");
  
  u8g2.sendBuffer();
  delay(1500);
}

void setupHardware() {
  Serial.begin(115200);
  Serial.println("\n\n=== nRF-BOX Pro Iniciando ===");
  
  // Inicializa NVS
  if (!prefs.begin("nrfbox", false)) {
    Serial.println("ERRO: Falha ao iniciar NVS!");
  }
  
  // I2C
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // OLED
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Iniciando...");
  u8g2.sendBuffer();
  
  // Botões com PULLUP
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  // Interrupção no BACK (emergência)
  attachInterrupt(digitalPinToInterrupt(BTN_BACK), handleBackInterrupt, FALLING);
  
  // SPI
  SPI.begin(NRF_SCK, NRF_MISO, NRF_MOSI);
  
  // nRF24
  if (radio.begin()) {
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setAutoAck(false);
    radio.stopListening();
    radio.powerDown();
    Serial.println("nRF24: OK");
  } else {
    Serial.println("nRF24: FALHA");
  }
  
  // CC1101
  ELECHOUSE_cc1101.setSpiPin(NRF_SCK, NRF_MISO, NRF_MOSI, CC_CSN);
  if (ELECHOUSE_cc1101.getCC1101()) {
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setGDO0(CC_GDO0);
    ELECHOUSE_cc1101.setGDO(CC_GDO0, CC_GDO2);
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.setModulation(0);
    ELECHOUSE_cc1101.setMHZ(433.92);
    ELECHOUSE_cc1101.setSidle();
    Serial.println("CC1101: OK");
  } else {
    Serial.println("CC1101: FALHA");
  }
  
  // Carrega sinal salvo
  size_t len = prefs.getBytesLength("signal");
  if (len == sizeof(CC1101Signal)) {
    prefs.getBytes("signal", &currentSignal, sizeof(CC1101Signal));
    if (currentSignal.frequency > 0 && currentSignal.dataLength > 0) {
      hasSavedSignal = true;
      Serial.print("Sinal carregado: ");
      Serial.print(currentSignal.frequency / 1000.0, 3);
      Serial.println(" MHz");
    }
  }
  
  // Splash screen
  drawSplashScreen();
  
  // Watchdog timer (30 segundos)
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  
  Serial.println("=== Sistema Pronto ===");
}

// ============== SISTEMA DE MENU ==============
#define MENU_ITEMS 8

const char* menu_names[MENU_ITEMS] = {
  "nRF24 Scanner",
  "nRF24 Jammer", 
  "CC1101 Scanner",
  "CC1101 Capture",  // Auto-detect + Gravar
  "BLE Scan",
  "WiFi Deauth",
  "Sour Apple",
  "Configuracoes"
};

void (*menu_setup_funcs[MENU_ITEMS])();
void (*menu_loop_funcs[MENU_ITEMS])();

void initMenuSystem() {
  menu_setup_funcs[0] = nrfScannerSetup;
  menu_loop_funcs[0] = nrfScannerLoop;
  
  menu_setup_funcs[1] = nrfJammerSetup;
  menu_loop_funcs[1] = nrfJammerLoop;
  
  menu_setup_funcs[2] = cc1101ScannerSetup;
  menu_loop_funcs[2] = cc1101ScannerLoop;
  
  menu_setup_funcs[3] = cc1101TransceiverSetup;
  menu_loop_funcs[3] = cc1101TransceiverLoop;
  
  menu_setup_funcs[4] = bleScanSetup;
  menu_loop_funcs[4] = bleScanLoop;
  
  menu_setup_funcs[5] = wifiDeauthSetup;
  menu_loop_funcs[5] = wifiDeauthLoop;
  
  menu_setup_funcs[6] = sourAppleSetup;
  menu_loop_funcs[6] = sourAppleLoop;
  
  menu_setup_funcs[7] = settingsSetup;
  menu_loop_funcs[7] = settingsLoop;
}

void drawMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  
  // Título com borda
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.setCursor(35, 9);
  u8g2.print("nRF-BOX Pro");
  u8g2.setDrawColor(1);
  
  // Itens (4 por vez)
  int start = (current_menu_item / 4) * 4;
  for (int i = 0; i < 4; i++) {
    int idx = start + i;
    if (idx >= MENU_ITEMS) break;
    
    int y = 16 + (i * 12);
    
    if (idx == current_menu_item) {
      u8g2.drawBox(0, y-1, 128, 11);
      u8g2.setDrawColor(0);
    }
    
    u8g2.setCursor(5, y+7);
    u8g2.print(menu_names[idx]);
    u8g2.setDrawColor(1);
  }
  
  // Indicador de página
  int total_pages = (MENU_ITEMS + 3) / 4;
  int current_page = current_menu_item / 4;
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(100, 63);
  u8g2.print(current_page + 1);
  u8g2.print("/");
  u8g2.print(total_pages);
  
  // Hint de navegação
  u8g2.setCursor(0, 63);
  u8g2.print("SEL:Ent B:Voltar");
  
  u8g2.sendBuffer();
}

void handleMenu() {
  if (buttonPressed(BTN_UP)) {
    if (current_menu_item > 0) {
      current_menu_item--;
      drawMenu();
      delay(50);
    }
  }
  
  if (buttonPressed(BTN_DOWN)) {
    if (current_menu_item < MENU_ITEMS - 1) {
      current_menu_item++;
      drawMenu();
      delay(50);
    }
  }
  
  if (buttonPressed(BTN_SELECT)) {
    current_screen = 1;
    back_pressed = false;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(10, 30, "Carregando...");
    u8g2.sendBuffer();
    
    resetAllRadios();
    
    if (menu_setup_funcs[current_menu_item]) {
      menu_setup_funcs[current_menu_item]();
    }
  }
}

void runCurrentFunction() {
  if (menu_loop_funcs[current_menu_item]) {
    menu_loop_funcs[current_menu_item]();
  }
}

// ============== SETUP E LOOP PRINCIPAL ==============
void setup() {
  setupHardware();
  initMenuSystem();
  drawMenu();
}

void loop() {
  // Reseta watchdog
  esp_task_wdt_reset();
  
  // Verifica interrupção de BACK primeiro (maior prioridade)
  checkBackInterruptFlag();
  
  // Loop principal
  if (current_screen == 0) {
    handleMenu();
  } else {
    runCurrentFunction();
  }
  
  delay(10);
}
