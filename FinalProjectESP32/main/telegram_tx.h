#pragma once

#define TELEGRAM_TASK_STACK_SIZE 8192
#define TELEGRAM_TASK_PRIORITY   2

void Telegram_Init(void);
void testTelegram(void);
void Telegram_SendMessage(const String &msg);
void Telegram_StartPeriodicHeartbeat(uint32_t period_ms);
void vTelegramTask(void *pvParameters);
