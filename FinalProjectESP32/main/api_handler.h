#ifndef API_HANDLER_H
#define API_HANDLER_H

#include "passwords.h"
#include <Arduino.h>
#include "session_tracker.h"

#define GEMINI_MODEL           "gemini-2.5-flash"
#define GEMINI_TASK_STACK_SIZE  8192
#define GEMINI_TASK_PRIORITY    2

void   connectWiFiGemini(void);
String postGemini(const String &prompt);
void postGeminiSessionReport(const SessionSummary_t &summary);
void vGeminiTask(void *pvParameters);
void vWiFiKeepAliveTask(void *pvParameters);
bool WiFi_EnsureConnected(void);

#endif /* API_HANDLER_H */