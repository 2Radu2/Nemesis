# Nemesis Hardware Pinout & Wiring Guide

This document provides the final, corrected pin mappings for the 3-stack PCB architecture, exactly as defined in the official thesis documentation. Use this as the definitive guide when wiring breadboards or soldering the PCBs.

---

## 1. ESP32-S3 (Master Controller)
*The central orchestrator and UI hub.*

### TFT LCD Display (SPI)
| Pin | ESP32-S3 GPIO |
| :--- | :--- |
| **MOSI** | `GPIO 11` |
| **SCK** | `GPIO 12` |
| **CS** | `GPIO 10` |
| **RST** | `GPIO 21` |
| **DC** | `GPIO 14` |
| **BL** (Backlight) | `GPIO 15` |

### MicroSD Card Adapter (SPI)
| Pin | ESP32-S3 GPIO |
| :--- | :--- |
| **MOSI** | `GPIO 39` |
| **MISO** | `GPIO 40` |
| **SCK** | `GPIO 38` |
| **CS** | `GPIO 41` |

### Peripheral Inputs
| Component | ESP32-S3 GPIO |
| :--- | :--- |
| **GPS Module (NEO-6M) RX** | `GPIO 18` |
| **Joystick UP** | `GPIO 1` |
| **Joystick DOWN** | `GPIO 2` |
| **Joystick LEFT** | `GPIO 4` |
| **Joystick RIGHT** | `GPIO 5` |
| **Joystick CENTER** | `GPIO 6` |

### Serial Comms to Slaves (UART)
| Slave | S3 TX Pin | S3 RX Pin |
| :--- | :--- | :--- |
| **To ESP32-32U (Bluetooth)** | `GPIO 17` | `GPIO 16` |
| **To ESP8266 (Wi-Fi)** | `GPIO 7` | `GPIO 8` |

> [!NOTE]
> *Page 40 of the thesis text accidentally references IO32 and IO35 for the Wi-Fi bridge, but Table 1 and the final schematic confirm it is actually **7 and 8**!*

---

## 2. ESP32-WROOM-32U (Bluetooth Jammer)
*The dedicated slave handling the quad-nRF array and BLE floods.*

### Serial Comms to Master
| Pin | ESP32-32U GPIO | Connects To |
| :--- | :--- | :--- |
| **RX** | `GPIO 35` | S3 TX (`GPIO 17`) |
| **TX** | `GPIO 32` | S3 RX (`GPIO 16`) |

### Shared SPI Buses for NRF Modules
To prevent data collisions during high-speed AFH poisoning, the 4 radios are split across two separate hardware SPI buses.

| Bus | Radios | MOSI | MISO | SCK |
| :--- | :--- | :--- | :--- | :--- |
| **HSPI** | NRF 1 & 2 | `13` | `12` | `14` |
| **VSPI** | NRF 3 & 4 | `23` | `19` | `18` |

### NRF Individual Control Pins (CE & CSN)
| Module | CE Pin | CSN Pin |
| :--- | :--- | :--- |
| **NRF 1** | `GPIO 16` | `GPIO 15` |
| **NRF 2** | `GPIO 17` | `GPIO 4` |
| **NRF 3** | `GPIO 25` | `GPIO 26` |
| **NRF 4** | `GPIO 22` | `GPIO 21` |

---

## 3. ESP8266 (Wi-Fi Attacker)
*The dedicated Wi-Fi Deauthentication and Beacon Flood coprocessor.*

### Serial Comms to Master
| Pin | ESP8266 Pin | Connects To |
| :--- | :--- | :--- |
| **RX** | `D1 (GPIO 5)` | S3 TX (`GPIO 7`) |
| **TX** | `D2 (GPIO 4)` | S3 RX (`GPIO 8`) |

> [!IMPORTANT]
> **UART Crossover Rule:** When connecting the master to the slaves, you must cross the lines. The **TX** on the Master goes to the **RX** on the slave, and vice-versa.
