// bluetooth_jammer.cpp
// QUAD-NRF Bluetooth Jammer V8 — 4× Noise Packet Firehose
// Educational purposes only - jamming is ILLEGAL!
//
// ARCHITECTURE:
// - 4× NRF24L01+ PA+LNA on dual SPI buses (HSPI + VSPI)
// - Each NRF targets a different channel group
// - MultiNRFJammer orchestrates all 4 with round-robin updates
// - ESP32 BLE flood runs simultaneously on internal radio
//
// CHANNEL DIVISION (Classic BT, 79 channels):
//   NRF #1 → ch  0-19  (20 channels)
//   NRF #2 → ch 20-39  (20 channels)
//   NRF #3 → ch 40-59  (20 channels)
//   NRF #4 → ch 60-78  (19 channels)
// = 4x coverage vs single NRF. AFH cannot escape.

#include "bluetooth_jammer.h"
#include <BLEDevice.h>
#include <BLEAdvertising.h>

// ============================================================
// Channel definitions
// ============================================================

// BLE advertising + adjacent (NRF channel numbers)
static uint8_t bleAdvExpandedChannels[] = {
  1, 2, 3, 25, 26, 27, 79, 80, 81
};

// BLE data channels (all 40)
static uint8_t bleAllChannels[] = {
   2,  4,  6,  8, 10, 12, 14, 16, 18, 20,
  22, 24, 26, 28, 30, 32, 34, 36, 38, 40,
  42, 44, 46, 48, 50, 52, 54, 56, 58, 60,
  62, 64, 66, 68, 70, 72, 74, 76, 78, 80
};

// Classic BT (79 channels)
static uint8_t btClassicChannels[79];
static bool btClassicInit = false;

// WiFi channels
static uint8_t wifiChannels[] = {
  12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72, 84
};

// Full band
static uint8_t fullBandChannels[126];
static bool fullBandInit = false;

// Init channel arrays
static void initChannelArrays() {
  if (!btClassicInit) {
    for (int i = 0; i < 79; i++) btClassicChannels[i] = i + 2;
    btClassicInit = true;
  }
  if (!fullBandInit) {
    for (int i = 0; i < 126; i++) fullBandChannels[i] = i;
    fullBandInit = true;
  }
}


// ============================================================
// ===== SINGLE NRF RADIO =====
// ============================================================

NRFRadio::NRFRadio()
  : radio(RF24_SPI_SPEED),
    id(0),
    initialized(false),
    jamming(false),
    channelList(nullptr),
    channelCount(0),
    currentChannelIndex(0),
    pktsPerChannel(PKTS_PER_CH_MANY),
    pktsSentOnChannel(0),
    packetsSent(0),
    hopsPerformed(0)
{
}

bool NRFRadio::init(SPIClass* spi, uint8_t cePin, uint8_t csnPin, int radioId) {
  id = radioId;

  // Re-initialize the RF24 object with correct pins
  radio = RF24(cePin, csnPin);

  Serial.printf("[NRF#%d] Init CE=%d CSN=%d...\n", id, cePin, csnPin);

  if (!radio.begin(spi)) {
    Serial.printf("[NRF#%d] [FAIL]\n", id);
    return false;
  }

  // Configure for maximum packet throughput
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setPayloadSize(32);
  radio.setAddressWidth(3);

  uint8_t addr[] = { 0xFF, 0xFF, 0xFF };
  radio.openWritingPipe(addr);

  regenerateNoise();

  initialized = true;

  if (radio.isChipConnected()) {
    Serial.printf("[NRF#%d] [OK] Ready | +20dBm | 2MBPS\n", id);
  } else {
    Serial.printf("[NRF#%d] [WARN] Init OK but chip not responding - check power!\n", id);
  }
  return true;
}

void NRFRadio::regenerateNoise() {
  for (int b = 0; b < NUM_NOISE_BUFS; b++) {
    for (int i = 0; i < 32; i++) {
      noiseBuffers[b][i] = random(0, 256);
    }
  }
}

