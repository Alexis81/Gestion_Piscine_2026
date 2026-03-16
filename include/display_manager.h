#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "config.h"
#include <M5Unified.h>
#include <NTPClient.h>
#include <WiFi.h>

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void displayInfo(float tempEau, bool pompeEtat, bool electrolyseEtat,
                   IPAddress ip);
  void manageBrightness(bool elegantOtaIsActive);
  void updateActivity();

private:
  unsigned long lastLcdActivity;
  bool lcdDimmed;
  bool firstUpdate;
};

#endif // DISPLAY_MANAGER_H
