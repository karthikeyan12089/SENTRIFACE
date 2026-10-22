# SENTRIFACE

A face-activated "door" built with an ESP32 and a laptop webcam.

A sensor notices someone, the ESP32 tells the laptop to open its camera, the laptop looks for a face, and if it finds one the ESP32 opens the "door" (a servo) for 5 seconds, then closes it.

> **Current status:** any detected face opens the door. Recognising only specific people is a planned upgrade.

---

## How it works

```
 Sensor sees someone
        |
        v
 ESP32  --- "MOTION_DETECTED" --->  Laptop (Python) opens the webcam
        <--- "CAMERA_READY" ------
        <--- "ACCESS_GRANTED" ----  (face found)   or   "NO_FACE" (after 15 s)
        |
        v
 OLED loading bar -> "MATCH!"  |  LCD shows status  |  Servo opens 5 s, then closes
        |
        v
 ESP32  --- "CAMERA_CLOSE" ---->   Laptop closes the webcam
        |
        v
 3 s cooldown, then ready again
```

The ESP32 and the laptop talk over the USB cable (serial, 115200 baud, one text line per message).

## Hardware

| Part | ESP32 pin |
|---|---|
| LCD 16x2 (I2C) SDA / SCL | GPIO 32 / GPIO 33 |
| OLED 0.96" (I2C) SDA / SCL | GPIO 21 / GPIO 22 |
| Motion sensor output | GPIO 27 |
| Servo (SG90) signal | GPIO 13 |

The LCD and OLED sit on **separate I2C buses** on purpose, so they never fight each other.

Power notes (check the labels on your modules):

- Power the IR/motion sensor from **3.3 V**, not 5 V. At 5 V its output can exceed what the ESP32 pin tolerates.
- Power the servo from the **5 V / VIN** pin, with a shared GND. Powering it from 3.3 V can reset the board.
- Use a **data** micro-USB cable. Charge-only cables will not show a COM port.

## Project layout

```
sentriface/
  firmware/
    sentriface/sentriface.ino      <- the main ESP32 sketch
    i2c_scanner/i2c_scanner.ino    <- finds your LCD/OLED addresses
  python/
    sentriface_camera.py           <- the laptop program (talks to the ESP32)
    camera_test.py                 <- webcam + face test, no ESP32 needed
  run.bat                          <- double-click to run everything
  test_camera.bat                  <- double-click to test only the camera
  requirements.txt
```

---

## Part A: One-time ESP32 setup (do this once per board)

You only need this on the laptop you use to **upload** the sketch. After uploading, the code stays on the board, and other laptops only need Part B.

