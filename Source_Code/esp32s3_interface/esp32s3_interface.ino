// ESP32-S3 Master UI Dashboard (ILI9341/ST7789)
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <SD.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// Disp Cfg:
#define USE_ILI9341    0   // ILI9341 
#define USE_ST7789     1   // ST7789
#define INVERT_COLORS  0   // Inv Col

#if USE_ILI9341
  #include <Adafruit_ILI9341.h>
#else
  #include <Adafruit_ST7789.h>
#endif

// Pins
#define TFT_CS 10
#define TFT_RST 21
#define TFT_DC 14
#define TFT_MOSI 11
#define TFT_SCK 12
#define TFT_BL 15
#define SD_SCK 38
#define SD_MOSI 39
#define SD_MISO 40
#define SD_CS 41
#define GPS_RX 18
#define JOY_UP 1
#define JOY_DOWN 2
#define JOY_LEFT 4
#define JOY_RIGHT 5
#define JOY_CENTER 6
#define UART32U_TX 17
#define UART32U_RX 16
#define ESP8266_TX 7
#define ESP8266_RX 8

// Objs
SPIClass spiSD(HSPI);
SPIClass tftSPI(FSPI);

#if USE_ILI9341
  Adafruit_ILI9341 tft = Adafruit_ILI9341(&tftSPI, TFT_DC, TFT_CS, TFT_RST);
#else
  Adafruit_ST7789 tft = Adafruit_ST7789(&tftSPI, TFT_CS, TFT_DC, TFT_RST);
#endif

TinyGPSPlus gps;
SoftwareSerial gpsSerial(GPS_RX, -1);
HardwareSerial uart32u(2);
HardwareSerial esp8266(1);

// UI Grid
#define COL1_X 0
#define COL1_W 104
#define COL2_X 104
#define COL2_W 100
#define COL3_X 204
#define COL3_W 116
#define HDR_H 30
#define BODY_Y 30
#define BODY_H 170
#define FTR_Y 200
#define FTR_H 40

// Colors
#define C_BG    0x0000  // Blk
#define C_WHITE 0xFFFF  // Wht
#define C_FRAME 0x4A49  // D-Gry
#define C_STAR  0x2945  // Stars

// Menu Colors
#define C_BLE   0x237D  // BLE (Purp)
#define C_WIFI  0xFF47  // WiFi (Org)
#define C_SCAN  0x14A1  // Scan (Grn)
#define C_EVIL  0xB882  // Evil (Red)
#define C_UTIL  0x07F7  // Util (Cyn)
#define C_SEL   0x780F  // Sel (Blk)

// Menu Items
int curSel = 0;
const int N_ITEMS = 5;
const char* mLabel[] = {"BLE Attacks", "Wi-Fi Attacks", "Scanning", "Evil-USB stuff", "Utils"};
const uint16_t mColor[] = {C_BLE, C_WIFI, C_SCAN, C_EVIL, C_UTIL};
bool sdOK = false;
bool joyU=false, joyD=false, joyC=false;

