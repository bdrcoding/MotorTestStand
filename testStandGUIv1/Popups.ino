// ----- Menus -----
void drawMainScreen();
void drawWifiConnectScreen();

// ----- Globals -----
extern bool popupVisible;
extern bool mainMenu;
extern String popupMessage;

extern int testTime;

// ----- Touch -----
int getTouchX();
int getTouchY();

extern XPT2046_Touchscreen touch;
extern TFT_eSPI tft;

void testTimePopup(){
  tft.fillSmoothRoundRect(140, 50, 170, 150, 5, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.drawLine(140, 50, 310, 50, TFT_BLACK);
  tft.drawString("5 secs", 150, 58, 2);
  tft.drawLine(140, 80, 310, 80, TFT_BLACK);
  tft.drawString("10 secs", 150, 88, 2);
  tft.drawLine(140, 110, 310, 110, TFT_BLACK);
  tft.drawString("20 secs", 150, 118, 2);
  tft.drawLine(140, 140, 310, 140, TFT_BLACK);
  tft.drawString("30 secs", 150, 148, 2);
  tft.drawLine(140, 170, 310, 170, TFT_BLACK);
  tft.drawString("60 secs", 150, 178, 2);
  tft.drawLine(140, 200, 310, 200, TFT_BLACK);
  tft.drawString("Test Time: "+String(testTime)+" secs", 150, 208, 2);
  tft.fillTriangle(
    290,  222,
    280,  207,
    300,  207,
    TFT_GREEN
  );
}

void showPopup(String message)
{
    popupVisible = true;
    popupMessage = message;

    // Background
    tft.fillRoundRect(10, 10, 300, 60, 8, TFT_BLUE);

    // Border
    tft.drawRoundRect(10, 10, 300, 60, 8, TFT_WHITE);

    // Message
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1.5);
    tft.drawString(message, 20, 30);

    // Close button
    tft.fillRect(275, 15, 25, 25, TFT_DARKGREY);
    tft.drawRect(275, 15, 25, 25, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.drawString("X", 282, 20);
}

void clearPopup()
{
    popupVisible = false;

    // Redraw top area
    tft.fillRect(0, 0, 320, 80, TFT_BLACK);

    // Re-draw your normal UI
    if (mainMenu){
      drawMainScreen();
    }
    else{
      drawWifiConnectScreen();
    }
}

String onScreenKeyboard(int boxX, int boxY, int boxW, int boxH) {

  String text = "";
  bool shift = false;

  struct Key {
    const char* label;
    int x, y, w, h;
  };

  // 4 rows keyboard layout
  const int keyH = 25;
  const int keyW = 24;

  Key keys[] = {
    // Row 1
    {"Q",  0, 120, keyW, keyH}, {"W",  25, 120, keyW, keyH}, {"E",  50, 120, keyW, keyH},
    {"R",  75, 120, keyW, keyH}, {"T", 100, 120, keyW, keyH}, {"Y", 125, 120, keyW, keyH},
    {"U", 150, 120, keyW, keyH}, {"I", 175, 120, keyW, keyH}, {"O", 200, 120, keyW, keyH},
    {"P", 225, 120, keyW, keyH},

    // Row 2
    {"A",  10, 145, keyW, keyH}, {"S",  35, 145, keyW, keyH}, {"D",  60, 145, keyW, keyH},
    {"F",  85, 145, keyW, keyH}, {"G", 110, 145, keyW, keyH}, {"H", 135, 145, keyW, keyH},
    {"J", 160, 145, keyW, keyH}, {"K", 185, 145, keyW, keyH}, {"L", 210, 145, keyW, keyH},

    // Row 3
    {"SHIFT", 0, 170, 50, keyH},
    {"Z", 55, 170, keyW, keyH}, {"X", 80, 170, keyW, keyH}, {"C", 105, 170, keyW, keyH},
    {"V", 130, 170, keyW, keyH}, {"B", 155, 170, keyW, keyH}, {"N", 180, 170, keyW, keyH},
    {"M", 205, 170, keyW, keyH},
    {"DEL", 230, 170, 40, keyH},

    // Row 4 (numbers + space + enter)
    {"1",  0, 195, keyW, keyH}, {"2", 25, 195, keyW, keyH}, {"3", 50, 195, keyW, keyH},
    {"4",  75, 195, keyW, keyH}, {"5", 100, 195, keyW, keyH}, {"6", 125, 195, keyW, keyH},
    {"7", 150, 195, keyW, keyH}, {"8", 175, 195, keyW, keyH}, {"9", 200, 195, keyW, keyH},
    {"0", 225, 195, keyW, keyH},

    {"SPACE", 0, 220, 180, 20},
    {"ENTER", 180, 220, 100, 20}
  };

  int keyCount = sizeof(keys) / sizeof(keys[0]);

  tft.fillScreen(TFT_BLACK);

  tft.drawString("Enter Password:", 50, 20, 2);

  while (true) {

    // ---- draw text box ----
    tft.fillRect(boxX, boxY, boxW, boxH, TFT_DARKGREY);
    tft.drawRect(boxX, boxY, boxW, boxH, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setCursor(boxX + 5, boxY + 5);
    tft.print(text);

    // ---- draw keyboard (only once-ish) ----
    for (int i = 0; i < keyCount; i++) {
      tft.drawRect(keys[i].x, keys[i].y, keys[i].w, keys[i].h, TFT_WHITE);
      tft.setCursor(keys[i].x + 2, keys[i].y + 8);
      tft.print(keys[i].label);
    }

    // ---- touch handling ----
    if (touch.touched()) {

      int x = getTouchX();
      int y = getTouchY();

      for (int i = 0; i < keyCount; i++) {

        if (x > keys[i].x && x < keys[i].x + keys[i].w &&
            y > keys[i].y && y < keys[i].y + keys[i].h) {

          String k = keys[i].label;

          // SHIFT
          if (k == "SHIFT") {
            shift = !shift;
          }

          // BACKSPACE
          else if (k == "DEL") {
            if (text.length() > 0)
              text.remove(text.length() - 1);
          }

          // SPACE
          else if (k == "SPACE") {
            text += " ";
          }

          // ENTER
          else if (k == "ENTER") {
            return text;
          }

          // normal key
          else {
              if (shift) {
                  k.toUpperCase();
              } else {
                  k.toLowerCase();
              }

              text += k;
          }

          delay(200); // debounce
        }
      }
    }

    delay(20);
  }
}