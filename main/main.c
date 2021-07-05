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
#include "esp_http_client.h"
#include "app_config_mqtt_switch.h"
#include "app_config_wifi.h"

#define TAG "MAIN"
#define GPIO_QUEUE_LENGTH       10
#define GPIO_POLL_INTERVAL_MS   500  

#define SIZEOF(arr)     sizeof(arr)/sizeof(arr[0])

bool current_alarm = false;
bool mqtt_enable;
app_config_mqtt_switch_t *sw1;
app_config_mqtt_switch_t *sw2;
char *avail_topic;

esp_err_t _http_event_handle(esp_http_client_event_t *evt){
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGW(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void notify_tg(){
    char tg_url[255] = {0};
    char *tg_token;
    char *tg_chat;
    char *tg_text = "Attention%21%20Leak%20detected%21%20Water%20supply%20stopped%20until%20manual%20restart%21";
    app_config_getString("api_key", (char **)&tg_token);
    app_config_getString("chat_id", (char **)&tg_chat);
    if(!strlen(tg_token) || !strlen(tg_chat)) {
        ESP_LOGW(TAG, "Telegram data missed");
        return;
    }
    snprintf(tg_url, sizeof(tg_url), "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s", tg_token, tg_chat, tg_text);
    ESP_LOGI(TAG, "TG URL: %s", tg_url);
    esp_http_client_config_t config = {
        .url = tg_url,
        .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
    ESP_LOGI(TAG, "Status = %d, content_length = %d",
           esp_http_client_get_status_code(client),
           esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);
}

void notify_alarm(){
    ESP_LOGW(TAG, "Notifying alarm");
    bool buzzer;
    bool relay;
    bool tg;
    app_config_getBool("buzzer_notify", &buzzer);
    app_config_getBool("relay_notify", &relay);
    app_config_getBool("tg_notify", &tg);
    if(buzzer) buzzer_set(1);
    if(relay) relay_set(1);
    // TODO: MQTT alarm notification
    // TODO: HAB alarm notification
    if(tg) notify_tg();
}

void notify_stop(){
    ESP_LOGI(TAG, "Clearing alarm notification");
    bool buzzer;
    bool relay;
    bool tg;
    app_config_getBool("buzzer_notify", &buzzer);
    app_config_getBool("relay_notify", &relay);
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
            if(mqtt_enable){
                app_config_mqtt_switch_set(0, sw1);
                app_config_mqtt_switch_set(0, sw2);
            }
            notify_alarm();
        } else {
            if(get_reset() && current_alarm){
                ESP_LOGI(TAG, "Stopping alarm");
                current_alarm = false;
                valves_on();
                if(mqtt_enable){
                    app_config_mqtt_switch_set(1, sw1);
                    app_config_mqtt_switch_set(1, sw2);
                }
                notify_stop();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(GPIO_POLL_INTERVAL_MS));
    }
}

void switch_handler(uint8_t state, app_config_mqtt_switch_t *sw){
    lc_valve_t *valve = (lc_valve_t *)sw->user_data;
    valve_set(valve, state);
}

void ip_cb(ip_event_t event, void *event_data){
    char *std_mqtt_prefix;
    app_config_getString("std_mqtt_prefix", &std_mqtt_prefix);
    char *std_mqtt_objid;
    app_config_getString("std_mqtt_objid", &std_mqtt_objid);
    int avail_topic_len = strlen(std_mqtt_prefix) + strlen(std_mqtt_objid) + strlen(CONFIG_APP_CONFIG_MQTT_SWITCH_AVAIL_STR) + 3;
    avail_topic = (char *)malloc(avail_topic_len);
    snprintf(avail_topic, avail_topic_len, "%s/%s%s", std_mqtt_prefix, std_mqtt_objid, CONFIG_APP_CONFIG_MQTT_SWITCH_AVAIL_STR);
    app_config_mqtt_lwt_t lwt = {.topic = avail_topic, .msg = "offline"};
    app_config_mqtt_init(&lwt);
}

void mqtt_cb(esp_mqtt_event_handle_t event){
    if(event->event_id == MQTT_EVENT_CONNECTED){
        app_config_mqtt_publish(avail_topic, "online", false);

        char *valves_prefix;
        app_config_getString("valves_prefix", &valves_prefix);
        char *sensor_prefix;
        app_config_getString("sensors_prefix", &sensor_prefix);
        char *valve1_n;
        app_config_getString("valve1_name", &valve1_n);
        char *valve2_n;
        app_config_getString("valve2_name", &valve2_n);
        ESP_LOGI(TAG, "%s",valve1_n);
        sw1 = app_config_mqtt_switch_create(valves_prefix, "drive1", valve1_n, switch_handler, true, get_valve(0));
        app_config_mqtt_switch_set(1, sw1);
        sw2 = app_config_mqtt_switch_create(valves_prefix, "drive2", valve2_n, switch_handler, true, get_valve(1));
        app_config_mqtt_switch_set(1, sw2);
    } else if(event->event_id == MQTT_EVENT_DISCONNECTED){
        free(avail_topic);
    }
}

void app_main(void)
{
    app_config_cbs_t app_cbs;
    ESP_ERROR_CHECK(app_config_init(&app_cbs));		// Initializing and loading configuration
    board_init();
    app_config_getBool("mqtt_enable", &mqtt_enable);
    valves_on();
    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "Started");
    if(mqtt_enable){
        app_config_ip_register_cb(IP_EVENT_STA_GOT_IP, ip_cb);
        app_config_mqtt_register_cb(MQTT_EVENT_CONNECTED, mqtt_cb);
        app_config_mqtt_register_cb(MQTT_EVENT_DISCONNECTED, mqtt_cb);
    }
    vTaskDelay(portMAX_DELAY);
}
