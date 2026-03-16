#include "display_manager.h"

DisplayManager::DisplayManager()
    : lastLcdActivity(0), lcdDimmed(false), firstUpdate(true) {}

void DisplayManager::begin() {
  M5.Display.setRotation(1);
  M5.Display.setBrightness(BRIGHTNESS_MAX);
  M5.Display.fillScreen(BLACK);
  lastLcdActivity = millis();
  lcdDimmed = false;

  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.println("Initialisation...");
}

void DisplayManager::displayInfo(float tempEau, bool pompeEtat,
                                 bool electrolyseEtat, IPAddress ip) {
  if (firstUpdate) {
    M5.Display.fillScreen(BLACK);
    firstUpdate = false;
  }

  M5.Display.startWrite();
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(10, 20);
  M5.Display.print("Piscine");

  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 60);
  M5.Display.setTextColor(BLUE, BLACK);
  M5.Display.printf("Eau   : %5.1f C ", tempEau);

  M5.Display.setCursor(10, 95);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.printf("Pompe : %s   ", pompeEtat ? "ON" : "OFF");

  M5.Display.setCursor(10, 130);
  M5.Display.printf("Electrolyse: %s   ", electrolyseEtat ? "ON" : "OFF");

  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 228);
  M5.Display.printf("IP: %-15s", ip.toString().c_str());

  M5.Display.endWrite();
}

void DisplayManager::manageBrightness(bool elegantOtaIsActive) {
  if (elegantOtaIsActive)
    return;

  unsigned long now = millis();
  if (!lcdDimmed && (now - lastLcdActivity > TIME_FADE)) {
    M5.Display.setBrightness(BRIGHTNESS_MINI);
    lcdDimmed = true;
    Serial.println("LCD Dimmed");
  } else if (lcdDimmed && (now - lastLcdActivity <= TIME_FADE)) {
    M5.Display.setBrightness(BRIGHTNESS_MAX);
    lcdDimmed = false;
    Serial.println("LCD Bright");
  }
}

void DisplayManager::updateActivity() {
  lastLcdActivity = millis();
  if (lcdDimmed) {
    M5.Display.setBrightness(BRIGHTNESS_MAX);
    lcdDimmed = false;
    Serial.println("LCD Bright");
  }
}
