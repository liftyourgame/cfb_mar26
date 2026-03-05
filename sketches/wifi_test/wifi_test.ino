#include <WiFi.h>

const char* SSID = "Humanising Technologies";
const char* PASS = "turner73";

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("WiFi connection test...");
  Serial.print("SSID: [");
  Serial.print(SSID);
  Serial.print("] PASS: [");
  Serial.print(PASS);
  Serial.println("]");

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);

  for (int i = 0; i < 30; i++) {
    wl_status_t s = WiFi.status();
    Serial.print("Attempt ");
    Serial.print(i + 1);
    Serial.print(": status=");
    switch (s) {
      case WL_IDLE_STATUS:    Serial.println("IDLE"); break;
      case WL_NO_SSID_AVAIL: Serial.println("NO_SSID_AVAIL"); break;
      case WL_CONNECTED:     Serial.println("CONNECTED"); break;
      case WL_CONNECT_FAILED:Serial.println("CONNECT_FAILED"); break;
      case WL_DISCONNECTED:  Serial.println("DISCONNECTED"); break;
      default:               Serial.println(s); break;
    }
    if (s == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("Gateway: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("DNS: ");
      Serial.println(WiFi.dnsIP());
      return;
    }
    if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) {
      Serial.println("Connection failed.");
      return;
    }
    delay(1000);
  }
  Serial.println("Timed out.");
}

void loop() { delay(10000); }
