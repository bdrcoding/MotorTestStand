// ----- Menus -----
void drawMainScreen();
void drawWifiConnectScreen();
void updateMainMenuValues();

// ----- Popups -----
void testTimePopup();
void showPopup(String message);
void clearPopup();

// ----- WiFi -----
void createWiFiConnection(int netNumber);

// ----- Globals -----
extern bool mainMenu;
extern bool popupVisible;
extern bool testTimePopupUp;
extern int testTime;
extern int netCount;

void Buttons(int x, int y){
  //MAIN MENU BUTTONS-------------------------------------------------------------------
  if (mainMenu){
    if (!testTimePopupUp){
      if (x > 10 && x < 160){
        if (y > 200 && y < 230){
          Serial.println("Tare Load Cell");
          sendCommand("TARE");
        }
      }
      tft.fillSmoothRoundRect(140, 200, 170, 30, 5, TFT_RED);
      if (x > 140 && x < 310){
        if (y > 200 && y < 230){
          Serial.println("Test Time Popup");
          testTimePopupUp = true;
          testTimePopup();
        }
      }
      if (x > 170 && x < 310){
        if (y > 0 && y < 30){
          Serial.println("Wifi Menu");
          mainMenu = false;
          drawWifiConnectScreen();
        }
      }
    }
    else{
      if (x > 140 && x < 310){
        if (y > 50 && y < 80){
          testTimePopupUp = false;
          Serial.println("Test Time Popup Down");
          testTime = 5;
          sendCommand("TESTTIME:5");
          drawMainScreen();
          Serial.println("Test Time set to 5 seconds");
        }
        if (y > 80 && y < 110){
          testTimePopupUp = false;
          Serial.println("Test Time Popup Down");
          testTime = 10;
          sendCommand("TESTTIME:10");
          drawMainScreen();
          Serial.println("Test Time set to 10 seconds");
        }
        if (y > 110 && y < 140){
          testTimePopupUp = false;
          Serial.println("Test Time Popup Down");
          testTime = 20;
          sendCommand("TESTTIME:20");
          drawMainScreen();
          Serial.println("Test Time set to 20 seconds");
        }
        if (y > 140 && y < 170){
          testTimePopupUp = false;
          Serial.println("Test Time Popup Down");
          testTime = 30;
          sendCommand("TESTTIME:30");
          drawMainScreen();
          Serial.println("Test Time set to 30 seconds");
        }
        if (y > 170 && y < 200){
          testTimePopupUp = false;
          Serial.println("Test Time Popup Down");
          testTime = 60;
          sendCommand("TESTTIME:60");
          drawMainScreen();
          Serial.println("Test Time set to 60 seconds");
        }
        if (y > 200 && y < 230){
          testTimePopupUp = false;
          Serial.println("Test Time Popup Down");
          drawMainScreen();
        }
      }
      else {
        testTimePopupUp = false;
        Serial.println("Test Time Popup Down");
        drawMainScreen();
      }
    }
  }
  //WIFI SCREEN BUTTONS---------------------------------------------------------------------
  else{
    if (x > 10 && x < 150){
      if (y > 0 && y < 30){
        Serial.println("Back to Main Menu");
        mainMenu = true;
        drawMainScreen();
      }
    }

    for (int i = 0; i < min(netCount, 5); i++){
    //   tft.fillSmoothRoundRect(10, 72 + (30*i), 150, 28, 5, TFT_RED);
    // tft.drawString(availableNetworks[i].ssid, 20, 78 + (30*i), 2);
      if (x > 10 && x < 150){
        if (y > 72 + (30*i) && y < 102 + (30*i)){
          createWiFiConnection(i);
        }
      }
    }
    if (netCount > 5){
      for (int i = 5; i < min(netCount, 10); i++){
        // tft.fillSmoothRoundRect(170, 62 + (30*i), 150, 28, 5, TFT_RED);
        // tft.drawString(availableNetworks[i].ssid, 180, 70 + (30*i), 2);
      }
    }
  }
  // POPUP BUTTONS----------------------------------------------------------------tft.fillRect(275, 15, 25, 25, TFT_DARKGREY);
  if (popupVisible){
    if (x > 275 && x < 300){
      if (y > 15 && y < 40){
        clearPopup();
      }
    }
  }
}