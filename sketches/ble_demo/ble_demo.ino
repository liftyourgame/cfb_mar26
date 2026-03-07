// ble_demo.ino — Two-way BLE communication for ESP32-C3
// Advertises as "humn.au", accepts write commands from phone,
// sends button-press and heartbeat notifications back.
//
// Phone side: use free LightBlue or nRF Connect app (no custom iOS app).
//
// Commands (write to Command characteristic):
//   LED:RED  LED:GREEN  LED:BLUE  LED:OFF  — set RGB LED color
//   MSG:<text>                              — show text on OLED
//
// Notifications (subscribe to Notify characteristic):
//   PONG         — reply to PING command
//   HB:<seconds> — heartbeat every 5 s

#include <NimBLEDevice.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// ----- Hardware pins -----
#define NEOPIXEL_PIN  2

// ----- OLED (SSD1306 72x40, I2C: SDA=GPIO5, SCL=GPIO6) -----
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ----- NeoPixel RGB LED -----
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ----- BLE UUIDs -----
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CMD_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define NOTIFY_CHAR_UUID    "d5a41e2b-90f7-4e56-8c3a-6f2d7bc0e943"

// ----- BLE state -----
NimBLEServer*         pServer    = nullptr;
NimBLECharacteristic* pCmdChar   = nullptr;
NimBLECharacteristic* pNotify    = nullptr;
bool deviceConnected    = false;
bool prevConnected      = false;

// ----- Display state -----
char statusLine[32]     = "BLE: Adv";
char messageLine[32]    = "";
unsigned long msgExpiry  = 0;

// ----- Heartbeat -----
unsigned long lastHB     = 0;
const unsigned long HB_INTERVAL = 5000;

// Forward declarations
void showMsg(const char* msg);
void updateDisplay();
void sendNotification(const char* msg);

// =========================================================
// BLE callbacks
// =========================================================

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.println("BLE client connected");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.printf("BLE client disconnected (reason %d)\n", reason);
  }
};

class CmdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    std::string val = pChar->getValue();
    if (val.empty()) return;

    Serial.printf("CMD: %s\n", val.c_str());

    // --- LED commands ---
    if (val == "LED:RED")        { pixel.setPixelColor(0, pixel.Color(255, 0, 0));   pixel.show(); showMsg("LED Red");    }
    else if (val == "LED:GREEN") { pixel.setPixelColor(0, pixel.Color(0, 255, 0));   pixel.show(); showMsg("LED Green");  }
    else if (val == "LED:BLUE")  { pixel.setPixelColor(0, pixel.Color(0, 0, 255));   pixel.show(); showMsg("LED Blue");   }
    else if (val == "LED:OFF")   { pixel.setPixelColor(0, 0);                        pixel.show(); showMsg("LED Off");    }

    // --- MSG command ---
    else if (val.rfind("MSG:", 0) == 0) {
      String text = String(val.c_str() + 4);
      text.trim();
      if (text.length() > 0) {
        showMsg(text.c_str());
      }
    }

    // --- PING → PONG round-trip ---
    else if (val == "PING") {
      sendNotification("PONG");
      showMsg("PONG!");
    }

    else {
      Serial.printf("Unknown command: %s\n", val.c_str());
      showMsg("???");
    }
  }
};

// =========================================================
// Helpers
// =========================================================

void showMsg(const char* msg) {
  strncpy(messageLine, msg, sizeof(messageLine) - 1);
  messageLine[sizeof(messageLine) - 1] = '\0';
  msgExpiry = millis() + 3000;
  updateDisplay();
}

void updateDisplay() {
  // Status line
  if (deviceConnected) {
    snprintf(statusLine, sizeof(statusLine), "BLE: Connected");
  } else {
    snprintf(statusLine, sizeof(statusLine), "BLE: Advertising");
  }

  bool showingMsg = (messageLine[0] != '\0' && millis() < msgExpiry);

  u8g2.clearBuffer();

  // Top line — branding
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(2, 14, "humn.au");

  // Bottom line — status or temporary message
  u8g2.setFont(u8g2_font_6x10_tr);
  if (showingMsg) {
    u8g2.drawStr(2, 34, messageLine);
  } else {
    u8g2.drawStr(2, 34, statusLine);
  }

  u8g2.sendBuffer();
}

void sendNotification(const char* msg) {
  if (!deviceConnected) return;
  pNotify->setValue((const uint8_t*)msg, strlen(msg));
  pNotify->notify();
  Serial.printf("NOTIFY: %s\n", msg);
}

// =========================================================
// Setup
// =========================================================

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== BLE Demo ===");

  // OLED
  u8g2.begin();
  u8g2.setContrast(255);
  updateDisplay();
  Serial.println("OLED ready");

  // NeoPixel — start off
  pixel.begin();
  pixel.setPixelColor(0, 0);
  pixel.show();
  Serial.println("NeoPixel ready");

  // ----- BLE init -----
  NimBLEDevice::init("humn.au");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // Command characteristic — phone writes to this
  pCmdChar = pService->createCharacteristic(
    CMD_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  pCmdChar->setCallbacks(new CmdCallbacks());

  // Notify characteristic — board pushes data to phone
  pNotify = pService->createCharacteristic(
    NOTIFY_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();

  // Start advertising
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setName("humn.au");
  pAdv->start();

  Serial.println("BLE advertising as \"humn.au\"");
  updateDisplay();
}

// =========================================================
// Loop
// =========================================================

void loop() {
  // --- Heartbeat notification every 5 s ---
  if (millis() - lastHB >= HB_INTERVAL) {
    lastHB = millis();
    if (deviceConnected) {
      char hb[16];
      snprintf(hb, sizeof(hb), "HB:%lu", millis() / 1000);
      sendNotification(hb);
    }
  }

  // --- Re-advertise after disconnect ---
  if (!deviceConnected && prevConnected) {
    NimBLEDevice::getAdvertising()->start();
    Serial.println("Re-advertising...");
  }
  prevConnected = deviceConnected;

  // --- Refresh display (clear expired messages) ---
  if (messageLine[0] != '\0' && millis() >= msgExpiry) {
    messageLine[0] = '\0';
    updateDisplay();
  }

  // Periodic display refresh (every 5 s) to keep status current
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 5000) {
    lastRefresh = millis();
    updateDisplay();
  }

  delay(10);
}
