#ifndef API_HANDLER_H
#define API_HANDLER_H
#include "passwords.h"
#include <Arduino.h>


#define GEMINI_MODEL  "gemini-2.5-flash"

void connectWiFiGemini(void);
String postGemini(const String &prompt);

#endif /* API_HANDLER_H */
