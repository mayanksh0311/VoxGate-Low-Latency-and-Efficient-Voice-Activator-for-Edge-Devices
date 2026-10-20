# VoxGate v2 (EchoAccess) 🎙️⚡

**Privacy-First Edge AI Voice Activator for Smart India Hackathon (SIH 2026)**
**Problem Statement:** SIH26172

![VoxGate Thumbnail](docs/voxgate_thumbnail.png)

VoxGate v2 is a 100% offline, privacy-first keyword spotting (KWS) system built for edge devices. By utilizing TinyML techniques on the ESP32-S3, this system continuously listens for a specific wake-word without ever transmitting audio data to the cloud, ensuring complete user privacy, ultra-low latency, and adherence to strict low-BOM constraints.

## 🚀 Key Performance Benchmarks
* **Algorithm:** Depthwise Separable 2D Convolutional Neural Network (DS-CNN)
* **Quantization:** INT8 (via Edge Impulse EON Compiler)
* **Overall Accuracy:** 93.3% (Validation Set)
* **Target Keyword Recognition:** 100%
* **Inference Latency:** ~106 ms
* **Peak RAM Usage:** ~25 KB (Well below the 256 KB SIH constraint)

## 🛠️ Hardware Requirements
* **Microcontroller:** ESP32-S3 DevKitC
* **Microphone:** INMP441 Omnidirectional I2S MEMS Microphone
* **Visual Indicator:** 1x Standard LED (with 220Ω resistor)

### I2S Wiring Diagram
| INMP441 Pin | ESP32-S3 Pin | Function |
| :--- | :--- | :--- |
| **VDD** | 3.3V | Power Supply |
| **GND** | GND | Common Ground |
| **L/R** | GND | Left Channel Selection (pulled low) |
| **SD** | GPIO 4 | I2S Serial Data Out |
| **WS** | GPIO 5 | I2S Word Select / LRCK |
| **SCK** | GPIO 6 | I2S Bit Clock / BCLK |

*See `docs/wiring_diagram.png` for the visual schematic.*

## 📂 Repository Structure
* `/firmware`: Contains the primary `VoxGate_Edge_Firmware.ino` script featuring RMS Voice Activity Detection (VAD) gating and K-of-N temporal smoothing.
* `/models`: Contains the optimized `.zip` Arduino library exported directly from Edge Impulse.
* `/scripts`: Python telemetry scripts for live serial monitoring, colorized terminal outputs, and CSV activation logging.
* `/docs`: Wiring diagrams, performance metrics (confusion matrix), and presentation assets.

## ⚙️ Getting Started

### 1. Install the Inferencing Library
1. Open the Arduino IDE.
2. Navigate to **Sketch** -> **Include Library** -> **Add .ZIP Library...**
3. Select `models/ei-voxgatev2-arduino-1.0.2-impulse-#1.zip`.

### 2. Flash the Firmware
1. Open `firmware/VoxGate_Edge_Firmware/VoxGate_Edge_Firmware.ino` in the Arduino IDE.
2. Select **ESP32S3 Dev Module** from the Boards menu.
3. Compile and upload to your board.

### 3. Run Live Telemetry
To monitor triggers and log activations to a CSV for evaluation:
```bash
cd scripts
pip install -r requirements.txt
python serial_telemetry.py --port <YOUR_COM_PORT>
