#include <ELECHOUSE_CC1101_SRC_DRV.h> // Force o include aqui primeiro
#include "config.h"

// Declarações externas das funções do menu
extern void setupHardware();
extern void initMenuSystem();
extern void drawMenu();
extern void handleMenu();
extern void runCurrentFunction();

void setup() {
  setupHardware();
  initMenuSystem();
  drawMenu();
}

void loop() {
  if (current_screen == 0) {
    handleMenu();
  } else {
    runCurrentFunction();
  }
  delay(10); // Pequeno delay para estabilidade
}
