/*
  SENTRIFACE - ESP32 side (v5, written from scratch)

  Serial protocol (115200 baud, one text line per message):
    ESP32 -> PC : MOTION_DETECTED   sensor triggered, open the camera
    PC -> ESP32 : CAMERA_READY      (optional) camera is open, start scanning
    PC -> ESP32 : ACCESS_GRANTED    face found, open the door
    PC -> ESP32 : NO_FACE           no face found, give up
    ESP32 -> PC : CAMERA_CLOSE      cycle finished, release the camera

  Simulation (browser Wokwi): you type ACCESS_GRANTED in the serial monitor.

  Buses:  LCD  -> Wire  (SDA 32, SCL 33)
          OLED -> Wire1 (SDA 21, SCL 22)

  Libraries needed: Adafruit SSD1306, Adafruit GFX Library, ESP32Servo
  (The LCD uses a small built-in driver, no LiquidCrystal_I2C library.)
*/

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- PINS / SETTINGS ----------------
const int SDA_LCD = 32, SCL_LCD = 33;
const int SDA_OLED = 21, SCL_OLED = 22;
const int SENSOR_PIN = 27;
const int SERVO_PIN = 13;

// Wokwi PIR: HIGH when motion. Most real IR obstacle modules: LOW when detected.
// Flip to LOW for the physical IR sensor.
const int SENSOR_ACTIVE_LEVEL = HIGH;

const uint8_t LCD_ADDR = 0x27;
const uint8_t OLED_ADDR = 0x3C;

const int SERVO_CLOSED = 0;
const int SERVO_OPEN = 90;

const unsigned long CAMERA_WAIT_MS = 3000;    // fallback if no CAMERA_READY arrives
const unsigned long SCAN_TIMEOUT_MS = 20000;  // give up if no answer from PC
const unsigned long VERIFY_MS = 1500;         // OLED loading bar duration
const unsigned long DOOR_OPEN_MS = 5000;
const unsigned long CLOSED_MSG_MS = 1500;
const unsigned long NOFACE_MSG_MS = 2000;
const unsigned long COOLDOWN_MS = 3000;

// ---------------- MINIMAL I2C LCD DRIVER (PCF8574 backpack, 16x2) ----------------
class SimpleLCD {
  TwoWire *w;
  uint8_t addr;
  uint8_t backlightBit = 0x08;
  void expWrite(uint8_t d) {
    w->beginTransmission(addr);
    w->write(d | backlightBit);
    w->endTransmission();
  }
  void pulse(uint8_t d) {
    expWrite(d | 0x04);
    delayMicroseconds(2);
    expWrite(d & ~0x04);
    delayMicroseconds(60);
  }
  void write4(uint8_t d) { expWrite(d); pulse(d); }
  void send(uint8_t v, uint8_t mode) {
    write4((v & 0xF0) | mode);
    write4(((v << 4) & 0xF0) | mode);
  }
public:
  SimpleLCD(TwoWire *wire, uint8_t address) : w(wire), addr(address) {}
  void begin() {
    delay(50);
    expWrite(0);
    delay(50);
    write4(0x30); delay(5);
    write4(0x30); delayMicroseconds(150);
    write4(0x30); delayMicroseconds(150);
    write4(0x20); delayMicroseconds(150);   // 4-bit mode
    send(0x28, 0);   // 2 lines, 5x8 font
    send(0x0C, 0);   // display on, cursor off
    send(0x06, 0);   // entry mode
    clear();
  }
  void clear() { send(0x01, 0); delay(3); }
  void setCursor(uint8_t col, uint8_t row) {
    static const uint8_t off[] = {0x00, 0x40};
    send(0x80 | (col + off[row & 1]), 0);
  }
  void print(const char *s) { while (*s) send(*s++, 0x01); }
  void backlight(bool on) { backlightBit = on ? 0x08 : 0x00; expWrite(0); }
  // Writes both lines, padded to 16 chars so no clear() flicker.
  void show(const char *l1, const char *l2 = "") {
    char buf[17];
    snprintf(buf, sizeof(buf), "%-16.16s", l1);
    setCursor(0, 0); print(buf);
    snprintf(buf, sizeof(buf), "%-16.16s", l2);
    setCursor(0, 1); print(buf);
  }
};

SimpleLCD lcd(&Wire, LCD_ADDR);
Adafruit_SSD1306 oled(128, 64, &Wire1, -1);
Servo doorServo;

// ---------------- STATE MACHINE ----------------
enum State { IDLE, WAIT_CAMERA, SCANNING, VERIFYING, DOOR_OPEN, DOOR_CLOSED, NO_FACE_MSG, COOLDOWN };
State state = IDLE;
unsigned long stateStart = 0;
unsigned long lastBarDraw = 0;
String rxLine = "";

bool sensorActive() { return digitalRead(SENSOR_PIN) == SENSOR_ACTIVE_LEVEL; }