1. Install **Arduino IDE 2** from https://www.arduino.cc/en/software
2. Open Arduino IDE, then **Tools, Board, Boards Manager**. Search `esp32`, and install **esp32 by Espressif Systems**.
3. Open **Tools, Manage Libraries** and install these three:
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`
   - `ESP32Servo`
   - (If it offers to install dependencies, click **Install all**.)
4. Plug in the ESP32 with the USB cable.
5. Open **Device Manager, Ports (COM & LPT)**. A new COM port should appear (for example `Silicon Labs CP210x (COM5)` or `USB-SERIAL CH340 (COM5)`).
   - No port? Try another cable, and install the driver for your chip (CP210x or CH340).
6. In Arduino IDE, choose **File, Open** and select `firmware/sentriface/sentriface.ino`.
7. **Tools, Board, ESP32 Arduino, ESP32 Dev Module**, then **Tools, Port** and pick your COM port.
8. **Check the sensor polarity** near the top of `sentriface.ino`:
   ```cpp
   const int SENSOR_ACTIVE_LEVEL = HIGH;   // PIR sensor / Wokwi
   ```
   - PIR sensor: leave it as `HIGH`.
   - Most **IR obstacle avoidance modules** output **LOW** when something is detected. Change it to `LOW`.
   - Not sure? See "Sensor test" below.
9. Click **Upload** (the right arrow). If it says `Connecting......`, **hold the BOOT button** on the ESP32 until it starts uploading, then release.
10. When it says **Done uploading**, the LCD shows `System Ready` after about 3 seconds and the servo sits at its closed position.

### Sensor test

Open **Tools, Serial Monitor** at **115200** baud and wave a hand at the sensor.

| What you see | Meaning |
|---|---|
| `MOTION_DETECTED` appears when you wave | Polarity is correct. |
| `MOTION_DETECTED` appears constantly with nobody there | Polarity is wrong. Flip `SENSOR_ACTIVE_LEVEL`, then re-upload. |
| Nothing ever appears | Polarity is wrong the other way, or the sensor's sensitivity knob needs turning. Flip it and re-upload. |

You can also type `ACCESS_GRANTED` and press Enter in the Serial Monitor to test the door without a camera. The loading bar, `MATCH!`, and servo should run.

**Close the Serial Monitor when you finish.** It locks the COM port, and the Python program cannot connect while it is open.

### If the LCD or OLED stays blank

Upload `firmware/i2c_scanner/i2c_scanner.ino` and open the Serial Monitor at 115200. It lists the addresses it finds. This project expects:

| Device | Expected address | If yours differs |
|---|---|---|
| LCD | `0x27` | Many LCDs use `0x3F`. Change `LCD_ADDR` in `sentriface.ino`. |
| OLED | `0x3C` | Some use `0x3D`. Change `OLED_ADDR`. |

Then re-upload `sentriface.ino`.

---

## Part B: Running it on any Windows laptop

You need: the ESP32 already flashed (Part A), a USB cable, a webcam, and internet for the first run.

### 1. Install Python (once per laptop)

1. Download Python from https://www.python.org/downloads/ (Python 3.10 to 3.13 are the safest picks; 3.14 also worked in testing).
2. Run the installer and **tick "Add python.exe to PATH"** at the bottom of the first screen, then click **Install Now**.
3. If `py` or `python` later opens the Microsoft Store instead of running, search Windows for **"Manage app execution aliases"** and turn off the `python.exe` and `python3.exe` entries.

### 2. Get the project

Either clone it:

```
git clone https://github.com/YOUR-USERNAME/sentriface.git
```

or download it from GitHub with **Code, Download ZIP**, then extract the ZIP.

### 3. Allow the camera

Open **Settings, Privacy & security, Camera**. Make sure **Camera access** is on, and that **Let desktop apps access your camera** is on.

### 4. Plug in the ESP32

Connect it by USB. Confirm in **Device Manager, Ports** that a COM port appears. If it doesn't, install the CP210x or CH340 driver.

### 5. Test the camera first (recommended)

Double-click **`test_camera.bat`**.

- The first run takes a minute or two, because it creates a Python environment and downloads OpenCV.
- A camera window opens. A **green box** should follow your face, and the label turns **ACCESS GRANTED**.
- Press **`q`** in the camera window to quit.

### 6. Run the full system

1. Make sure the **Arduino Serial Monitor is closed**.
2. Double-click **`run.bat`**.
3. Wait 3 seconds after it says it is listening. The board may reboot when the program connects.
4. Wave a hand in front of the sensor. The camera window opens.
5. Look at the camera. When a face is seen, the OLED shows a loading bar and `MATCH!`, the LCD shows `Admin recognized / Access granted`, and the servo opens for 5 seconds, then closes.
6. The camera window closes when the cycle finishes. The system is ready for the next person after a 3 second cooldown, once the sensor has cleared (step away from it).
7. To quit, click the camera window and press **`q`**, or press **Ctrl+C** in the black window.

If you prefer the command line instead of the batch files:

```
py -3 -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
.venv\Scripts\python.exe python\sentriface_camera.py
```

(Use `.venv\Scripts\python.exe` directly. You do not need to "activate" the environment, which avoids PowerShell script-policy errors.)

---

## Expected behaviour on the display

| Stage | LCD | OLED | Serial |
|---|---|---|---|
| Idle | `System Ready / Waiting...` | blank | |
| Sensor triggers | `Motion detected / Camera opening..` | `Camera...` | ESP32 sends `MOTION_DETECTED` |
| Camera ready | `Scanning face... / Look at camera` | `Scanning Face` | PC sends `CAMERA_READY` |
| Face found | `Face recognized / Verifying...` | loading bar | PC sends `ACCESS_GRANTED` |
| Door open (5 s) | `Admin recognized / Access granted` | `MATCH!` | |
| Door closed | `Door closed` | `CLOSED` | ESP32 sends `CAMERA_CLOSE` |
| No face in 15 s | `No face found / Try again` | `NO FACE` | PC sends `NO_FACE` |
| Cooldown (3 s) | `Please wait...` | blank | |

---

## Settings you may want to change

In `python/sentriface_camera.py`:

| Setting | Default | When to change it |
|---|---|---|
| `PORT` | `None` (auto-detect) | Set to `"COM5"` if it picks the wrong port. |
| `CAMERA_INDEX` | `0` | Set to `1` if the wrong camera opens (for example a virtual or external one). |
| `SCAN_TIMEOUT_S` | `15` | Seconds to wait for a face before `NO_FACE`. |
| `MIN_FACE_PX` | `80` | Lower it (try 60) if your face is not detected from far away. |
| `FACE_FRAMES_NEEDED` / `FACE_WINDOW` | `5` of `8` | Lower for faster, less strict detection. |
| `SHOW_PREVIEW` | `True` | Set to `False` to hide the camera window. |

In `firmware/sentriface/sentriface.ino`: `SENSOR_ACTIVE_LEVEL`, `LCD_ADDR`, `OLED_ADDR`, `SERVO_OPEN`, and the timing constants at the top.

---

## Troubleshooting

| Problem | Likely cause | Fix |
|---|---|---|
| `Python was not found` | Python isn't installed or not on PATH | Reinstall Python and tick **Add python.exe to PATH**. |
| `pip.exe ... blocked by Application Control` | Managed laptop policy | Use `run.bat` (it runs pip through Python). If still blocked, use a laptop you control. |
| `running scripts is disabled` (PowerShell) | PowerShell execution policy | Use the `.bat` files, or call `.venv\Scripts\python.exe` directly. Never "activate" the venv. |
| `OpenCV 5.x ... no CascadeClassifier` | OpenCV 5 removed the Haar detector | `run.bat` already pins OpenCV 4. Manual fix: `pip install "opencv-python<5"`. |
| `PermissionError` on the COM port | Serial Monitor or another program holds the port | Close the Arduino Serial Monitor and any other terminal using it. |
| `Could not pick a port automatically` | Zero or several serial devices | Read the printed list, then set `PORT = "COMx"`. |
| No COM port in Device Manager | Charge-only cable or missing driver | Use a data cable. Install CP210x or CH340 drivers. |
| Board reboots when Python starts | Windows toggles DTR on port open | Normal. Wait 3 s before testing. |
| `Camera failed to open` | Another app is using the webcam | Close Zoom, Teams, and the Camera app. Try `CAMERA_INDEX = 1`. |
| Camera opens but never grants | Dim light or small face | Face a light source, move closer, lower `MIN_FACE_PX`. |
| Box flickers or appears on non-faces | Haar detector limits | Raise `minNeighbors` (5 to 6) for fewer false boxes. |
| Sensor triggers nonstop or never | Wrong `SENSOR_ACTIVE_LEVEL` | Flip it and re-upload. See "Sensor test". |
| Works once, then never again | IR sensors detect presence, not motion | Step away so the sensor clears, then wait for the 3 s cooldown. |
| LCD blank, backlight on | Wrong I2C address or SDA/SCL swapped | Run the I2C scanner, update `LCD_ADDR`. |
| `OLED not found` on the LCD | OLED wiring or address | Check pins 21/22 and run the scanner. |
| Servo twitches or the board resets | Servo power sag | Power the servo from 5 V / VIN with a shared GND. |
| `Connecting......` upload failure | Board not in flash mode | Hold BOOT during upload. |

---

## Simulation (no hardware)

The ESP32 logic runs in the Wokwi simulator in a browser. The simulator cannot reach your laptop's webcam, so type `ACCESS_GRANTED` in the simulator's serial monitor to stand in for the camera. In the simulator the motion sensor is a PIR, so keep `SENSOR_ACTIVE_LEVEL = HIGH`.

## Planned upgrades

- Recognise specific enrolled faces instead of any face (OpenCV's YuNet and SFace models).
- Access-denied path for unknown faces.
- LEDs, buzzer, and manual button.
- Re-add the Miku OLED animation.
