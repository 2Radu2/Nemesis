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
*   **Antennas:** 4x 2.4GHz SMA Antennas + 1x 2.4Ghz U.FL (4 for NRF modules, 1 for ESP32 BLE)
*   **Power Smoothing:** 4x 10µF - 100µF Capacitors (essential for NRF module power stability, connected on the VCC& GND of the nRF24L01)

### Layer 3: Wi-Fi Attacker
*   **Microcontroller:** ESP8266 (NodeMCU / Wemos D1 Mini)

### Power Delivery & PCB
*   **Battery:** 2500mAh Li-Po Battery (must support high current discharge ~1A+)
*   **Power Regulation:** High-current 3.3V and 5V Voltage Regulators (LDOs/Buck converters capable of handling >1A peaks)
*   **Custom PCBs:** 3x Custom-designed Gerber boards (included in the `/Hardware` folder)
*   **Connectors:** 2.54mm Pitch Male/Female Headers for stacking
