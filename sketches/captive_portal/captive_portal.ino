#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "lwip/lwip_napt.h"
#include "lwip/tcpip.h"
#include "esp_netif.h"
#include "dhcpserver/dhcpserver.h"

// ----- AP credentials -----
const char* AP_SSID = "humn.au";
const char* AP_PASS = "turner73";

// ----- Upstream WiFi (internet source) -----
const char* STA_SSID = "Humanising Technologies";
const char* STA_PASS = "turner73";

// ----- Hardware -----
#define LED_PIN 8
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ----- Servers -----
DNSServer captiveDns;
WebServer webServer(80);

const IPAddress apIP(192, 168, 4, 1);

// ----- State -----
bool internetMode = false;
bool staConnected = false;

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
    <div class="status green">Toggle WiFi off/on, then browse.</div>
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

void handleRedirect() {
  webServer.sendHeader("Location", "http://192.168.4.1/", true);
  webServer.send(302, "text/plain", "");
}

// Called at boot after STA connects — configures NAPT and DHCP before clients join
void setupNaptAndDhcp() {
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

  LOCK_TCPIP_CORE();
  ip_napt_enable(apIP, 1);
  UNLOCK_TCPIP_CORE();
  Serial.println("NAPT enabled");

  esp_netif_dns_info_t dns_info;
  esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
  Serial.print("Upstream DNS: ");
  Serial.println(IPAddress(dns_info.ip.u_addr.ip4.addr));

  esp_netif_dhcps_stop(ap_netif);
  esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
  dhcps_offer_t dhcps_flag = OFFER_DNS;
  esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
    ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_flag, sizeof(dhcps_flag));
  esp_netif_dhcps_start(ap_netif);
  Serial.println("DHCP configured with upstream DNS");
}

void handleEnableInternet() {
  if (internetMode) {
    webServer.send(200, "text/html", buildPage(SUCCESS_HTML));
    return;
  }

  if (!staConnected) {
    webServer.send(200, "text/html", buildPage(FAIL_HTML));
    return;
  }

  // Everything heavy was done at boot — just stop DNS hijacking
  captiveDns.stop();
  internetMode = true;
  Serial.println("Internet mode: captive DNS stopped");
  webServer.send(200, "text/html", buildPage(SUCCESS_HTML));
}

void updateDisplay() {
  int clients = WiFi.softAPgetStationNum();
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(2, 14, "humn");

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

  // AP+STA from boot
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP started: ");
  Serial.println(AP_SSID);

  // Connect to upstream before clients join
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
    setupNaptAndDhcp();
  } else {
    Serial.print("STA failed, status=");
    Serial.println(WiFi.status());
  }

  // Captive DNS — hijacks all domains until Internet button is pressed
  captiveDns.start(53, "*", apIP);
  Serial.println("Captive DNS started");

  // Web server
  webServer.on("/generate_204", handleRedirect);
  webServer.on("/connecttest.txt", handleRedirect);
  webServer.on("/hotspot-detect.html", handleRedirect);
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
  if (!internetMode) {
    captiveDns.processNextRequest();
  }

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
