#include "config.h"


// Forward declarations
void drawMenu(void);

#define MENU_ITEMS 8

const char* menu_names[MENU_ITEMS] = {
  "nRF24 Scanner",
  "nRF24 Jammer", 
  "CC1101 Scanner",
  "CC1101 Capture",  // Nova função com auto-detect
  "BLE Scan",
  "WiFi Deauth",
  "Sour Apple",
  "Configuracoes"
};

// Ponteiros para funções
void (*menu_setup_funcs[MENU_ITEMS])();
void (*menu_loop_funcs[MENU_ITEMS])();

// Variáveis do menu
uint8_t current_menu_item = 0;
uint8_t current_screen = 0;
volatile bool back_pressed = false;  // Flag global para BACK

// Declarações externas
extern void nrfScannerSetup();
extern void nrfScannerLoop();
extern void nrfJammerSetup();
extern void nrfJammerLoop();
extern void cc1101ScannerSetup();
extern void cc1101ScannerLoop();
extern void cc1101TransceiverSetup();
extern void cc1101TransceiverLoop();
extern void bleScanSetup();
extern void bleScanLoop();
extern void wifiDeauthSetup();
extern void wifiDeauthLoop();
extern void sourAppleSetup();
extern void sourAppleLoop();
extern void settingsSetup();
extern void settingsLoop();

// ==================== FUNÇÃO DE DEBOUNCE MELHORADA ====================

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
  
  // Detecta borda de descida (pressionamento)
  if (reading == LOW && lastState[idx] == HIGH) {
    if ((millis() - lastDebounceTime[idx]) > 150) { // Debounce 150ms
      lastDebounceTime[idx] = millis();
      lastState[idx] = reading;
      return true;
    }
  }
  
  // Atualiza estado quando solta
  if (reading == HIGH && lastState[idx] == LOW) {
    lastState[idx] = reading;
  }
  
  return false;
}

// ==================== CHECK ESPECIAL PARA BACK ====================

void checkBackButton() {
  if (buttonPressed(BTN_BACK)) {
    back_pressed = true;
    current_screen = 0;
    
    // Desativa todos os rádios ao voltar
    radio.powerDown();
    ELECHOUSE_cc1101.setSidle();
    
    // Reseta estados internos
    extern void resetCC1101State();
    resetCC1101State();
    
    // Redesenha menu
    drawMenu();
    
    // Pequeno delay para evitar bounce
    delay(200);
  }
}

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
  
  // Título
  u8g2.drawStr(30, 10, "nRF-BOX Pro");
  u8g2.drawHLine(0, 12, 128);
  
  // Itens do menu (mostra 4 por vez)
  int start = (current_menu_item / 4) * 4;
  for (int i = 0; i < 4; i++) {
    int idx = start + i;
    if (idx >= MENU_ITEMS) break;
    
    int y = 25 + (i * 12);
    
    if (idx == current_menu_item) {
      u8g2.drawBox(0, y-9, 128, 11);
      u8g2.setDrawColor(0);
    }
    
    u8g2.setCursor(5, y);
    u8g2.print(menu_names[idx]);
    u8g2.setDrawColor(1);
  }
  
  // Indicador de página
  int total_pages = (MENU_ITEMS + 3) / 4;
  int current_page = current_menu_item / 4;
  u8g2.setCursor(110, 60);
  u8g2.print(current_page + 1);
  u8g2.print("/");
  u8g2.print(total_pages);
  
  // Hint de navegação
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setCursor(0, 63);
  u8g2.print("SEL:Entrar B:Voltar");
  
  u8g2.sendBuffer();
}

void handleMenu() {
  // Verifica BACK primeiro
  checkBackButton();
  
  if (buttonPressed(BTN_UP)) {
    if (current_menu_item > 0) {
      current_menu_item--;
      drawMenu();
      delay(100); // Feedback visual
    }
  }
  
  if (buttonPressed(BTN_DOWN)) {
    if (current_menu_item < MENU_ITEMS - 1) {
      current_menu_item++;
      drawMenu();
      delay(100);
    }
  }
  
  if (buttonPressed(BTN_SELECT)) {
    current_screen = 1;
    back_pressed = false;
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(10, 30, "Carregando...");
    u8g2.sendBuffer();
    
    if (menu_setup_funcs[current_menu_item]) {
      menu_setup_funcs[current_menu_item]();
    }
  }
}

void runCurrentFunction() {
  // VERIFICA BACK EM TODOS OS ESTADOS - CORREÇÃO PRINCIPAL
  checkBackButton();
  
  // Se voltou para menu, não executa a função
  if (current_screen == 0) return;
  
  // Executa função atual
  if (menu_loop_funcs[current_menu_item]) {
    menu_loop_funcs[current_menu_item]();
  }
}