void NRFRadio::startJamming(uint8_t* channels, uint8_t count, uint8_t pktsPerCh) {
  if (!initialized) return;

  channelList = channels;
  channelCount = count;
  pktsPerChannel = pktsPerCh;
  currentChannelIndex = 0;
  pktsSentOnChannel = 0;
  packetsSent = 0;
  hopsPerformed = 0;

  radio.powerUp();
  radio.stopListening();
  radio.flush_tx();
  radio.setChannel(channelList[0]);

  jamming = true;

  Serial.printf("[NRF#%d] [ACTIVE] Jamming %d channels, %d pkt/ch\n",
                id, count, pktsPerCh);
}

// Lightweight channel swap — NO radio restart, NO counter reset
// Used during group rotation to seamlessly switch channels
void NRFRadio::updateChannels(uint8_t* channels, uint8_t count) {
  if (!initialized || !jamming) return;

  channelList = channels;
  channelCount = count;
  // Keep currentChannelIndex valid
  if (currentChannelIndex >= channelCount) {
    currentChannelIndex = 0;
  }
  // Immediately switch to the new channel at current index
  radio.setChannel(channelList[currentChannelIndex]);
}

void NRFRadio::stopJamming() {
  if (!jamming) return;
  jamming = false;
  radio.flush_tx();
  radio.powerDown();
  Serial.printf("[NRF#%d] [STOP] %lu pkts, %lu hops\n",
                id, packetsSent, hopsPerformed);
}

void NRFRadio::update() {
  if (!jamming || !initialized) return;

  // === NON-BLOCKING FIRE ===
  // startFastWrite loads FIFO + CE HIGH without busy-waiting.
  // If FIFO is full, the packet is silently dropped — fine for jamming!
  // This eliminates the 800ms txStandBy bottleneck.
  uint8_t bufIdx = packetsSent % NUM_NOISE_BUFS;
  radio.startFastWrite(noiseBuffers[bufIdx], 32, true, true);
  packetsSent++;
  pktsSentOnChannel++;

  // After N packets, hop to next channel
  if (pktsSentOnChannel >= pktsPerChannel) {
    // Flush remaining FIFO packets (they're on the old channel)
    radio.flush_tx();

    currentChannelIndex++;
    if (currentChannelIndex >= channelCount) {
      currentChannelIndex = 0;
    }
    radio.setChannel(channelList[currentChannelIndex]);

    pktsSentOnChannel = 0;
    hopsPerformed++;
  }
}

bool NRFRadio::isJamming() { return jamming; }
bool NRFRadio::isInitialized() { return initialized; }
unsigned long NRFRadio::getPacketsSent() { return packetsSent; }
unsigned long NRFRadio::getHopsPerformed() { return hopsPerformed; }
int NRFRadio::getId() { return id; }


// ============================================================
// ===== MULTI-NRF ORCHESTRATOR =====
// ============================================================

MultiNRFJammer::MultiNRFJammer()
  : hspi(nullptr),
    vspi(nullptr),
    initialized(false),
    jamming(false),
    currentMode(JAM_BLUETOOTH),
    activeRadios(0),
    updateIndex(0),
    cycleCount(0),
    cycleStartMs(0),
    firstDivide(true),
    cumulativePackets(0),
    cumulativeHops(0)
{
  initChannelArrays();
}