// Skull bmp
static const unsigned char PROGMEM image_paint_6_bits[] = {0x80};
static const unsigned char PROGMEM image_skull_bits[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x3f,0xfe,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x3f,0xfe,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x01,0xff,0xff,0xc0,0x00,0x0f,0xff,0xff,0xf0,0x00,0x01,0xff,0xff,0xc0,0x00,0x0f,0xff,0xff,0xf0,0x00,0x1f,0xff,0xff,0xfc,0x00,0x0f,0xff,0xff,0xf0,0x00,0x1f,0xff,0xff,0xfc,0x00,0x0f,0xff,0xff,0xf0,0x00,0x7f,0xff,0xff,0xff,0x00,0x0f,0xff,0xff,0xf0,0x01,0xff,0xff,0xff,0xff,0x80,0x0f,0xff,0xff,0xf0,0x01,0xff,0xff,0xff,0xff,0x80,0x0f,0xff,0xff,0xf0,0x03,0xff,0xff,0xff,0xff,0xe0,0x0f,0xff,0xff,0xf0,0x03,0xff,0xff,0xff,0xff,0xe0,0x0f,0xff,0xff,0xf0,0x03,0xff,0xff,0xff,0xff,0xe0,0x0f,0xff,0xff,0xf0,0x03,0xff,0xff,0xff,0xff,0xe0,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0xff,0xff,0xff,0xff,0xf8,0x0f,0xff,0xff,0xf0,0x0e,0x7f,0xff,0xff,0xff,0x38,0x0f,0xff,0xff,0xf0,0x0e,0x7f,0xff,0xff,0xff,0x38,0x0f,0xff,0xff,0xf0,0x0e,0x7f,0x03,0xe0,0x7f,0x38,0x0f,0xff,0xff,0xf0,0x0f,0x9f,0xfc,0x9f,0xfc,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0x9f,0xfc,0x9f,0xfc,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0x9f,0xff,0xff,0xfc,0xf8,0x0f,0xff,0xff,0xf0,0x0f,0x9f,0xff,0xff,0xfc,0xf8,0x0f,0xff,0xff,0xf0,0x03,0x98,0x0e,0x78,0x0c,0xe0,0x0f,0xff,0xff,0xf0,0x03,0x98,0x0e,0x78,0x0c,0xe0,0x0f,0xff,0xff,0xf0,0x02,0x60,0x00,0x80,0x03,0x20,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x80,0x00,0x00,0x0f,0xff,0xff,0xf0,0x01,0x80,0x03,0xe0,0x00,0x80,0x0f,0xff,0xff,0xf0,0x01,0x80,0x03,0xe0,0x00,0x80,0x0f,0xff,0xff,0xf0,0x01,0x80,0x03,0xe0,0x00,0x80,0x0f,0xff,0xff,0xf0,0x03,0x80,0x0e,0x78,0x00,0xe0,0x0f,0xff,0xff,0xf0,0x03,0x80,0x0e,0x78,0x00,0xe0,0x0f,0xff,0xff,0xf0,0x03,0xe0,0x7c,0x1f,0x03,0xe0,0x0f,0xff,0xff,0xf0,0x03,0xe0,0x7c,0x1f,0x03,0xe0,0x0f,0xff,0xff,0xf0,0x03,0x9f,0x0c,0x18,0x7c,0xe0,0x0f,0xff,0xff,0xf0,0x03,0x80,0x0c,0x18,0x00,0xe0,0x0f,0xff,0xff,0xf0,0x03,0xe0,0x7c,0x1f,0x03,0xe0,0x0f,0xff,0xff,0xf0,0x01,0xfe,0x7c,0x9f,0x3f,0x80,0x0f,0xff,0xff,0xf0,0x01,0xfe,0x7c,0x9f,0x3f,0x80,0x0f,0xff,0xff,0xf0,0x00,0x79,0xff,0xff,0xcf,0x00,0x0f,0xff,0xff,0xf0,0x00,0x79,0xff,0xff,0xcf,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x7f,0xff,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x7f,0xff,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x4e,0x79,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x4e,0x79,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x4e,0x79,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x00,0x00,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x00,0x00,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x4e,0x79,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x18,0x4e,0x79,0x0c,0x00,0x0f,0xff,0xff,0xf0,0x00,0x06,0x4e,0x79,0x30,0x00,0x0f,0xff,0xff,0xf0,0x00,0x04,0xce,0x79,0x30,0x00,0x0f,0xff,0xff,0xf0,0x00,0x01,0xff,0xff,0xc0,0x00,0x0f,0xff,0xff,0xf0,0x00,0x01,0xff,0xff,0xc0,0x00,0x0f,0xff,0xff,0xf0,0x00,0x01,0xff,0xff,0xc0,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x3f,0xfe,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x3f,0xfe,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

// 8x8 Icons
const unsigned char ico_bt[] PROGMEM  = {0x08,0x0C,0x2A,0x1C,0x1C,0x2A,0x0C,0x08}; // BT
const unsigned char ico_wf[] PROGMEM  = {0x3C,0x42,0x18,0x24,0x00,0x18,0x18,0x00}; // WiFi
const unsigned char ico_sc[] PROGMEM  = {0x3C,0x42,0x99,0xA5,0xA5,0x99,0x42,0x3C}; // Scan
const unsigned char ico_hw[] PROGMEM  = {0x42,0x7E,0xDB,0xFF,0xBD,0x7E,0x3C,0x24}; // HW
const unsigned char ico_ge[] PROGMEM  = {0xC3,0x66,0x3C,0x18,0x18,0x3C,0x66,0xC3}; // Gear
const unsigned char* mIcon[] = {ico_bt, ico_wf, ico_sc, ico_hw, ico_ge};

// Batt bmp
const unsigned char ico_batt[] PROGMEM = {
  0xFF,0xFC, 0x80,0x06, 0xBF,0xF6, 0xBF,0xF6, 0xBF,0xF6,
  0xBF,0xF6, 0xBF,0xF6, 0xBF,0xF6, 0x80,0x06, 0xFF,0xFC
};

// Stars
void drawStars(int x0, int y0, int w, int h) {
  for (int i = 0; i < 15; i++) {
    int sx = x0 + ((i * 37 + 13) % w);
    int sy = y0 + ((i * 53 + 7) % h);
    if (sx > x0+1 && sx < x0+w-2 && sy > y0+1 && sy < y0+h-2) {
      tft.drawPixel(sx, sy, C_STAR);
      tft.drawPixel(sx-1, sy, C_STAR);
      tft.drawPixel(sx+1, sy, C_STAR);
      tft.drawPixel(sx, sy-1, C_STAR);
      tft.drawPixel(sx, sy+1, C_STAR);
    }
  }
}

// Corners
void drawCorners(int x, int y, int w, int h, int sz, uint16_t col) {
  // TL
  tft.drawLine(x, y, x+sz, y, col);
  tft.drawLine(x, y, x, y+sz, col);
  // TR
  tft.drawLine(x+w-1-sz, y, x+w-1, y, col);
  tft.drawLine(x+w-1, y, x+w-1, y+sz, col);
  // BL
  tft.drawLine(x, y+h-1, x+sz, y+h-1, col);
  tft.drawLine(x, y+h-1-sz, x, y+h-1, col);
  // BR
  tft.drawLine(x+w-1-sz, y+h-1, x+w-1, y+h-1, col);
  tft.drawLine(x+w-1, y+h-1-sz, x+w-1, y+h-1, col);
}

// Header
void drawHeader() {
  // Brackets
  drawCorners(0, 0, 320, 240, 12, C_WHITE);
  
  // Box
  tft.drawRect(5, 5, 310, 23, C_WHITE);
  
  // Battery icon header
  tft.drawBitmap(12, 11, ico_batt, 16, 10, C_WHITE);
  
  // Title
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(34, 13);
  tft.print("WiFI & Bluetooth Slayer >");
  
  // Stat box
  for (int i = 0; i < 3; i++) {
    tft.drawRect(255 + i*17, 10, 12, 13, C_WHITE);
  }
  
  // Divider
  tft.drawLine(0, HDR_H-1, 319, HDR_H-1, C_FRAME);
}

// L-Pane
void drawLeftPane() {
  // Divider
  tft.drawLine(COL1_W, BODY_Y, COL1_W, FTR_Y-1, C_WHITE);
  
  // Stars
  drawStars(5, BODY_Y+5, COL1_W-10, BODY_H-10);
  
  // Skull
  int sx = (COL1_W - 80) / 2; 
  int sy = BODY_Y + 35;      
  tft.drawBitmap(sx, sy, image_skull_bits, 80, 80, C_WHITE);
  
  // Masking
  tft.fillRect(sx, sy + 76, 80, 4, C_BG);      
  tft.fillRect(sx + 68, sy, 12, 76, C_BG);     
  tft.fillRect(sx - 1, sy, 69, 4, C_BG);       
  tft.fillRect(sx, sy + 4, 12, 73, C_BG);      

 
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, BODY_Y + 22); tft.print("D");
  tft.setCursor(28, BODY_Y + 16); tft.print("3");
  tft.setCursor(37, BODY_Y + 12); tft.print("D");
  tft.setCursor(46, BODY_Y + 11); tft.print("S");
  tft.setCursor(55, BODY_Y + 12); tft.print("K");
  tft.setCursor(64, BODY_Y + 16); tft.print("U");
  tft.setCursor(72, BODY_Y + 22); tft.print("L");
  tft.setCursor(80, BODY_Y + 29); tft.print("L");
  
  // paint 6
  tft.drawBitmap(sx + 30, sy + 63, image_paint_6_bits, 1, 1, C_WHITE);
  
  // Bottom Box
  tft.drawRect(5, BODY_Y + 130, COL1_W - 10, 30, C_WHITE);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(12, BODY_Y + 141);
  tft.print("WifiBT_EN1.0");
}

