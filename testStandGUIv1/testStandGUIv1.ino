// GUI Design Architecture.
//Main Screen:
//   Thrust Test Stand          Connect to WiFi Network (button)     //
//    
//    Throttle: (motor speed)
//
//    Load: (Load cell value)
//
//    Voltage: (Voltage pull)
//
//    Current: (Current draw)
//
//    Tare Load Cell (button)       Set Test Time (popup)
//











#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

TFT_eSPI tft;

#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define MAX_NETS 50
#include <WiFiUdp.h>

WiFiUDP udp;
const int UDP_PORT = 4210;
IPAddress sensorIP; // optional if you want reply targeting

XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);


float thrust = 0;
float voltage = 0;
float current = 0;
int throttle = 0;
int testTime = 5;
bool testTimePopupUp = false;

bool popupVisible = false;
String popupMessage = "";
bool mainMenu = true;

//TOUCHSCREEN VALUES
//Top Right: approx (370, 260)
//Top Left: approx (3900, 260)
//Bottom Right: approx (370, 3860)
//Bottom Left: approx (3900, 3860)

const int TOUCH_MIN_X = 370;
const int TOUCH_MAX_X = 3900;

const int TOUCH_MIN_Y = 260;
const int TOUCH_MAX_Y = 3860;

struct WiFiNet {
  String ssid;
  int rssi;
};

WiFiNet availableNetworks[MAX_NETS];

int netCount = 0;

// Menus
void drawMainScreen();
void drawWifiConnectScreen();
void updateMainMenuValues();

// Buttons
void Buttons(int x, int y);

// WiFi
bool connectToWiFiNetwork(String ssid, String password);
void scanWiFiNetworks();
String getCurrentWifiNetwork();
void createWiFiConnection(int netNumber);

// Popups
void testTimePopup();
void showPopup(String message);
void clearPopup();

String onScreenKeyboard(
    int boxX,
    int boxY,
    int boxW,
    int boxH
);

int getTouchX(){
    TS_Point p = touch.getPoint();

    int x = map(
        p.x,
        TOUCH_MAX_X,
        TOUCH_MIN_X,
        0,
        320
    );

    return constrain(x, 0, 319);
}

int getTouchY(){
    TS_Point p = touch.getPoint();

    int y = map(
        p.y,
        TOUCH_MIN_Y,
        TOUCH_MAX_Y,
        0,
        240
    );

    return constrain(y, 0, 239);
}

void setup() {
  Serial.begin(115200); 

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.init();
  tft.setRotation(1);

  drawMainScreen();

  SPI.begin(14, 12, 13, 33);

  touch.begin();
  touch.setRotation(1);

  WiFi.mode(WIFI_AP_STA);
  // connectToWiFiNetwork("NETGEAR94", "widepotato326");
  if (WiFi.softAP("ThrustStand")) {
      Serial.println("AP Started");
  } else {
      Serial.println("AP Failed");
  }

  Serial.println("=== AP INFO ===");

  Serial.print("SSID: ");
  Serial.println(WiFi.softAPSSID());

  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.print("Channel: ");
  Serial.println(WiFi.channel());

  Serial.print("MAC: ");
  Serial.println(WiFi.softAPmacAddress());

  Serial.print("Num stations connected: ");
  Serial.println(WiFi.softAPgetStationNum());

  udp.begin(UDP_PORT);
  Serial.print("Listening on UDP port: ");
  Serial.println(UDP_PORT);

  server.on("/", handleRoot);

  server.on("/status", handleStatus);

  server.on("/tare", handleTare);

  server.on("/setTestTime", handleSetTestTime);

  server.on("/scanWifi", handleScanWifi);

  server.on("/connectWifi", handleConnectWifi);

  server.begin();

  delay(200);
}

void loop() {
  server.handleClient();  
  getData();
  if (!testTimePopupUp && mainMenu){
    updateMainMenuValues();
  }
  

  if (touch.touched()){
    int x = getTouchX();
    int y = getTouchY();
    Buttons(x, y);
    
  }

  delay(100);
}