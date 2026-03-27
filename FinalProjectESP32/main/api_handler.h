#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <Arduino.h>

#define WIFI_SSID     ""
#define WIFI_PASSWORD ""
#define GEMINI_KEY    "AIzaSyA1Amrj1grbtLubNDz7ebWcGj6mKtzWuGM"
#define GEMINI_MODEL  "gemini-2.0-flash"

void connectWiFi(void);
String postGemini(const String &prompt);

#endif /* API_HANDLER_H */