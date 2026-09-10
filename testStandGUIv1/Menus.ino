// ----- WiFi -----
String getCurrentWifiNetwork();
void scanWiFiNetworks();

// ----- Globals -----
extern int testTime;
extern int netCount;

extern float thrust;
extern float voltage;
extern float current;
extern int throttle;

extern WiFiNet availableNetworks[];

void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, 150, 30, TFT_RED);

  tft.setTextColor(TFT_WHITE);
  tft.drawString("Thrust Test Stand", 10, 8, 2);

  tft.fillSmoothRoundRect(170, 0, 140, 30, 5, TFT_RED);

  tft.setTextColor(TFT_WHITE);
  tft.drawString("Connect WiFi", 180, 8, 2);

  tft.setTextColor(TFT_WHITE);

  tft.drawString("Throttle:", 10, 50, 2);
  tft.drawString("Thrust:",   10, 90, 2);
  tft.drawString("Voltage:",  10, 130, 2);
  tft.drawString("Current:",  10, 170, 2);

  tft.fillSmoothRoundRect(10, 200, 120, 30, 5, TFT_RED);
  tft.drawString("Tare Load Cell", 20, 208, 2);

  tft.fillSmoothRoundRect(140, 200, 170, 30, 5, TFT_RED);
  tft.drawString("Test Time: "+String(testTime)+" secs", 150, 208, 2);
  tft.fillTriangle(
    290,  207,
    280,  222,
    300,  222,
    TFT_GREEN
  );
}

void updateMainMenuValues() {
  tft.fillRect(140, 45, 150, 30, TFT_BLACK);
  tft.drawString(String(throttle)+"%", 140, 50, 2);

  tft.fillRect(140, 85, 150, 30, TFT_BLACK);
  tft.drawString(String(thrust,1) + " g", 140, 90, 2);

  tft.fillRect(140, 125, 150, 30, TFT_BLACK);
  tft.drawString(String(voltage,1) + " V", 140, 130, 2);

  tft.fillRect(140, 165, 150, 30, TFT_BLACK);
  tft.drawString(String(current,1) + " A", 140, 170, 2);
}

void drawWifiConnectScreen(){
  tft.fillScreen(TFT_BLACK);

  tft.fillSmoothRoundRect(10, 0, 150, 30, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Back to Main Menu", 20, 8, 2);

  tft.drawString(getCurrentWifiNetwork(), 180, 8, 2);
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("Drawing ip?");
    tft.drawString("IP: "+WiFi.localIP(), 180, 28, 2);
  }
  
  tft.drawString("Available Networks", 100, 48, 2);

  tft.drawString("Loading...", 130, 78, 2);

  scanWiFiNetworks();

  tft.fillRect(100, 70, 200, 30, TFT_BLACK);

  for (int i = 0; i < min(netCount, 5); i++){
    tft.fillSmoothRoundRect(10, 72 + (30*i), 150, 28, 5, TFT_RED);
    tft.drawString(availableNetworks[i].ssid, 20, 78 + (30*i), 2);
  }
  if (netCount > 5){
    for (int i = 5; i < min(netCount, 10); i++){
      tft.fillSmoothRoundRect(170, 72 + (30*i), 150, 28, 5, TFT_RED);
      tft.drawString(availableNetworks[i].ssid, 180, 80 + (30*i), 2);
    }
  }

}
