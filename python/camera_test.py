"""
SENTRIFACE camera test - no ESP32 needed.
Uses the same detection logic as sentriface_camera.py.

Install:  pip install -r requirements.txt   (or just double-click test_camera.bat)
Run:      test_camera.bat   (or: python python/camera_test.py)
Keys:     q = quit,  n = try next camera index
"""

import time
from collections import deque

import cv2

if not hasattr(cv2, "CascadeClassifier"):
    raise SystemExit(
        f"OpenCV {cv2.__version__} has no face detector (CascadeClassifier).\n"
        'Fix: pip install "opencv-python<5"  (or just use run.bat)')

CAMERA_INDEX = 0
FACE_FRAMES_NEEDED = 5
FACE_WINDOW = 8
MIN_FACE_PX = 80


def open_cam(index):
    cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap.release()
        cap = cv2.VideoCapture(index)
    if cap.isOpened():
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        for _ in range(5):
            cap.read()
        return cap
    cap.release()
    return None


def main():
    print("OpenCV version:", cv2.__version__)
    cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    if cascade.empty():
        print("FAIL: face detector file could not be loaded.")
        return

    index = CAMERA_INDEX
    cap = open_cam(index)
    if cap is None:
        print(f"FAIL: camera {index} did not open. Close Zoom/Teams/Camera app and retry, "
              "or change CAMERA_INDEX.")
        return
    print(f"Camera {index} opened. Look at the camera. Press q to quit.")

    hits = deque(maxlen=FACE_WINDOW)
    granted = False
    t0, frames, fps = time.time(), 0, 0.0

    while True:
        ok, frame = cap.read()
        if not ok:
            print("FAIL: could not read a frame.")
            break

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = cascade.detectMultiScale(gray, 1.2, 5, minSize=(MIN_FACE_PX, MIN_FACE_PX))
        hits.append(1 if len(faces) else 0)
        for (x, y, w, h) in faces:
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

        now_granted = sum(hits) >= FACE_FRAMES_NEEDED
        if now_granted and not granted:
            print("ACCESS_GRANTED would be sent now")
        granted = now_granted

        frames += 1
        if time.time() - t0 >= 1:
            fps, frames, t0 = frames, 0, time.time()

        status = "ACCESS GRANTED" if granted else "No face"
        color = (0, 255, 0) if granted else (0, 0, 255)
        cv2.putText(frame, f"{status}  | faces: {len(faces)}  | fps: {fps}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
        cv2.imshow("SENTRIFACE camera test", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        if key == ord("n"):
            cap.release()
            index += 1
            cap = open_cam(index)
            if cap is None:
                index = 0
                cap = open_cam(0)
            print("Switched to camera", index)
            hits.clear()

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
