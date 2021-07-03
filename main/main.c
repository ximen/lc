/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "board.h"

#define TAG "MAIN"
#define GPIO_QUEUE_LENGTH       10
#define GPIO_POLL_INTERVAL_MS   500  

#define SIZEOF(arr)     sizeof(arr)/sizeof(arr[0])

bool current_alarm = false;

void notify_alarm(){
    ESP_LOGW(TAG, "Notifying alarm");
    bool buzzer;
    bool relay;
    bool mqtt;
    bool tg;
    bool can;
    app_config_getBool("buzzer_notify", &buzzer);
    app_config_getBool("relay_notify", &relay);
    app_config_getBool("mqtt_notify", &mqtt);
    app_config_getBool("can_notify", &can);
    app_config_getBool("tg_notify", &tg);
    if(buzzer) buzzer_set(1);
    if(relay) relay_set(1);
    // TODO: MQTT alarm notification
    // TODO: HAB alarm notification
    // TODO: Telegram alarm notification
}

void notify_stop(){
    ESP_LOGI(TAG, "Clearing alarm notification");
    bool buzzer;
    bool relay;
    bool mqtt;
    bool tg;
    bool can;
    app_config_getBool("buzzer_notify", &buzzer);
    app_config_getBool("relay_notify", &relay);
    app_config_getBool("mqtt_notify", &mqtt);
    app_config_getBool("can_notify", &can);
    app_config_getBool("tg_notify", &tg);
    if(buzzer) buzzer_set(0);
    if(relay) relay_set(0);
    // TODO: MQTT alarm notification
    // TODO: HAB alarm notification
    // TODO: Telegram alarm notification
}

static void gpio_task(void* arg){
    while(true){
        uint8_t alarm = 0;
        for (uint8_t i = 0; i < SENSORS_NUM; i++){
            uint8_t state = get_sensor_state(i);
            alarm += !state;
            if(!state && !current_alarm) ESP_LOGE(TAG, "Alarm on sensor %d!", i+1);
        }
        if(alarm && !current_alarm){
            ESP_LOGW(TAG, "Starting alarm");
            current_alarm = true;
            valves_off();
            notify_alarm();
        } else {
            if(get_reset() && current_alarm){
                ESP_LOGI(TAG, "Stopping alarm");
                current_alarm = false;
                valves_on();
                notify_stop();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(GPIO_POLL_INTERVAL_MS));
    }
}

void app_main(void)
{
    app_config_cbs_t app_cbs;						// Structure containing pointers to callback functions
    ESP_ERROR_CHECK(app_config_init(&app_cbs));		// Initializing and loading configuration
    board_init();
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    ESP_LOGI(TAG, "Started");
    //vTaskStartScheduler();
    vTaskDelay(portMAX_DELAY);
}
