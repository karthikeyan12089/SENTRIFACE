// I2C scanner for SENTRIFACE. Prints the address of every device it finds
// on BOTH buses. Open Serial Monitor at 115200.
// LCD bus : Wire  (SDA 32, SCL 33)   - expect 0x27 or 0x3F
// OLED bus: Wire1 (SDA 21, SCL 22)   - expect 0x3C or 0x3D
#include <Wire.h>

void scan(TwoWire &bus, const char *name) {
  Serial.print("Scanning "); Serial.println(name);
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.print("  found 0x"); Serial.println(a, HEX);
      found++;
    }
  }
  if (!found) Serial.println("  nothing found - check wiring/power");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(32, 33);
  Wire1.begin(21, 22);
  delay(1000);
}

void loop() {
  scan(Wire, "LCD bus (SDA 32, SCL 33)");
  scan(Wire1, "OLED bus (SDA 21, SCL 22)");
  Serial.println("---");
  delay(3000);
}