void oledText(const char *text, int size, int x, int y) {
  oled.clearDisplay();
  oled.setTextSize(size);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(x, y);
  oled.print(text);
  oled.display();
}

void setState(State s) {
  state = s;
  stateStart = millis();
  switch (s) {
    case IDLE:
      lcd.show("System Ready", "Waiting...");
      oled.clearDisplay(); oled.display();
      break;
    case WAIT_CAMERA:
      lcd.show("Motion detected", "Camera opening..");
      oledText("Camera...", 2, 14, 24);
      Serial.println("MOTION_DETECTED");
      break;
    case SCANNING:
      lcd.show("Scanning face...", "Look at camera");
      oledText("Scanning Face", 1, 20, 28);
      break;
    case VERIFYING:
      lcd.show("Face recognized", "Verifying...");
      lastBarDraw = 0;
      break;
    case DOOR_OPEN:
      lcd.show("Admin recognized", "Access granted");
      oledText("MATCH!", 2, 22, 24);
      doorServo.write(SERVO_OPEN);
      break;
    case DOOR_CLOSED:
      doorServo.write(SERVO_CLOSED);
      lcd.show("Door closed", "");
      oledText("CLOSED", 2, 22, 24);
      break;
    case NO_FACE_MSG:
      lcd.show("No face found", "Try again");
      oledText("NO FACE", 2, 16, 24);
      break;
    case COOLDOWN:
      Serial.println("CAMERA_CLOSE");
      lcd.show("Please wait...", "");
      oled.clearDisplay(); oled.display();
      break;
  }
}

void drawLoadingBar(unsigned long elapsed) {
  int pct = (int)min(100UL, elapsed * 100UL / VERIFY_MS);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(22, 14);
  oled.print("Loading...");
  oled.drawRect(14, 30, 100, 12, SSD1306_WHITE);
  oled.fillRect(14, 30, pct, 12, SSD1306_WHITE);
  oled.setCursor(52, 48);
  oled.print(pct);
  oled.print("%");
  oled.display();
}

// ---------------- SERIAL INPUT (non-blocking) ----------------
void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  bool listening = (state == WAIT_CAMERA || state == SCANNING);

  if (cmd == "CAMERA_READY") {
    if (state == WAIT_CAMERA) setState(SCANNING);
  } else if (cmd.startsWith("ACCESS_GRANTED")) {
    if (listening) setState(VERIFYING);
  } else if (cmd == "NO_FACE" || cmd == "ACCESS_DENIED") {
    if (listening) setState(NO_FACE_MSG);
  }
}

void readSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxLine.length()) { handleCommand(rxLine); rxLine = ""; }
    } else if (rxLine.length() < 64) {
      rxLine += c;
    }
  }
}

// ---------------- SETUP / LOOP ----------------
void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_LCD, SCL_LCD);
  Wire1.begin(SDA_OLED, SCL_OLED);
  Wire1.setClock(400000);

  lcd.begin();
  lcd.backlight(true);
  lcd.show("SENTRIFACE", "Booting...");

  // periphBegin=false: we already started Wire1 on the right pins
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false)) {
    lcd.show("OLED not found", "check wiring");
    Serial.println("OLED_INIT_FAILED");
    for (;;) delay(1000);
  }
  oled.clearDisplay(); oled.display();

  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  doorServo.write(SERVO_CLOSED);

  pinMode(SENSOR_PIN, INPUT);

  delay(1000);
  setState(COOLDOWN);   // start by waiting for the sensor to settle
  Serial.println("SENTRIFACE_READY");
}

void loop() {
  readSerial();
  unsigned long elapsed = millis() - stateStart;

  switch (state) {
    case IDLE:
      if (sensorActive()) setState(WAIT_CAMERA);
      break;

    case WAIT_CAMERA:
      if (elapsed >= CAMERA_WAIT_MS) setState(SCANNING);
      break;

    case SCANNING:
      if (elapsed >= SCAN_TIMEOUT_MS) setState(NO_FACE_MSG);
      break;

    case VERIFYING:
      if (millis() - lastBarDraw >= 50) {
        lastBarDraw = millis();
        drawLoadingBar(elapsed);
      }
      if (elapsed >= VERIFY_MS) setState(DOOR_OPEN);
      break;

    case DOOR_OPEN:
      if (elapsed >= DOOR_OPEN_MS) setState(DOOR_CLOSED);
      break;

    case DOOR_CLOSED:
      if (elapsed >= CLOSED_MSG_MS) setState(COOLDOWN);
      break;

    case NO_FACE_MSG:
      if (elapsed >= NOFACE_MSG_MS) setState(COOLDOWN);
      break;

    case COOLDOWN:
      // re-arm only after cooldown AND the sensor has cleared
      if (elapsed >= COOLDOWN_MS && !sensorActive()) setState(IDLE);
      break;
  }
}
