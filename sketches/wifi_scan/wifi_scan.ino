#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("WiFi scan starting...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks:");
  for (int i = 0; i < n; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(": [");
    Serial.print(WiFi.SSID(i));
    Serial.print("] RSSI:");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" Ch:");
    Serial.print(WiFi.channel(i));
    Serial.print(" ");
    Serial.println(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
  }
  Serial.println("Scan complete.");
}

void loop() {
  delay(10000);
}
