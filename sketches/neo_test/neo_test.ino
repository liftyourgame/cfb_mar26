#include <Adafruit_NeoPixel.h>

// Cycles red → green → blue → off on the onboard WS2812B
// to confirm which GPIO controls it and that it responds

Adafruit_NeoPixel neo(1, 2, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(1500);
  neo.begin();
  neo.setBrightness(40);
  neo.clear();
  neo.show();
  Serial.println("NeoPixel test starting");
}

void loop() {
  Serial.println("RED");
  neo.setPixelColor(0, neo.Color(180, 0, 0));
  neo.show();
  delay(1000);

  Serial.println("GREEN");
  neo.setPixelColor(0, neo.Color(0, 180, 0));
  neo.show();
  delay(1000);

  Serial.println("BLUE");
  neo.setPixelColor(0, neo.Color(0, 0, 180));
  neo.show();
  delay(1000);

  Serial.println("OFF");
  neo.clear();
  neo.show();
  delay(1000);
}
