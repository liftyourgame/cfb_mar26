#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Wire.h>

// ----- AP credentials -----
const char* AP_SSID = "humn.au";
const char* AP_PASS = "turner73";

// ----- Hardware -----
#define LED_PIN 8
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ----- Servers -----
DNSServer dnsServer;
WebServer webServer(80);

const byte DNS_PORT = 53;
const IPAddress apIP(192, 168, 4, 1);

// ----- Display refresh -----
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 2000;

// ----- Portal HTML -----
const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>humn.au</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #0a0a0a;
      color: #e0e0e0;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      text-align: center;
    }
    .container {
      padding: 2rem;
      max-width: 400px;
    }
    h1 {
      font-size: 2.5rem;
      font-weight: 300;
      letter-spacing: 0.15em;
      margin-bottom: 0.5rem;
      color: #ffffff;
    }
    .divider {
      width: 60px;
      height: 2px;
      background: #444;
      margin: 1.5rem auto;
    }
    p {
      font-size: 1.1rem;
      line-height: 1.6;
      color: #888;
      font-weight: 300;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>humn</h1>
    <div class="divider"></div>
    <p>Welcome to the network.</p>
  </div>
</body>
</html>
)rawliteral";

void handlePortal() {
  webServer.send(200, "text/html", PORTAL_HTML);
}

// Android captive portal check
void handleGenerate204() {
  webServer.sendHeader("Location", String("http://") + apIP.toString(), true);
  webServer.send(302, "text/plain", "");
}

// Windows captive portal check
void handleConnectTest() {
  webServer.sendHeader("Location", String("http://") + apIP.toString(), true);
  webServer.send(302, "text/plain", "");
}

// Apple captive portal check
void handleHotspotDetect() {
  webServer.sendHeader("Location", String("http://") + apIP.toString(), true);
  webServer.send(302, "text/html", "");
}

void updateDisplay() {
  int clients = WiFi.softAPgetStationNum();

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(2, 14, "humn");

  u8g2.setFont(u8g2_font_6x10_tr);
  char buf[20];
  snprintf(buf, sizeof(buf), "Clients: %d", clients);
  u8g2.drawStr(2, 34, buf);

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Captive portal booting...");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // LED on during startup

  // OLED init
  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.setContrast(30);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(2, 20, "Starting");
  u8g2.drawStr(2, 34, "AP...");
  u8g2.sendBuffer();

  // Start WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP started: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // DNS: resolve every domain to our IP
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("DNS server started");

  // Web server routes
  webServer.on("/generate_204", handleGenerate204);        // Android
  webServer.on("/connecttest.txt", handleConnectTest);     // Windows
  webServer.on("/hotspot-detect.html", handleHotspotDetect); // Apple
  webServer.on("/", handlePortal);
  webServer.onNotFound(handlePortal);                      // Everything else -> portal
  webServer.begin();
  Serial.println("Web server started");

  digitalWrite(LED_PIN, HIGH);  // LED off — startup complete
  updateDisplay();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (millis() - lastDisplayUpdate > DISPLAY_INTERVAL) {
    lastDisplayUpdate = millis();
    updateDisplay();
    Serial.print("Connected clients: ");
    Serial.println(WiFi.softAPgetStationNum());
  }
}