// M-Pane
void drawMiddlePane() {
  // Divider
  tft.drawLine(COL2_X + COL2_W, BODY_Y, COL2_X + COL2_W, FTR_Y-1, C_WHITE);
  
  // Stars
  drawStars(COL2_X+5, BODY_Y+5, COL2_W-10, BODY_H-10);
  
  // Sys Info
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(COL2_X + 15, BODY_Y + 8);
  tft.print("System Info");
  
  // Radar grid
  int cx = COL2_X + COL2_W/2;
  int cy = BODY_Y + 75;
  tft.drawCircle(cx, cy, 25, C_FRAME);
  tft.drawCircle(cx, cy, 15, C_FRAME);
  tft.drawLine(cx-28, cy, cx+28, cy, C_FRAME);
  tft.drawLine(cx, cy-28, cx, cy+28, C_FRAME);
  tft.drawPixel(cx, cy, C_WHITE);
  
  // Telemetry
  tft.setTextColor(C_FRAME);
  tft.setCursor(COL2_X+8, BODY_Y + BODY_H - 40);
  tft.print("CPU:240MHz");
  tft.setCursor(COL2_X+8, BODY_Y + BODY_H - 28);
  tft.print("RAM:OK");
  tft.setCursor(COL2_X+8, BODY_Y + BODY_H - 16);
  tft.print("SPI:40MHz");
}

