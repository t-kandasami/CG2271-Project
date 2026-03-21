#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include <Arduino.h>

#define DHT_PIN              20
#define DHT_TASK_PRIORITY    1
#define DHT_TASK_STACK_SIZE  2048
#define DHT_POLL_MS          2000

#define TEMP_GOOD            26.0f
#define TEMP_WARN            27.5f
#define TEMP_ALERT           29.0f
#define HUM_GOOD             65.0f
#define HUM_WARN             70.0f
#define HUM_ALERT            75.0f

void  DHT_Init(void);
void  vDHTTask(void *pvParameters);
float DHT_GetTemp(void);
float DHT_GetHumidity(void);

#endif // DHT_SENSOR_H