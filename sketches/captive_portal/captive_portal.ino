#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include "lwip/lwip_napt.h"
#include "lwip/tcpip.h"
#include "esp_netif.h"
#include "secrets.h"

// ----- Hardware -----
#define LED_PIN 8
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ----- Servers -----
WebServer webServer(80);
const IPAddress apIP(192, 168, 4, 1);

// Forward declarations for BLE logging
void bleLog(const char* msg);
extern char _bleBuf[128];
#define bleLogf(fmt, ...) do { \
  snprintf(_bleBuf, sizeof(_bleBuf), fmt, ##__VA_ARGS__); \
  bleLog(_bleBuf); \
} while(0)

// ----- Per-device authorization -----
#define MAX_AUTHORIZED 8
IPAddress authorizedIPs[MAX_AUTHORIZED];
int numAuthorized = 0;
bool forwarderStarted = false;
bool staConnected = false;

bool isAuthorized(IPAddress ip) {
  for (int i = 0; i < numAuthorized; i++) {
    if (authorizedIPs[i] == ip) return true;
  }
  return false;
}

bool authorizeClient(IPAddress ip) {
  if (isAuthorized(ip)) return true;
  if (numAuthorized >= MAX_AUTHORIZED) return false;
  authorizedIPs[numAuthorized++] = ip;
  bleLogf("Authorized: %s (%d/%d)", ip.toString().c_str(), numAuthorized, MAX_AUTHORIZED);
  return true;
}

// ----- Unified DNS on port 53 -----
WiFiUDP dnsSocket;
WiFiUDP dnsOut;
IPAddress upstreamDns;

#define MAX_PENDING 16
struct PendingQuery {
  IPAddress clientIP;
  uint16_t clientPort;
  uint16_t txnId;
  unsigned long sentAt;
  bool active;
} pending[MAX_PENDING];

// ----- Display refresh -----
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 5000;

// ----- BLE log monitor -----
#define BLE_LOG_SERVICE_UUID   "91bad492-b950-4226-aa2b-4ede9fa42f59"
#define BLE_LOG_CHAR_UUID      "ca73b3ba-39f6-4ab3-91ae-186dc9577d99"
NimBLEServer*         bleServer  = nullptr;
NimBLECharacteristic* bleLogChar = nullptr;
bool bleClientConnected = false;

class BleLogServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& ci) override  { bleClientConnected = true;  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& ci, int r) override { bleClientConnected = false; }
};

// Writes to both Serial and BLE notify (chunks > 20 bytes for BLE MTU)
void bleLog(const char* msg) {
  Serial.println(msg);
  if (!bleClientConnected) return;
  size_t len = strlen(msg);
  const size_t chunk = 20;
  for (size_t off = 0; off < len; off += chunk) {
    size_t n = (len - off > chunk) ? chunk : len - off;
    bleLogChar->setValue((const uint8_t*)(msg + off), n);
    bleLogChar->notify();
  }
}

char _bleBuf[128];