// Menu Item
void drawMenuItem(int i, bool sel) {
  int yPos = BODY_Y + 8 + (i * 32);
  
  // Clr Row
  tft.fillRect(COL3_X+1, yPos, COL3_W-2, 28, C_BG);
  
  if (sel) {
    // Highlight
    tft.fillRect(COL3_X+1, yPos, COL3_W-2, 28, C_SEL);
    tft.drawBitmap(COL3_X+5, yPos+10, mIcon[i], 8, 8, C_WHITE);
    tft.setTextColor(C_WHITE);
  } else {
    tft.drawBitmap(COL3_X+5, yPos+10, mIcon[i], 8, 8, C_WHITE);
    tft.setTextColor(mColor[i]);
  }
  tft.setTextSize(1);
  tft.setCursor(COL3_X+18, yPos+10);
  tft.print(mLabel[i]);
}

// R-Pane
void drawRightPane() {
  // Stars
  drawStars(COL3_X+5, BODY_Y+5, COL3_W-10, BODY_H-10);
  
  for (int i = 0; i < N_ITEMS; i++)
    drawMenuItem(i, i == curSel);
}

// Footer
void drawFooter() {
  tft.drawLine(0, FTR_Y, 319, FTR_Y, C_WHITE);
  
  // Live Telem
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(12, FTR_Y+15);
  tft.print("SD:"); 
  tft.setTextColor(sdOK ? C_UTIL : C_WIFI);
  tft.print(sdOK ? "ONLINE" : "OFFLINE");
  
  // Divider
  tft.drawLine(COL1_W, FTR_Y, COL1_W, 239, C_WHITE);
  
  // Nav
  tft.setTextColor(C_FRAME);
  tft.setCursor(COL1_W+8, FTR_Y+15);
  tft.print("UP/DN: Navig  |  CTR: Activate");
}

