// bluetooth_jammer.h
// QUAD-NRF Bluetooth Jammer V8 (4× Firehose)
// 4× NRF24L01+ PA+LNA (+20dBm) + ESP32 BLE (+9dBm)
// ESP8266 WiFi attacks via Serial bridge
// Educational purposes only - jamming is ILLEGAL!

#ifndef BLUETOOTH_JAMMER_H
#define BLUETOOTH_JAMMER_H

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// ============================================================
// Hardware Pin Definitions
// ============================================================

// Number of NRF modules
#define NUM_NRFS        4

// NRF #1 — HSPI bus
#define NRF1_CE_PIN     16
#define NRF1_CSN_PIN    15

// NRF #2 — HSPI bus (shared SCK/MOSI/MISO with NRF #1)
#define NRF2_CE_PIN     17
#define NRF2_CSN_PIN    4

// NRF #3 — VSPI bus
#define NRF3_CE_PIN     25
#define NRF3_CSN_PIN    26

// NRF #4 — VSPI bus (shared SCK/MOSI/MISO with NRF #3)
#define NRF4_CE_PIN     22
#define NRF4_CSN_PIN    21

// HSPI signal pins (NRF #1, #2)
#define HSPI_SCK        14
#define HSPI_MOSI       12    // PCB routes NRF MOSI (pin 6) to GPIO12
#define HSPI_MISO       13    // PCB routes NRF MISO (pin 7) to GPIO13

// VSPI signal pins (NRF #3, #4)
#define VSPI_SCK        18
#define VSPI_MOSI       23
#define VSPI_MISO       19

// ESP8266 Serial bridge (for WiFi deauther commands)
#define ESP8266_TX_PIN  32    // ESP32 TX → ESP8266 RX
#define ESP8266_RX_PIN  35    // ESP32 RX ← ESP8266 TX (input-only pin)
#define ESP8266_BAUD    9600

// GPS Module (moved from GPIO16 to avoid NRF conflict)
#define GPS_RX_PIN      34    // ESP32 RX ← GPS TX (input-only pin)
#define GPS_BAUD        9600

// ============================================================
// Firehose parameters
// ============================================================

// Packets to blast on each channel before hopping.
// More = higher duty per channel but slower hopping.
// For BLE ADV (3 ch): lots of packets = high duty cycle
// For Classic BT (79 ch): more packets = stronger per-channel saturation
#define PKTS_PER_CH_FEW    50    // For 3-14 channel modes
#define PKTS_PER_CH_MANY   15    // For 40+ channel modes (higher dwell = more disruption)

// Burst-fire: packets per update() call
// NRF24 has 3-level TX FIFO — keep it saturated
#define BURST_COUNT        4

// Number of pre-generated noise patterns
#define NUM_NOISE_BUFS     8

// Regenerate noise buffers every N group rotations
// Prevents receiver from adapting to repeating patterns
#define NOISE_REGEN_CYCLES 50

// ============================================================
// Channel group rotation (AFH countermeasure)
// ============================================================
#define GROUP_SIZE         10     // Channels per focus group (concentrated fire works best)
#define GROUP_ROTATE_MS    2000   // Rotate every 2s (faster than AFH reclassify ~1-2s)

// Auto-cycle WITHOUT pause: shift group every 15s, never stop TX
// The group shift causes the "interrupt" effect at cycle start
// but we NEVER stop transmitting
#define CYCLE_ON_MS        10000  // Shift group every 10 seconds
#define CYCLE_OFF_MS       0      // NO pause — continuous TX

// ============================================================
// Jamming modes
// ============================================================
enum JamMode {
  JAM_BLUETOOTH,
  JAM_BLE,
  JAM_BLE_ADV_ONLY,
  JAM_WIFI,
  JAM_ALL,
  JAM_COMBO
};

// ============================================================
// Single NRF24L01+ Radio Instance
// ============================================================
class NRFRadio {
public:
  NRFRadio();
  bool init(SPIClass* spi, uint8_t cePin, uint8_t csnPin, int radioId);
  void startJamming(uint8_t* channels, uint8_t count, uint8_t pktsPerCh);
  void updateChannels(uint8_t* channels, uint8_t count);  // Lightweight swap (no restart)
  void stopJamming();
  void update();  // Blast packets + hop (burst-fire)
  void regenerateNoise();  // Refresh noise buffers (called periodically)

  bool isJamming();
  bool isInitialized();
  unsigned long getPacketsSent();
  unsigned long getHopsPerformed();
  int getId();

private:
  RF24 radio;
  int id;
  bool initialized;
  bool jamming;

  // Channel management
  uint8_t* channelList;
  uint8_t channelCount;
  uint8_t currentChannelIndex;
  uint8_t pktsPerChannel;
  uint8_t pktsSentOnChannel;

  // Stats
  unsigned long packetsSent;
  unsigned long hopsPerformed;

  // Noise buffers
  uint8_t noiseBuffers[NUM_NOISE_BUFS][32];
};

// ============================================================
// Multi-NRF Orchestrator (manages all 4 radios)
// ============================================================
class MultiNRFJammer {
public:
  MultiNRFJammer();
  bool init();
  void startJamming(JamMode mode);
  void stopJamming();
  void update();  // Round-robin update all radios

  bool isJamming();
  bool isInitialized();
  JamMode getMode();
  const char* getModeName();
  unsigned long getTotalPacketsSent();
  unsigned long getTotalHops();
  unsigned long getCycleCount();
  int getActiveRadioCount();

private:
  NRFRadio radios[NUM_NRFS];
  SPIClass* hspi;
  SPIClass* vspi;
  bool initialized;
  bool jamming;
  JamMode currentMode;
  int activeRadios;      // How many NRFs actually initialized
  int updateIndex;       // Round-robin counter

  // Cycling
  unsigned long cycleCount;
  unsigned long cycleStartMs;
  bool firstDivide;      // True on first call (use startJamming)

  // Cumulative counters (survive rotation resets)
  unsigned long cumulativePackets;
  unsigned long cumulativeHops;

  void divideChannels(JamMode mode);
};

// ============================================================
// ESP32 BLE Advertising Flood
// ============================================================
namespace ESP32BLEFlood {
  bool init();
  void start();
  void stop();
  void refreshData();
  bool isRunning();
}

#endif