bool MultiNRFJammer::init() {
  Serial.println("[MULTI-NRF] Initializing 4× NRF24L01+...");

  // CRITICAL FIX: Deselect ALL NRF modules immediately before any SPI traffic.
  // If a CSN pin is left floating LOW before its specific radio.init() is called, 
  // that NRF module will "listen" to the SPI bus and drive the shared MISO line,
  // causing data collisions that make the FIRST module fail to initialize.
  pinMode(NRF1_CSN_PIN, OUTPUT); digitalWrite(NRF1_CSN_PIN, HIGH);
  pinMode(NRF2_CSN_PIN, OUTPUT); digitalWrite(NRF2_CSN_PIN, HIGH);
  pinMode(NRF3_CSN_PIN, OUTPUT); digitalWrite(NRF3_CSN_PIN, HIGH);
  pinMode(NRF4_CSN_PIN, OUTPUT); digitalWrite(NRF4_CSN_PIN, HIGH);

  // Let all 4 PA+LNA modules stabilize on the 3.3V rail before init.
  // Without this, simultaneous inrush current (~600mA peak) collapses
  // the rail and causes the first modules to fail.
  Serial.println("[MULTI-NRF] Waiting for 3.3V rail to stabilize...");
  delay(500);

  // Create HSPI bus (NRF #1, #2)
  hspi = new SPIClass(HSPI);
  hspi->begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, -1);

  // Create VSPI bus (NRF #3, #4)
  vspi = new SPIClass(VSPI);
  vspi->begin(VSPI_SCK, VSPI_MISO, VSPI_MOSI, -1);

  activeRadios = 0;

  // Stagger each NRF init by 200ms to avoid simultaneous current spikes.
  // 4× PA+LNA modules each draw ~150mA peak — all at once = 3.3V sag.
  if (radios[0].init(hspi, NRF1_CE_PIN, NRF1_CSN_PIN, 1)) activeRadios++;
  delay(200);
  if (radios[1].init(hspi, NRF2_CE_PIN, NRF2_CSN_PIN, 2)) activeRadios++;
  delay(200);
  if (radios[2].init(vspi, NRF3_CE_PIN, NRF3_CSN_PIN, 3)) activeRadios++;
  delay(200);
  if (radios[3].init(vspi, NRF4_CE_PIN, NRF4_CSN_PIN, 4)) activeRadios++;

  Serial.printf("[MULTI-NRF] %d/%d radios online\n", activeRadios, NUM_NRFS);

  initialized = (activeRadios > 0);

  if (!initialized) {
    Serial.println("[MULTI-NRF] [FAIL] No radios initialized! Check wiring.");
  }

  return initialized;
}

// ============================================================
// Channel Grouping — V7 AFH Poisoning Strategy
// ============================================================
// Instead of spreading all channels thin, ALL radios focus on
// the SAME group of GROUP_SIZE channels. This gives concentrated
// duty per channel (5-10%+) which crosses the AFH detection
// threshold. Groups rotate every GROUP_ROTATE_MS to poison all
// channel groups over time.
// ============================================================

// Static storage for channel groups
static uint8_t dividedChannels[NUM_NRFS][126];
static uint8_t dividedCount[NUM_NRFS];
static uint8_t groupOffset = 0;  // Current group start offset

void MultiNRFJammer::divideChannels(JamMode mode) {
  uint8_t* masterList = nullptr;
  uint8_t masterCount = 0;
  uint8_t pktsPerCh = PKTS_PER_CH_MANY;

  switch (mode) {
    case JAM_BLE_ADV_ONLY:
      masterList = bleAdvExpandedChannels;
      masterCount = 9;
      pktsPerCh = PKTS_PER_CH_FEW;
      break;
    case JAM_BLE:
      masterList = bleAllChannels;
      masterCount = 40;
      pktsPerCh = PKTS_PER_CH_MANY;
      break;
    case JAM_BLUETOOTH:
    case JAM_COMBO:
      masterList = btClassicChannels;
      masterCount = 79;
      pktsPerCh = PKTS_PER_CH_MANY;
      break;
    case JAM_WIFI:
      masterList = wifiChannels;
      masterCount = 14;
      pktsPerCh = PKTS_PER_CH_FEW;
      break;
    case JAM_ALL:
      masterList = fullBandChannels;
      masterCount = 126;
      pktsPerCh = PKTS_PER_CH_MANY;
      break;
  }

  memset(dividedCount, 0, sizeof(dividedCount));

  // For large channel sets (BT Classic 79ch, Full Band 126ch):
  // Use V7 channel grouping — focus ALL radios on GROUP_SIZE channels
  if (masterCount > GROUP_SIZE) {
    // Pick GROUP_SIZE channels starting at groupOffset
    uint8_t groupChannels[GROUP_SIZE];
    for (int i = 0; i < GROUP_SIZE; i++) {
      groupChannels[i] = masterList[(groupOffset + i) % masterCount];
    }

    // ALL active radios get the SAME channel group
    // = concentrated fire = high duty per channel = AFH poisoning
    // BUT stagger starting positions so radios hit DIFFERENT channels
    // within the group at any instant → multi-channel coverage
    int radioSlot = 0;
    for (int r = 0; r < NUM_NRFS; r++) {
      if (radios[r].isInitialized()) {
        // Rotate the channel order for each radio so they start at
        // different offsets: radio0→ch0, radio1→ch2, radio2→ch5, etc.
        int stagger = (radioSlot * GROUP_SIZE) / max(activeRadios, 1);
        for (int i = 0; i < GROUP_SIZE; i++) {
          dividedChannels[r][i] = groupChannels[(i + stagger) % GROUP_SIZE];
        }
        dividedCount[r] = GROUP_SIZE;
        radioSlot++;
      }
    }

    // Advance group offset for next rotation
    groupOffset = (groupOffset + GROUP_SIZE) % masterCount;

  } else {
    // For small channel sets (BLE 9ch, WiFi 14ch, BLE 40ch):
    // Already concentrated enough — all radios get all channels
    int radioSlot = 0;
    for (int r = 0; r < NUM_NRFS; r++) {
      if (radios[r].isInitialized()) {
        memcpy(dividedChannels[r], masterList, masterCount);
        dividedCount[r] = masterCount;
        radioSlot++;
      }
    }
  }

  // First time: use startJamming (full init). After that: just swap channels.
  if (firstDivide) {
    for (int r = 0; r < NUM_NRFS; r++) {
      if (radios[r].isInitialized() && dividedCount[r] > 0) {
        radios[r].startJamming(dividedChannels[r], dividedCount[r], pktsPerCh);
      }
    }
    firstDivide = false;
  } else {
    // Lightweight swap — radios keep firing, just new channel list
    for (int r = 0; r < NUM_NRFS; r++) {
      if (radios[r].isJamming() && dividedCount[r] > 0) {
        radios[r].updateChannels(dividedChannels[r], dividedCount[r]);
      }
    }
  }
}