// ----- Shared CSS -----
const char SHARED_CSS[] PROGMEM = R"rawliteral(
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: #0a0a0a; color: #e0e0e0;
  display: flex; justify-content: center; align-items: center;
  min-height: 100vh; text-align: center;
}
.container { padding: 2rem; max-width: 400px; }
h1 { font-size: 2.5rem; font-weight: 300; letter-spacing: 0.15em; color: #fff; }
.divider { width: 60px; height: 2px; background: #444; margin: 1.5rem auto; }
p { font-size: 1.1rem; line-height: 1.6; color: #888; font-weight: 300; margin-bottom: 2rem; }
button {
  background: #222; color: #fff;
  border: 1px solid #444; padding: 0.8rem 2.5rem;
  font-size: 1rem; border-radius: 8px;
  cursor: pointer; transition: all 0.2s;
  -webkit-appearance: none;
}
button:hover { background: #333; border-color: #666; }
button:active { background: #444; }
.ok { border-color: #4a4 !important; }
.status { margin-top: 1rem; font-size: 0.9rem; }
.green { color: #4a4; }
.red { color: #a44; }
.hint { margin-top: 2rem; color: #555; font-size: 0.8rem; }
)rawliteral";

const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>humn.au</title>
  <style>%CSS%</style>
</head>
<body>
  <div class="container">
    <h1>humn</h1>
    <div class="divider"></div>
    <p>Welcome to the network.</p>
    <form action="/internet" method="get">
      <button type="submit">Internet</button>
    </form>
    <div class="hint">
      If button doesn't work, open your<br>browser and go to <b>192.168.4.1</b>
    </div>
  </div>
</body>
</html>
)rawliteral";

const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>humn.au</title>
  <style>%CSS%</style>
</head>
<body>
  <div class="container">
    <h1>humn</h1>
    <div class="divider"></div>
    <p>Internet is now available.</p>
    <button class="ok" disabled>&#10003; Connected</button>
    <div class="status green">You can now browse the internet.</div>
  </div>
</body>
</html>
)rawliteral";

const char FAIL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>humn.au</title>
  <style>%CSS%</style>
</head>
<body>
  <div class="container">
    <h1>humn</h1>
    <div class="divider"></div>
    <p>Internet is not available.</p>
    <form action="/" method="get"><button type="submit">Back</button></form>
    <div class="status red">Upstream network not connected.</div>
  </div>
</body>
</html>
)rawliteral";

String buildPage(const char* tpl) {
  String page = FPSTR(tpl);
  String css = FPSTR(SHARED_CSS);
  page.replace("%CSS%", css);
  return page;
}

// ==================== Web handlers (per-client checks) ====================

void handlePortal() {
  webServer.send(200, "text/html", buildPage(PORTAL_HTML));
}

void handleAppleDetect() {
  if (isAuthorized(webServer.client().remoteIP())) {
    webServer.send(200, "text/html",
      "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  } else {
    webServer.send(200, "text/html", buildPage(PORTAL_HTML));
  }
}

void handleAndroidDetect() {
  if (isAuthorized(webServer.client().remoteIP())) {
    webServer.send(204, "", "");
  } else {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  }
}

void handleWindowsDetect() {
  if (isAuthorized(webServer.client().remoteIP())) {
    webServer.send(200, "text/plain", "Microsoft Connect Test");
  } else {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  }
}

// ==================== Network setup ====================

void enableNapt() {
  LOCK_TCPIP_CORE();
  ip_napt_enable(apIP, 1);
  UNLOCK_TCPIP_CORE();
  bleLog("NAPT enabled");
}

void ensureForwarderStarted() {
  if (forwarderStarted) return;
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_dns_info_t dns_info;
  esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
  upstreamDns = IPAddress(dns_info.ip.u_addr.ip4.addr);
  dnsOut.begin(5353);
  memset(pending, 0, sizeof(pending));
  forwarderStarted = true;
  bleLogf("DNS forwarding -> %s", upstreamDns.toString().c_str());
}

// ==================== Unified DNS handler (per-client) ====================

void processDns() {
  // Process up to 8 client queries per call to keep up with burst traffic
  for (int q = 0; q < 8; q++) {
    int pktSize = dnsSocket.parsePacket();
    if (pktSize <= 0 || pktSize > 512) break;

    uint8_t buf[512];
    int len = dnsSocket.read(buf, sizeof(buf));
    if (len < 12) continue;

    IPAddress clientIP = dnsSocket.remoteIP();
    uint16_t clientPort = dnsSocket.remotePort();
    uint16_t txnId = (buf[0] << 8) | buf[1];

    if (!isAuthorized(clientIP)) {
      buf[2] = 0x81; buf[3] = 0x80;
      buf[6] = 0x00; buf[7] = 0x01;
      buf[8] = 0; buf[9] = 0;
      buf[10] = 0; buf[11] = 0;

      int pos = 12;
      while (pos < len && buf[pos] != 0) pos += buf[pos] + 1;
      pos += 5;

      uint8_t answer[] = {
        0xC0, 0x0C,
        0x00, 0x01,
        0x00, 0x01,
        0x00, 0x00, 0x00, 0x3C,
        0x00, 0x04,
        apIP[0], apIP[1], apIP[2], apIP[3]
      };
      if (pos + (int)sizeof(answer) <= 512) {
        memcpy(buf + pos, answer, sizeof(answer));
        dnsSocket.beginPacket(clientIP, clientPort);
        dnsSocket.write(buf, pos + sizeof(answer));
        dnsSocket.endPacket();
      }
    } else {
      int slot = -1;
      for (int i = 0; i < MAX_PENDING; i++) {
        if (!pending[i].active || millis() - pending[i].sentAt > 3000) {
          slot = i; break;
        }
      }
      if (slot >= 0) {
        pending[slot] = { clientIP, clientPort, txnId, millis(), true };
        dnsOut.beginPacket(upstreamDns, 53);
        dnsOut.write(buf, len);
        dnsOut.endPacket();
      }
    }
  }

  // Process up to 8 upstream responses per call
  if (forwarderStarted) {
    for (int r = 0; r < 8; r++) {
      int pktSize = dnsOut.parsePacket();
      if (pktSize <= 0 || pktSize > 512) break;

      uint8_t buf[512];
      int len = dnsOut.read(buf, sizeof(buf));
      if (len < 2) continue;

      uint16_t txnId = (buf[0] << 8) | buf[1];
      for (int i = 0; i < MAX_PENDING; i++) {
        if (pending[i].active && pending[i].txnId == txnId) {
          dnsSocket.beginPacket(pending[i].clientIP, pending[i].clientPort);
          dnsSocket.write(buf, len);
          dnsSocket.endPacket();
          pending[i].active = false;
          break;
        }
      }
    }
  }
}

// ==================== Internet button (per-client) ====================

void handleEnableInternet() {
  IPAddress clientIP = webServer.client().remoteIP();

  if (isAuthorized(clientIP)) {
    webServer.send(200, "text/html", buildPage(SUCCESS_HTML));
    return;
  }
  if (!staConnected) {
    webServer.send(200, "text/html", buildPage(FAIL_HTML));
    return;
  }

  ensureForwarderStarted();
  authorizeClient(clientIP);
  webServer.send(200, "text/html", buildPage(SUCCESS_HTML));
}

// ==================== OLED ====================

void updateDisplay() {
  int clients = WiFi.softAPgetStationNum();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(2, 14, "humn.au");
  u8g2.setFont(u8g2_font_6x10_tr);
  char buf[24];
  if (staConnected) {
    snprintf(buf, sizeof(buf), "%d/%d  Cli:%d", numAuthorized, clients, clients);
  } else {
    snprintf(buf, sizeof(buf), "AP   Cli:%d", clients);
  }
  u8g2.drawStr(2, 34, buf);
  u8g2.sendBuffer();
}

// ==================== Setup & Loop ====================

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Captive portal booting...");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(30);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(2, 20, "Starting");
  u8g2.drawStr(2, 34, "WiFi...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP started: %s\n", AP_SSID);

  WiFi.begin(STA_SSID, STA_PASS);
  WiFi.setAutoReconnect(true);
  Serial.printf("Connecting to %s\n", STA_SSID);

  unsigned long start = millis();
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    attempt++;
    char oledBuf[20];
    snprintf(oledBuf, sizeof(oledBuf), "STA:%d s:%d", attempt, WiFi.status());
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(2, 20, "Connecting");
    u8g2.drawStr(2, 34, oledBuf);
    u8g2.sendBuffer();
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    staConnected = true;
    Serial.printf("STA connected, IP: %s\n", WiFi.localIP().toString().c_str());
    enableNapt();
  } else {
    Serial.printf("STA failed, status=%d\n", WiFi.status());
  }

  dnsSocket.begin(53);
  Serial.println("DNS listening on :53");

  webServer.on("/hotspot-detect.html", handleAppleDetect);
  webServer.on("/generate_204", handleAndroidDetect);
  webServer.on("/connecttest.txt", handleWindowsDetect);
  webServer.on("/internet", handleEnableInternet);
  webServer.on("/", handlePortal);
  webServer.onNotFound(handlePortal);
  webServer.begin();
  Serial.println("Web server started");

  // ----- BLE log monitor init -----
  NimBLEDevice::init("humn.au-log");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new BleLogServerCB());
  NimBLEService* bleSvc = bleServer->createService(BLE_LOG_SERVICE_UUID);
  bleLogChar = bleSvc->createCharacteristic(
    BLE_LOG_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  bleSvc->start();
  NimBLEAdvertising* bleAdv = NimBLEDevice::getAdvertising();
  bleAdv->addServiceUUID(BLE_LOG_SERVICE_UUID);
  bleAdv->setName("humn.au-log");
  bleAdv->start();
  Serial.println("BLE log advertising as \"humn.au-log\"");

  digitalWrite(LED_PIN, HIGH);
  updateDisplay();
  bleLog("Ready!");
}

void loop() {
  processDns();
  webServer.handleClient();

  if (!staConnected && WiFi.status() == WL_CONNECTED) {
    staConnected = true;
    bleLogf("STA reconnected, IP: %s", WiFi.localIP().toString().c_str());
  }
  if (staConnected && WiFi.status() != WL_CONNECTED) {
    staConnected = false;
    bleLog("STA disconnected");
  }

  // Re-advertise BLE after disconnect
  {
    static bool prevBleConn = false;
    if (!bleClientConnected && prevBleConn) {
      NimBLEDevice::getAdvertising()->start();
    }
    prevBleConn = bleClientConnected;
  }

  if (millis() - lastDisplayUpdate > DISPLAY_INTERVAL) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}
