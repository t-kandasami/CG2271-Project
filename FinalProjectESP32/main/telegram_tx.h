#pragma once
void Telegram_Init();
//void vTelegramTask(void *pvParameters);
#define TELEGRAM_TASK_STACK_SIZE 8192
#define TELEGRAM_TASK_PRIORITY   2

void Telegram_SendMessage(const String &msg);