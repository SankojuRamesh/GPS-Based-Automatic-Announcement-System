
# ESP32-S3 Smart Announcement System

A portable, GPS-based smart announcement system built using the ESP32-S3. The system automatically plays location-based audio announcements, provides a professional dashboard UI, monitors battery status, and is designed for public transportation and public address applications.

---

## Features

- 📍 GPS-based automatic announcements
- 🔊 High-quality audio playback
- 💾 SD Card support for audio files
- 🖥️ TFT dashboard interface
- 🔋 Battery level monitoring
- 📡 GPS signal strength indication
- 🗺️ Route and station management
- 🎛️ GPIO button controls
- ⚡ Low power operation
- 🔄 Automatic station detection
- 📢 Manual announcement mode
- 🧭 Real-time distance calculation
- 🚏 Next station information

---

## Hardware

- ESP32-S3
- GPS Module (L89H / NEO-6M compatible)
- SD Card Module
- Audio Amplifier (PAM8403 / MAX98306)
- 3W or 5W Speaker
- ST7735 TFT Display (128×160)
- Li-Ion Battery Pack
- Battery Management System (BMS)
- Push Buttons

---

## Software Features

- GPS data processing
- Automatic station detection
- Route management
- Audio playback control
- TFT dashboard UI
- Battery monitoring
- SD card file management
- Button event handling
- Power management
- Distance calculation

---

## Project Structure

```
ESP32-S3-Smart-Announcement-System/
│
├── src/
│   ├── main.cpp
│   ├── gps.cpp
│   ├── audio.cpp
│   ├── display.cpp
│   ├── battery.cpp
│   ├── route.cpp
│   └── buttons.cpp
│
├── include/
│
├── lib/
│
├── data/
│   ├── audio/
│   └── routes/
│
├── images/
│
├── docs/
│
└── README.md
```

---

## System Workflow

1. Initialize hardware
2. Load route information
3. Read GPS location
4. Detect nearest station
5. Calculate distance
6. Trigger announcement
7. Update dashboard
8. Repeat until route completion

---

## Applications

- Metro Trains
- Public Buses
- School Buses
- Tourist Vehicles
- Shuttle Services
- Campus Transportation
- Public Address Systems

---

## Future Improvements

- Wi-Fi OTA Updates
- Bluetooth Configuration
- Web Dashboard
- MQTT Support
- Cloud Synchronization
- Multi-language Announcements
- Touchscreen Interface
- AI-based Route Prediction

---

## Requirements

- Arduino IDE 2.x or PlatformIO
- ESP32 Board Package
- TinyGPSPlus
- TFT_eSPI
- SPI
- SD
- Wire

---

## Getting Started

1. Clone the repository

```bash
git clone https://github.com/yourusername/ESP32-S3-Smart-Announcement-System.git
```

2. Open the project in Arduino IDE or PlatformIO.
3. Install the required libraries.
4. Configure the pin assignments.
5. Upload the firmware to the ESP32-S3.
6. Copy audio files to the SD card.
7. Power on the device.

---

## Screenshots

> Add screenshots of:

- Boot Screen
- Dashboard
- Route Selection
- Battery Status
- GPS Status

---

## License

This project is released under the MIT License.

---

## Author

**Ramesh Sankoju**

Embedded Software Engineer

- ESP32 Development
- Embedded Systems
- GPS Applications
- IoT Solutions
- Real-Time Systems

---

## Contributing

Contributions, feature requests, and bug reports are welcome. Feel free to fork the repository and submit a pull request.

---

## Acknowledgements

- Espressif Systems
- Arduino Community
- TinyGPSPlus Library
- TFT_eSPI Library