// UI scratch
void drawFullUI() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawLeftPane();
  drawMiddlePane();
  drawRightPane();
  drawFooter();
}

// Radar Anim
float radAngle = 0;
unsigned long lastRad = 0;

void updateRadar() {
  unsigned long now = millis();
  if (now - lastRad < 60) return;
  lastRad = now;
  int cx = COL2_X + COL2_W/2;
  int cy = BODY_Y + 75;
  // Erase old
  tft.drawLine(cx, cy, cx+(int)(cos(radAngle)*14), cy+(int)(sin(radAngle)*14), C_BG);
  // Rest grid
  tft.drawLine(cx-28, cy, cx+28, cy, C_FRAME);
  tft.drawLine(cx, cy-28, cx, cy+28, C_FRAME);
  // Draw new
  float a = (now % 3000) / 3000.0 * 2 * PI;
  tft.drawLine(cx, cy, cx+(int)(cos(a)*14), cy+(int)(sin(a)*14), C_BLE);
  tft.drawPixel(cx, cy, C_WHITE);
  radAngle = a;
}

void setupPeripherals() {
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  if (SD.begin(SD_CS, spiSD, 1000000)) sdOK = true;
  else { delay(500); if (SD.begin(SD_CS, spiSD, 1000000)) sdOK = true; }
  gpsSerial.begin(9600);
  uart32u.begin(9600, SERIAL_8N1, UART32U_RX, UART32U_TX);   // 9600 to match slave_32u
  esp8266.begin(9600, SERIAL_8N1, ESP8266_RX, ESP8266_TX);    // 9600 to match slave_8266
}

void setup() {
  Serial.begin(115200); delay(1000);
  pinMode(JOY_UP, INPUT_PULLUP);
  pinMode(JOY_DOWN, INPUT_PULLUP);
  pinMode(JOY_LEFT, INPUT_PULLUP);
  pinMode(JOY_RIGHT, INPUT_PULLUP);
  pinMode(JOY_CENTER, INPUT_PULLUP);
  
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH); delay(100);  // Rst High
  digitalWrite(TFT_RST, LOW);  delay(150);  // Rst Low
  digitalWrite(TFT_RST, HIGH); delay(300);  // Wait
  
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
  
  // Init TFT
  #if USE_ILI9341
    tft.begin(40000000); // Initialize ILI9341 SPI at 40MHz
  #else
    tft.init(240, 320);  // Initialize ST7789 SPI
    tft.setSPISpeed(40000000);
  #endif
  
  tft.setRotation(1);  // Landscape
  
  #if INVERT_COLORS
    tft.invertDisplay(true);
  #else
    tft.invertDisplay(false);
  #endif
  
  // Boot SCR
  Serial.println("Initializing TFT...");
  tft.fillScreen(C_BG);
  tft.setTextColor(C_WHITE); tft.setTextSize(1);
  tft.setCursor(20, 100); tft.print("> SLAYER CORE INITIALIZATION...");
  Serial.println("> SLAYER CORE INITIALIZATION...");
  
  setupPeripherals();
  
  tft.setCursor(20, 115);
  tft.setTextColor(sdOK ? C_UTIL : C_WIFI);
  tft.print(sdOK ? "> SD: ONLINE" : "> SD: OFFLINE");
  Serial.println(sdOK ? "> SD: ONLINE" : "> SD: OFFLINE");
  delay(500);
  
  Serial.println("Starting UI...");
  drawFullUI();
}

void processBackgroundTasks() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  while (uart32u.available()) uart32u.read();
  while (esp8266.available()) esp8266.read();
}

