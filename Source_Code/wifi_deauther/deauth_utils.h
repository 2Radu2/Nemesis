// deauth_utils.h
// ESP8266 WiFi Attack Utilities
// Custom implementation for NodeMCU v3 (CH340C)
// Ported from ESP32 wifi_utils — uses ESP8266 SDK raw frame TX
// Educational purposes only — unauthorized use is ILLEGAL!

#ifndef DEAUTH_UTILS_H
#define DEAUTH_UTILS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>

extern "C" {
  #include "user_interface.h"
}

// ============================================================
// Scan result structure
// ============================================================
struct WiFiTarget {
  String ssid;
  String bssid_str;
  uint8_t bssid[6];
  uint8_t channel;
  int32_t rssi;
};

// ============================================================
// Deauth Attack Engine
// ============================================================
class DeauthEngine {
public:
  DeauthEngine();

  // Scanning
  int scanNetworks();
  WiFiTarget getTarget(int index);
  int getNetworkCount();

  // Attacks
  void startDeauth(uint8_t* bssid, uint8_t channel);
  void startBeaconFlood(const char* fakeName, uint8_t channel);
  void stop();

  // Continuous attack loop (call from loop())
  void update();

  // Status
  bool isActive();
  const char* getModeName();
  unsigned long getPacketsSent();
  unsigned long getBurstCount();

private:
  // Attack state
  enum AttackMode { ATK_IDLE, ATK_DEAUTH, ATK_BEACON };
  AttackMode mode;

  uint8_t targetBSSID[6];
  uint8_t targetChannel;
  String beaconSSID;

  // Stats
  unsigned long packetsSent;
  unsigned long burstCount;
  unsigned long lastBurstTime;

  // Deauth config
  static const int PKTS_PER_BURST = 50;
  static const int BURST_DELAY_MS = 10;
  static const int BURST_INTERVAL_MS = 500;

  // Internal methods
  void sendDeauthPacket(const uint8_t* bssid, const uint8_t* sta);
  void sendDeauthBurst(const uint8_t* bssid, const uint8_t* sta, int count);
  void sendBeaconPacket(const char* ssid, uint8_t channel);
  void parseBSSID(const char* str, uint8_t* out);
  void enableRawTX();

  // Scan results
  WiFiTarget* scanResults;
  int scanCount;
};

#endif // DEAUTH_UTILS_H