void MultiNRFJammer::startJamming(JamMode mode) {
  if (!initialized) return;

  currentMode = mode;
  jamming = true;
  cycleCount = 0;
  cycleStartMs = millis();
  updateIndex = 0;
  firstDivide = true;
  cumulativePackets = 0;
  cumulativeHops = 0;

  const char* modeName = getModeName();
  Serial.printf("\n[MULTI-NRF] [ACTIVE] STARTING: %s | %d radios\n", modeName, activeRadios);

  divideChannels(mode);
}

extern HardwareSerial MasterSerial;

void MultiNRFJammer::stopJamming() {
  if (!jamming) return;

  // Accumulate final counts before stopping
  for (int r = 0; r < NUM_NRFS; r++) {
    cumulativePackets += radios[r].getPacketsSent();
    cumulativeHops += radios[r].getHopsPerformed();
    if (radios[r].isJamming()) radios[r].stopJamming();
  }

  jamming = false;
  Serial.printf("[MULTI-NRF]  All stopped. Total: %lu pkts, %lu hops, %lu cycles\n",
                cumulativePackets, cumulativeHops, cycleCount);
  
  // Broadcast to S3 for the Summary Dashboard
  MasterSerial.printf("Total: %lu pkts, %lu hops, %lu cycles\n",
                cumulativePackets, cumulativeHops, cycleCount);
}

// ============================================================
// FIRE ALL — every active radio fires every loop() call
// ============================================================
// Channel groups rotate every GROUP_ROTATE_MS (3s) to poison
// all AFH groups over time. This matches the V7 strategy that
// achieved sustained disruption.
// ============================================================

void MultiNRFJammer::update() {
  if (!jamming || !initialized) return;

  // Group rotation every GROUP_ROTATE_MS (2 sec)
  // Faster than AFH reclassify interval (~1-2s) to keep pressure on.
  // Poisoned channels take ~2s to recover, so we cycle back in time.
  unsigned long elapsed = millis() - cycleStartMs;
  if (elapsed >= GROUP_ROTATE_MS) {
    cycleStartMs = millis();
    cycleCount++;

    // Regenerate noise buffers periodically to prevent receiver adaptation
    if (cycleCount % NOISE_REGEN_CYCLES == 0) {
      for (int r = 0; r < NUM_NRFS; r++) {
        if (radios[r].isInitialized()) {
          radios[r].regenerateNoise();
        }
      }
    }

    divideChannels(currentMode);
    Serial.printf("[MULTI-NRF] [CYCLE] %lu - group rotated (10ch focused)\n", cycleCount);
  }

  // SPI bus interleaving: alternate between HSPI and VSPI radios
  // to avoid bus contention. Fire pattern: HSPI→VSPI→HSPI→VSPI
  // radios[0]=HSPI, radios[1]=HSPI, radios[2]=VSPI, radios[3]=VSPI
  static const int fireOrder[NUM_NRFS] = {0, 2, 1, 3};  // HSPI,VSPI,HSPI,VSPI
  for (int i = 0; i < NUM_NRFS; i++) {
    int idx = fireOrder[i];
    if (radios[idx].isJamming()) {
      radios[idx].update();
    }
  }
}

