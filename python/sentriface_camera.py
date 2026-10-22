"""
SENTRIFACE - PC side (v5)

Serial protocol (115200 baud, one line per message):
  ESP32 -> PC : MOTION_DETECTED   open the camera
  PC -> ESP32 : CAMERA_READY      camera is open, scanning started
  PC -> ESP32 : ACCESS_GRANTED    a face was seen
  PC -> ESP32 : NO_FACE           nobody showed up before the timeout
  ESP32 -> PC : CAMERA_CLOSE      cycle finished, release the camera

Install:  pip install -r requirements.txt   (or just double-click run.bat)
Run:      run.bat   (or: python python/sentriface_camera.py)
Close the Arduino Serial Monitor first (it locks the COM port).
Press q in the camera window (or Ctrl+C) to quit.
"""

import sys
import time
from collections import deque

import cv2
import serial
from serial.tools import list_ports

if not hasattr(cv2, "CascadeClassifier"):
    raise SystemExit(
        f"OpenCV {cv2.__version__} has no face detector (CascadeClassifier).\n"
        'Fix: pip install "opencv-python<5"  (or just use run.bat)')

# ---------------- SETTINGS ----------------
PORT = None            # e.g. "COM5" to force a port; None = auto-detect
BAUD = 115200
CAMERA_INDEX = 0       # try 1 if the wrong camera opens
SCAN_TIMEOUT_S = 15    # give up (NO_FACE) after this long
CAMERA_HOLD_MAX_S = 40 # safety: release camera even if CAMERA_CLOSE never arrives
SHOW_PREVIEW = True

FACE_FRAMES_NEEDED = 5   # face must appear in this many...
FACE_WINDOW = 8          # ...of the last N frames (avoids false triggers)
MIN_FACE_PX = 80


# ---------------- SERIAL ----------------
def find_port():
    if PORT:
        return PORT
    ports = list(list_ports.comports())
    keywords = ("cp210", "ch340", "ch910", "silicon labs", "usb serial", "usb-serial", "esp32")
    for p in ports:
        if any(k in (p.description or "").lower() for k in keywords):
            return p.device
    if len(ports) == 1:
        return ports[0].device
    print("Could not pick a port automatically. Available ports:")
    for p in ports:
        print(f"  {p.device}: {p.description}")
    print('Set PORT = "COMx" at the top of this file.')
    sys.exit(1)


def open_serial(port):
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = BAUD
    ser.timeout = 0
    ser.dtr = False   # avoids resetting the ESP32 on open (usually)
    ser.rts = False
    ser.open()
    return ser


def send(ser, text):
    ser.write((text + "\n").encode())
    print(f"PC  -> ESP32: {text}")


def read_lines(ser, buf):
    """Non-blocking: returns a list of complete lines, keeps partial data in buf."""
    data = ser.read(ser.in_waiting or 1)
    if data:
        buf.extend(data)
    lines = []
    while b"\n" in buf:
        i = buf.index(b"\n")
        line = bytes(buf[:i]).decode(errors="ignore").strip()
        del buf[:i + 1]
        if line:
            lines.append(line)
    return lines


# ---------------- CAMERA ----------------
def open_camera():
    cap = cv2.VideoCapture(CAMERA_INDEX, cv2.CAP_DSHOW)  # DSHOW opens faster on Windows
    if not cap.isOpened():
        cap.release()
        cap = cv2.VideoCapture(CAMERA_INDEX)
    if not cap.isOpened():
        return None
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    for _ in range(5):  # discard dark auto-exposure frames
        cap.read()
    return cap


def main():
    cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    if cascade.empty():
        print("Could not load the face detector.")
        sys.exit(1)

    port = find_port()
    ser = open_serial(port)
    print(f"Listening on {port} @ {BAUD}. Waiting for MOTION_DETECTED...")

    rx = bytearray()
    cap = None
    scanning = False
    scan_start = 0.0
    cam_start = 0.0
    hits = deque(maxlen=FACE_WINDOW)

    def release():
        nonlocal cap, scanning
        if cap is not None:
            cap.release()
            cap = None
        scanning = False
        if SHOW_PREVIEW:
            cv2.destroyAllWindows()
            cv2.waitKey(1)

    try:
        while True:
            # 1) messages from the ESP32
            for line in read_lines(ser, rx):
                print(f"ESP32 -> PC: {line}")
                if line == "MOTION_DETECTED" and cap is None:
                    cap = open_camera()
                    if cap is None:
                        print("Camera failed to open (is another app using it?).")
                        send(ser, "NO_FACE")
                    else:
                        hits.clear()
                        scanning = True
                        scan_start = cam_start = time.time()
                        send(ser, "CAMERA_READY")
                elif line == "CAMERA_CLOSE":
                    release()

            # 2) camera work
            if cap is not None:
                ok, frame = cap.read()
                if ok:
                    if scanning:
                        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                        faces = cascade.detectMultiScale(
                            gray, scaleFactor=1.2, minNeighbors=5,
                            minSize=(MIN_FACE_PX, MIN_FACE_PX))
                        hits.append(1 if len(faces) else 0)
                        for (x, y, w, h) in faces:
                            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

                        if sum(hits) >= FACE_FRAMES_NEEDED:
                            send(ser, "ACCESS_GRANTED")
                            scanning = False
                        elif time.time() - scan_start > SCAN_TIMEOUT_S:
                            send(ser, "NO_FACE")
                            scanning = False

                    if SHOW_PREVIEW:
                        label = "Scanning..." if scanning else "Done - waiting for ESP32"
                        cv2.putText(frame, label, (10, 30), cv2.FONT_HERSHEY_SIMPLEX,
                                    0.8, (0, 255, 255), 2)
                        cv2.imshow("SENTRIFACE camera", frame)

                if time.time() - cam_start > CAMERA_HOLD_MAX_S:
                    print("Safety timeout: releasing camera.")
                    release()

            if SHOW_PREVIEW:
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break
            else:
                time.sleep(0.01)

    except KeyboardInterrupt:
        pass
    finally:
        release()
        ser.close()
        print("Stopped.")


if __name__ == "__main__":
    main()