bool pingModule(HardwareSerial& serial) {
  // Clr RX
  while(serial.available()) serial.read();
  
  // Ping
  serial.println("ping");
  
  // Wait RX
  unsigned long t = millis();
  while(millis() - t < 500) {
    if(serial.available()) {
      // Clr RX
      while(serial.available()) serial.read();
      return true;
    }
    delay(5);
  }
  return false;
}

// Submenu
void runSubMenu() {
  tft.fillScreen(C_BG);
  drawCorners(0, 0, 320, 240, 12, mColor[curSel]);
  
  tft.drawBitmap(14, 8, ico_batt, 16, 10, C_WHITE);
  tft.setTextColor(C_WHITE); tft.setTextSize(1);
  tft.setCursor(36, 10); tft.print("Slayer > ");
  tft.setTextColor(mColor[curSel]); tft.print(mLabel[curSel]);
  tft.drawLine(0, HDR_H-1, 319, HDR_H-1, mColor[curSel]);
  
  tft.drawRect(15, 40, 290, 160, mColor[curSel]);
  drawStars(20, 45, 280, 150);
  
  tft.setTextColor(mColor[curSel]); tft.setCursor(25, 55);
  
  if (curSel == 3) {
    tft.print("=== HW DIAGNOSTICS ===");
    tft.setTextColor(C_WHITE);
    tft.setCursor(25, 75); tft.printf("SD Card .... %s", sdOK?"ONLINE":"OFFLINE");
    tft.setCursor(25, 90); tft.printf("GPS Sats ... %d", gps.satellites.value());
    // Ping UART
    bool u32 = pingModule(uart32u);
    bool u82 = pingModule(esp8266);
    
    tft.setCursor(25, 105); tft.printf("UART-32U ... %s", u32 ? "ACTIVE" : "NO RESP");
    tft.setCursor(25, 120); tft.printf("UART-8266 .. %s", u82 ? "ACTIVE" : "NO RESP");
    tft.setCursor(25, 135); tft.print("TFT ........ ILI9341 OK");
    tft.setCursor(25, 150); tft.printf("Heap ....... %d B", ESP.getFreeHeap());
  } else if (curSel == 4) {
    tft.print("=== CONFIG ===");
    tft.setTextColor(C_WHITE);
    tft.setCursor(25, 75); tft.print("Board: ESP32-S3 WROOM-1");
    tft.setCursor(25, 90); tft.print("Display: 240x320 2.4\" TFT");
    tft.setCursor(25, 105); tft.print("SPI: 40MHz Hardware SPI");
  } else {
    tft.print("=== MODULE STANDBY ===");
    tft.setTextColor(C_FRAME);
    tft.setCursor(25, 80); tft.print("Awaiting activation command...");
  }
  
  tft.setTextColor(C_FRAME);
  tft.setCursor(25, 210); tft.print("> CENTER to return");
  
  delay(200);
  while(!digitalRead(JOY_CENTER)) { processBackgroundTasks(); delay(10); }
  delay(50);
  while(digitalRead(JOY_CENTER)) { processBackgroundTasks(); delay(10); }
  delay(50);
  while(!digitalRead(JOY_CENTER)) { processBackgroundTasks(); delay(10); }
  drawFullUI();
}

void loop() {
  processBackgroundTasks();
  updateRadar();
  
  bool cu = !digitalRead(JOY_UP);
  bool cd = !digitalRead(JOY_DOWN);
  bool cc = !digitalRead(JOY_CENTER);
  
  if (cu && !joyU) {
    int old = curSel;
    curSel = (curSel - 1 + N_ITEMS) % N_ITEMS;
    drawMenuItem(old, false);
    drawMenuItem(curSel, true);
  }
  joyU = cu;
  
  if (cd && !joyD) {
    int old = curSel;
    curSel = (curSel + 1) % N_ITEMS;
    drawMenuItem(old, false);
    drawMenuItem(curSel, true);
  }
  joyD = cd;
  
  if (cc && !joyC) runSubMenu();
  joyC = cc;
  
  delay(10);
}