// ============================================================
// Status
// ============================================================

bool MultiNRFJammer::isJamming() { return jamming; }
bool MultiNRFJammer::isInitialized() { return initialized; }
JamMode MultiNRFJammer::getMode() { return currentMode; }
int MultiNRFJammer::getActiveRadioCount() { return activeRadios; }
unsigned long MultiNRFJammer::getCycleCount() { return cycleCount; }

const char* MultiNRFJammer::getModeName() {
  switch (currentMode) {
    case JAM_BLUETOOTH:    return "BT Classic";
    case JAM_BLE:          return "BLE All";
    case JAM_BLE_ADV_ONLY: return "BLE Adv";
    case JAM_WIFI:         return "WiFi";
    case JAM_ALL:          return "Full 2.4G";
    case JAM_COMBO:        return "COMBO";
    default:               return "?";
  }
}

unsigned long MultiNRFJammer::getTotalPacketsSent() {
  unsigned long total = cumulativePackets;
  for (int r = 0; r < NUM_NRFS; r++) {
    total += radios[r].getPacketsSent();
  }
  return total;
}

unsigned long MultiNRFJammer::getTotalHops() {
  unsigned long total = cumulativeHops;
  for (int r = 0; r < NUM_NRFS; r++) {
    total += radios[r].getHopsPerformed();
  }
  return total;
}


// ============================================================
// ===== ESP32 BLE ADVERTISING FLOOD =====
// ============================================================

namespace ESP32BLEFlood {

static BLEAdvertising* pAdvertising = nullptr;
static bool _running = false;
static bool _initialized = false;

bool init() {
  Serial.println("[ESP32-BLE] Initializing...");
  BLEDevice::init("J");
  BLEDevice::setPower(ESP_PWR_LVL_P9);

  pAdvertising = BLEDevice::getAdvertising();
  if (!pAdvertising) {
    Serial.println("[ESP32-BLE] [FAIL]");
    return false;
  }

  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);

  _initialized = true;
  Serial.println("[ESP32-BLE] [OK] Ready | +9dBm");
  return true;
}

void start() {
  if (!_initialized || !pAdvertising) {
    Serial.println("[ESP32-BLE] [FAIL] Not initialized!");
    return;
  }

  BLEAdvertisementData advData;
  char name[16];
  for (int i = 0; i < 15; i++) name[i] = 'A' + random(0, 26);
  name[15] = 0;
  advData.setName(name);

  BLEAdvertisementData scanData;
  char name2[16];
  for (int i = 0; i < 15; i++) name2[i] = 'a' + random(0, 26);
  name2[15] = 0;
  scanData.setName(name2);

  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanData);
  pAdvertising->start();
  _running = true;
  Serial.println("[ESP32-BLE] [ACTIVE] Flood!");
}

void stop() {
  if (!_initialized || !pAdvertising) return;
  pAdvertising->stop();
  _running = false;
  Serial.println("[ESP32-BLE] [STOP] Stopped");
}

void refreshData() {
  if (!_initialized || !_running || !pAdvertising) return;

  pAdvertising->stop();

  BLEAdvertisementData advData;
  char name[16];
  for (int i = 0; i < 15; i++) name[i] = 'A' + random(0, 26);
  name[15] = 0;
  advData.setName(name);

  BLEAdvertisementData scanData;
  char name2[16];
  for (int i = 0; i < 15; i++) name2[i] = 'a' + random(0, 26);
  name2[15] = 0;
  scanData.setName(name2);

  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanData);
  pAdvertising->start();
}

bool isRunning() { return _running; }

} // namespace ESP32BLEFlood
