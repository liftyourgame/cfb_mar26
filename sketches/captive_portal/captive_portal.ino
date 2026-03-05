#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "lwip/lwip_napt.h"
#include "lwip/tcpip.h"
#include "esp_netif.h"

// ----- AP credentials -----
const char* AP_SSID = "humn.au";
const char* AP_PASS = "LivingTheDream";

// ----- Upstream WiFi (internet source) -----
const char* STA_SSID = "Humanising Technologies";
const char* STA_PASS = "turner73";

// ----- Hardware -----
#define LED_PIN 8
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ----- Servers -----
WebServer webServer(80);
const IPAddress apIP(192, 168, 4, 1);

// ----- State -----
bool internetMode = false;
bool staConnected = false;

// ----- Unified DNS on port 53: captive replies OR upstream forwarding -----
WiFiUDP dnsSocket;  // port 53 — never rebound
WiFiUDP dnsOut;     // ephemeral — upstream forwarding only
IPAddress upstreamDns;

#define MAX_PENDING 4
struct PendingQuery {
  IPAddress clientIP;
  uint16_t clientPort;
  uint16_t txnId;
  unsigned long sentAt;
  bool active;
} pending[MAX_PENDING];

// ----- Display refresh -----
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 2000;

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

void handlePortal() {
  webServer.send(200, "text/html", buildPage(PORTAL_HTML));
}

void handleAppleDetect() {
  if (internetMode) {
    webServer.send(200, "text/html",
      "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  } else {
    webServer.send(200, "text/html", buildPage(PORTAL_HTML));
  }
}

void handleAndroidDetect() {
  if (internetMode) {
    webServer.send(204, "", "");
  } else {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  }
}

void handleWindowsDetect() {
  if (internetMode) {
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
  Serial.println("NAPT enabled");
}

void enableForwarding() {
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_dns_info_t dns_info;
  esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
  upstreamDns = IPAddress(dns_info.ip.u_addr.ip4.addr);
  dnsOut.begin(5353);
  memset(pending, 0, sizeof(pending));
  Serial.printf("DNS forwarding -> %s\n", upstreamDns.toString().c_str());
}

// ==================== Unified DNS handler ====================

void processDns() {
  // Incoming client query on port 53
  int pktSize = dnsSocket.parsePacket();
  if (pktSize > 0 && pktSize <= 512) {
    uint8_t buf[512];
    int len = dnsSocket.read(buf, sizeof(buf));
    if (len < 12) return;

    IPAddress clientIP = dnsSocket.remoteIP();
    uint16_t clientPort = dnsSocket.remotePort();
    uint16_t txnId = (buf[0] << 8) | buf[1];

    if (!internetMode) {
      // Captive: respond with AP IP for every query
      buf[2] = 0x81; buf[3] = 0x80;
      buf[6] = 0x00; buf[7] = 0x01;
      buf[8] = 0; buf[9] = 0;
      buf[10] = 0; buf[11] = 0;

      // Skip past QNAME + QTYPE + QCLASS
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
      // Internet: forward to upstream DNS
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
        Serial.printf("DNS Q %04X fwd\n", txnId);
      }
    }
  }

  // Upstream DNS response (internet mode only)
  if (internetMode) {
    pktSize = dnsOut.parsePacket();
    if (pktSize > 0 && pktSize <= 512) {
      uint8_t buf[512];
      int len = dnsOut.read(buf, sizeof(buf));
      if (len >= 2) {
        uint16_t txnId = (buf[0] << 8) | buf[1];
        for (int i = 0; i < MAX_PENDING; i++) {
          if (pending[i].active && pending[i].txnId == txnId) {
            dnsSocket.beginPacket(pending[i].clientIP, pending[i].clientPort);
            dnsSocket.write(buf, len);
            dnsSocket.endPacket();
            pending[i].active = false;
            Serial.printf("DNS R %04X ok\n", txnId);
            break;
          }
        }
      }
    }
  }
}

// ==================== Internet button ====================

void handleEnableInternet() {
  if (internetMode) {
    webServer.send(200, "text/html", buildPage(SUCCESS_HTML));
    return;
  }
  if (!staConnected) {
    webServer.send(200, "text/html", buildPage(FAIL_HTML));
    return;
  }

  enableForwarding();
  internetMode = true;
  Serial.println("Internet mode enabled");
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
  if (internetMode) {
    snprintf(buf, sizeof(buf), "NET  Cli:%d", clients);
  } else if (staConnected) {
    snprintf(buf, sizeof(buf), "RDY  Cli:%d", clients);
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
  Serial.print("AP started: ");
  Serial.println(AP_SSID);

  WiFi.begin(STA_SSID, STA_PASS);
  WiFi.setAutoReconnect(true);
  Serial.print("Connecting to ");
  Serial.println(STA_SSID);

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
    Serial.print("STA connected, IP: ");
    Serial.println(WiFi.localIP());
    enableNapt();
  } else {
    Serial.print("STA failed, status=");
    Serial.println(WiFi.status());
  }

  // Single DNS socket on port 53 — handles captive AND forwarding
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

  digitalWrite(LED_PIN, HIGH);
  updateDisplay();
  Serial.println("Ready!");
}

void loop() {
  processDns();
  webServer.handleClient();

  if (!staConnected && WiFi.status() == WL_CONNECTED) {
    staConnected = true;
    Serial.print("STA reconnected, IP: ");
    Serial.println(WiFi.localIP());
  }
  if (staConnected && WiFi.status() != WL_CONNECTED) {
    staConnected = false;
    Serial.println("STA disconnected");
  }

  if (millis() - lastDisplayUpdate > DISPLAY_INTERVAL) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}
