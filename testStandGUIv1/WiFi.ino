// ----- Popups -----
void showPopup(String message);
String onScreenKeyboard(int boxX, int boxY, int boxW, int boxH);

// ----- Menus -----
void drawMainScreen();

// ----- Globals -----
extern bool mainMenu;

extern WiFiNet availableNetworks[];
extern int netCount;

bool connectToWiFiNetwork(String ssid, String password){
  WiFi.begin(ssid, password);
  Serial.print("Connecting to "+ssid);

  int timeout = 20; // ~10 seconds
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nFailed to connect");
    return false;
  }

  delay(200);
}

String getCurrentWifiNetwork(){
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  } else {
    return "Not Connected";
  }
}

int findNet(WiFiNet nets[], int count, const String &ssid) {
  for (int i = 0; i < count; i++) {
    if (nets[i].ssid == ssid) return i;
  }
  return -1;
}

void sortNets(WiFiNet nets[], int count) {
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (nets[j].rssi > nets[i].rssi) {
        WiFiNet tmp = nets[i];
        nets[i] = nets[j];
        nets[j] = tmp;
      }
    }
  }
}

void scanWiFiNetworks() {

  int n = WiFi.scanNetworks();
  netCount = 0;

  for (int i = 0; i < n; i++) {

    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    if (ssid.length() == 0) continue;

    int idx = -1;

    for (int j = 0; j < netCount; j++) {
      if (availableNetworks[j].ssid == ssid) {
        idx = j;
        break;
      }
    }

    if (idx == -1) {
      if (netCount < MAX_NETS) {
        availableNetworks[netCount].ssid = ssid;
        availableNetworks[netCount].rssi = rssi;
        netCount++;
      }
    } else {
      if (rssi > availableNetworks[idx].rssi) {
        availableNetworks[idx].rssi = rssi;
      }
    }
  }

  WiFi.scanDelete();

  // sort strongest first
  for (int i = 0; i < netCount - 1; i++) {
    for (int j = i + 1; j < netCount; j++) {
      if (availableNetworks[j].rssi > availableNetworks[i].rssi) {
        WiFiNet tmp = availableNetworks[i];
        availableNetworks[i] = availableNetworks[j];
        availableNetworks[j] = tmp;
      }
    }
  }
}

void createWiFiConnection(int netNumber){
  String ssid = availableNetworks[netNumber].ssid;
  showPopup("Enter Password for "+ssid);

  String password = onScreenKeyboard(50, 60, 200, 20);

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);

  tft.drawString("Connecting...", 80, 80, 2);

  bool result = connectToWiFiNetwork(ssid, password);

  if (result){
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Success! Returning to main menu...", 40, 80, 2);
    mainMenu = true;
    
  }
  else{
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connection failed. Returning to main menu...", 40, 80, 2);
    mainMenu = true;
  }
  drawMainScreen();
  delay(3000);
}

