// ESP32 QUAD-NRF Jammer | SPI: NRF1-4 | UART: ESP8266

#include "bluetooth_jammer.h"

MultiNRFJammer nrf;

HardwareSerial MasterSerial(2);

unsigned long lastStats = 0;
unsigned long bleRefreshTimer = 0;

static String masterBuffer = "";

// Diag
static unsigned long loopCount = 0;
static unsigned long diagTimer = 0;
static unsigned long totalNrfUs = 0;
static unsigned long totalBleUs = 0;
static unsigned long totalSerialUs = 0;
static unsigned long worstLoopUs = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("╔═════════════════════════════════════════╗");
  Serial.println("║  BLUETOOTH JAMMER V9 — QUAD NRF          ║");
  Serial.println("║  4× NRF24 (+20dBm) + ESP32 BLE (+9dBm)    ║");
  Serial.println("║  + ESP8266 WiFi Attack Bridge             ║");
  Serial.println("╚═════════════════════════════════════════╝");
  Serial.println();

  MasterSerial.begin(9600, SERIAL_8N1, 35, 32); // UART
  MasterSerial.setTimeout(5);  // TO=5
  Serial.println("[BRIDGE] S3 Master connection ready (9600 baud)");

  if (!ESP32BLEFlood::init()) {
    Serial.println("[!] BLE init failed — NRF24 only mode");
  }

  if (!nrf.init()) {
    Serial.println("[!] No NRF modules initialized! Check wiring.");
  }

  Serial.println();
  printMenu();
}

void loop() {
  unsigned long loopStart = micros();

  if (Serial.available()) {
    char cmd = Serial.read();
    while (Serial.available()) Serial.read();
    handleCommand(cmd);
  }

  unsigned long t0 = micros();
  while (MasterSerial.available()) {
    char c = (char)MasterSerial.read();
    if (c == '\n') {
      if (masterBuffer.length() > 0) {
        masterBuffer.trim();
        if (masterBuffer == "ping") {
          MasterSerial.println("pong");
        } else if (masterBuffer == "info") {
          MasterSerial.printf("INFO:NRF:%d\n", nrf.getActiveRadioCount());
        } else {
          Serial.print("[MASTER] ");
          Serial.println(masterBuffer);
          handleCommand(masterBuffer.charAt(0));
        }
        masterBuffer = "";
      }
    } else if (c != '\r') {
      masterBuffer += c;
      if (masterBuffer.length() > 200) {
        masterBuffer = "";
      }
    }
  }
  unsigned long serialUs = micros() - t0;
  totalSerialUs += serialUs;

  unsigned long t1 = micros();
  nrf.update();
  unsigned long nrfUs = micros() - t1;
  totalNrfUs += nrfUs;

  unsigned long t2 = micros();
  if (ESP32BLEFlood::isRunning() && millis() - bleRefreshTimer >= 2000) {
    bleRefreshTimer = millis();
    ESP32BLEFlood::refreshData();
  }
  unsigned long bleUs = micros() - t2;
  totalBleUs += bleUs;

  unsigned long loopUs = micros() - loopStart;
  if (loopUs > worstLoopUs) worstLoopUs = loopUs;
  loopCount++;
  
  // Diag
  if (nrf.isJamming() && millis() - diagTimer >= 5000) {
    unsigned long elapsed = millis() - diagTimer;
    diagTimer = millis();
    float hz = (float)loopCount / ((float)elapsed / 1000.0f);
    Serial.printf("[DIAG] %.1f Hz | NRF:%luus BLE:%luus Serial:%luus | worst:%luus | loops:%lu\n",
                  hz, totalNrfUs/loopCount, totalBleUs/loopCount, totalSerialUs/loopCount,
                  worstLoopUs, loopCount);
    loopCount = 0;
    totalNrfUs = 0;
    totalBleUs = 0;
    totalSerialUs = 0;
    worstLoopUs = 0;
  }

  // Telem
  if (nrf.isJamming() && millis() - lastStats >= 1000) {
    lastStats = millis();
    printStats();
  }
}

void printMenu() {
  Serial.println("┌──────────────────────────────────────────┐");
  Serial.println("│        BLUETOOTH JAMMER MENU             │");
  Serial.println("├──────────────────────────────────────────┤");
  Serial.println("│  1 = Classic BT Jamming (79 ch)          │");
  Serial.println("│  2 = BLE Pairing Jammer (9 ch)           │");
  Serial.println("│  3 = COMBO MAX (All Radios Firing)       │");
  Serial.println("│                                          │");
  Serial.println("│  s = STOP ALL                            │");
  Serial.println("│  ? = Menu                                │");
  Serial.println("└──────────────────────────────────────────┘");
  Serial.println();
}

void handleCommand(char cmd) {
  switch (cmd) {
    case '1': stopAll(); nrf.startJamming(JAM_BLUETOOTH); break;
    case '2': stopAll(); nrf.startJamming(JAM_BLE_ADV_ONLY); break;

    case '3':
      stopAll();
      Serial.println("\n [ACTIVE] COMBO MAX — ALL RADIOS FIRING \n");
      ESP32BLEFlood::start();
      delay(200);
      nrf.startJamming(JAM_COMBO);
      Serial.println("[COMBO] ESP32 -> BLE ADV flood (ch 37,38,39)");
      Serial.printf("[COMBO] %dx NRF24 -> Noise firehose (79 BT ch)\n",
                    nrf.getActiveRadioCount());
      break;

    case 's': case 'S':
      stopAll();
      break;

    case '?':
      printMenu();
      break;

    default:
      if (cmd != '\n' && cmd != '\r')
        Serial.printf("? '%c' — press '?' for menu\n", cmd);
      break;
  }
}

void stopAll() {
  if (nrf.isJamming()) nrf.stopJamming();
  if (ESP32BLEFlood::isRunning()) ESP32BLEFlood::stop();
}

void printStats() {
  Serial.printf("[STATS] [%s] NRFs:%d Pkts:%lu Hops:%lu Cyc:%lu BLE:%s\n",
                nrf.getModeName(),
                nrf.getActiveRadioCount(),
                nrf.getTotalPacketsSent(),
                nrf.getTotalHops(),
                nrf.getCycleCount(),
                ESP32BLEFlood::isRunning() ? "ON" : "off");
                
  MasterSerial.printf("STATUS:BT:%lu:%lu:%lu\n",
                nrf.getTotalPacketsSent(),
                nrf.getTotalHops(),
                nrf.getCycleCount());
}
