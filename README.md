# esp32-ml-vibration-analysis
this project implements an Edge-AI solution to solve these industrial challenges. By processing high-frequency MPU6050 vibration telemetry directly on the ESP32-S3 microcontroller, we eliminate the need for costly cloud bandwidth, providing a low-power, high-efficiency system tailored for modern smart factories.
___

# ESP32-S3 FFT Vibration Monitor
 <img width="720" height="1604" alt="WhatsApp Image 2026-05-18 at 8 55 36 PM" src="https://github.com/user-attachments/assets/0dca4f3a-e21b-42f8-85ab-43b04dec612c" />

Real-time vibration frequency analysis using **ESP32-S3 SuperMini** + **MPU6050** accelerometer.  
Collects 256 samples at 500 Hz, computes FFT, and displays a live frequency spectrum chart in any browser over WiFi — no app, no installation, no internet required.

## Hardware
 
| Component | Details |
|---|---|
| Microcontroller | ESP32-S3 SuperMini |
| Sensor | MPU6050 / MPU9250 (I2C) |
| Power | USB or 3.3V |
 
### Wiring
 
```
MPU6050       ESP32-S3 SuperMini
────────       ──────────────────
VCC       →   3.3V
GND       →   GND
SDA       →   GPIO 8
SCL       →   GPIO 9
AD0       →   GND  (I2C address auto-detected)
```
 
> **Note:** AD0 to GND = address `0x68`. AD0 to 3.3V = `0x69`.  
> `mpu.begin()` auto-detects both — no hardcoding needed.
 
---

### Required Libraries
 
Install all via **Arduino IDE → Library Manager**:
 
| Library | Author | Version |
|---|---|---|
| Adafruit MPU6050 | Adafruit | latest |
| Adafruit Unified Sensor | Adafruit | latest (auto-dependency) |
| arduinoFFT | Enrique Condes | **v2.x** (not v1.x) |
 
> `ArduinoJson` is **not needed** in this version — JSON is built manually.
 
### Board Settings (Arduino IDE)
 
| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC on Boot | Enabled |
| CPU Frequency | 240 MHz |
| Upload Mode | USB-OTG or UART |
 
---

## Setup
 
**1. Clone the repo**
```bash
git clone https://github.com/yourusername/esp32-fft-vibration-monitor.git
```
 
**2. Open in Arduino IDE**
```
esp32_fft_webserver.ino
```
 
**3. Set your WiFi credentials**
```cpp
const char* SSID     = "YOUR_SSID";
const char* PASSWORD = "YOUR_PASSWORD";
```
 
**4. Upload to ESP32-S3**
 
Select the correct COM port and upload.
 
**5. Open Serial Monitor** at `115200` baud
 
```
=== ESP32-S3 FFT WebServer ===
[OK] MPU6050 ready
[WiFi] Connecting.......
[OK] Connected → http://192.168.1.42
[OK] Server started
```
 
**6. Open the IP in any browser on the same network**
 
---
