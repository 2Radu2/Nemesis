# Project Nemesis: Modular Wireless Auditor

## Legal Disclaimer
**This project is for educational and academic research purposes only.**

Project Nemesis was developed as a university thesis project to audit, stress-test, and evaluate the resilience of modern wireless protocols (IEEE 802.11 and Bluetooth) in strictly controlled and isolated laboratory environments.

The author does not endorse, encourage, or take responsibility for any illegal, malicious, or unauthorized use of this hardware or software. Transmitting jamming signals or executing deauthentication attacks against networks or devices without explicit, written permission from the network owner is a federal crime in many jurisdictions (including under FCC regulations in the United States and similar laws worldwide).

By downloading, viewing, or building this project, you agree that you are solely responsible for your own actions and that you will comply with all local, state, and federal laws.

---

##  Overview
Project Nemesis is a portable, offensive security embedded system designed to audit and stress-test the resilience of modern wireless networks (Wi-Fi & Bluetooth).

Unlike standard auditing tools that rely on a single microcontroller (which often leads to severe UI lag and firmware bottlenecks during heavy RF injection), this project utilizes a custom **3-Stack Hardware Architecture**:
- **Layer 1 (Master Controller):** An ESP32-S3 handling the TFT UI, joystick navigation, and SD-Card GPS logging, completely isolated from RF tasks to ensure zero UI lag.
- **Layer 2 (Bluetooth Disruption):** An ESP32-32U paired with a Quad-nRF24L01+PA+LNA array. It executes protocol-level BLE advertising floods and brute-force AFH (Adaptive Frequency Hopping) poisoning across 79 channels.
- **Layer 3 (Wi-Fi Attacker):** An ESP8266 utilizing the raw Espressif SDK to execute 802.11 deauthentication and beacon spam attacks.

By synchronizing these independent layers, the system can execute "Combo Max"—a simultaneous, multi-vector attack capable of completely saturating the 2.4 GHz spectrum while maintaining a perfectly fluid user interface.

##  Hardware Components
This project relies on a custom 3-stack PCB architecture to distribute processing power and prevent UI bottlenecks. Below is the full bill of materials (BOM):

### Layer 1: Master Controller & UI
*   **Microcontroller:** ESP32-S3 (Dev Module)
*   **Display:** SPI TFT LCD Screen (e.g., ILI9341)
*   **Input:** 5-Pin Analog/Digital Joystick Module
*   **Telemetry:** GPS Module (u-blox NEO-6M / 7M)
*   **Storage:** MicroSD Card Adapter (SPI) + MicroSD Card

### Layer 2: Bluetooth Jammer
*   **Microcontroller:** ESP32-WROOM-32U (with U.FL connector for external BLE antenna)
*   **RF Array:** 4x nRF24L01+PA+LNA Transceiver Modules
*   **Antennas:** 5x 2.4GHz SMA Antennas (4 for NRF modules, 1 for ESP32 BLE)
*   **Power Smoothing:** 4x 10µF - 100µF Capacitors (essential for NRF module power stability)

### Layer 3: Wi-Fi Attacker
*   **Microcontroller:** ESP8266 (NodeMCU / Wemos D1 Mini)

### Power Delivery & PCB
*   **Battery:** 2500mAh Li-Po Battery (must support high current discharge ~1A+)
*   **Power Regulation:** High-current 3.3V and 5V Voltage Regulators
*   **Custom PCBs:** 3x Custom-designed Gerber boards (included in the /Hardware folder)
