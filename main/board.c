#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "board.h"
#include "esp_log.h"

#define TAG "BOARD"

static lc_sensor_t sensors[] = {{GPIO_NUM_12, false},
                        {GPIO_NUM_13, false}, 
                        {GPIO_NUM_14, false},  
                        {GPIO_NUM_15, false}, 
                        {GPIO_NUM_16, false}, 
                        {GPIO_NUM_17, false}};

static lc_valve_t valves[] = {{GPIO_NUM_18, GPIO_NUM_32, 0},
                            {GPIO_NUM_19, GPIO_NUM_33, 0}};

void valve_timer_off(TimerHandle_t xTimer){
    lc_valve_t *valve = pvTimerGetTimerID(xTimer);;
    ESP_LOGI(TAG, "Powering off valve");
    gpio_set_level(valve->power, 0);
}

void board_init(){
    ESP_LOGI(TAG, "Initializing board");
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = GPIO_SENSORS_PIN_SEL,
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf);
    gpio_config_t io_conf2 = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = GPIO_OUT_PIN_SEL,
        .pull_down_en = 1,
        .pull_up_en = 0
    };
    gpio_set_direction(RESET, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RESET, GPIO_PULLDOWN_ENABLE);
    gpio_config(&io_conf2);
    for (uint8_t i = 0; i < VALVES_NUM; i++){
        valves[i].timer = xTimerCreate("valve_timer", pdMS_TO_TICKS(VALVE_TIMEOUT), pdFALSE, &valves[i], valve_timer_off);
    }
    
}

uint8_t get_sensor_state(uint8_t num){
    sensors[num].state = gpio_get_level(sensors[num].gpio);
    return(sensors[num].state);
}

void valve_set(lc_valve_t *valve, uint8_t state){
    ESP_LOGI(TAG, "Setting state %d to valve", state);
    gpio_set_level(valve->power, 1);
    gpio_set_level(valve->valve, state);
    xTimerStart(valve->timer, 0);
}

void valves_off(){
    ESP_LOGI(TAG, "Turning off valves");
    for (uint8_t i = 0; i < VALVES_NUM; i++){
        valve_set(&valves[i], 0);
    }
}

void valves_on(){
    ESP_LOGI(TAG, "Turning on valves");
    for (uint8_t i = 0; i < VALVES_NUM; i++){
        valve_set(&valves[i], 1);
    }
}

lc_sensor_t *sensor_from_gpio(gpio_num_t gpio){
    for (uint8_t i = 0; i < SENSORS_NUM; i++){
        if(sensors[i].gpio == gpio) return &sensors[i];
    }
    return(NULL);
}

void buzzer_set(uint8_t state){
    ESP_LOGI(TAG, "Setting buzzer to %d", state);
    gpio_set_level(BUZZER, state);
}

void relay_set(uint8_t state){
    ESP_LOGI(TAG, "Setting relay to %d", state);
    gpio_set_level(RELAY, state);
}

bool get_reset(){
    return(gpio_get_level(RESET));
}

lc_valve_t *get_valve(uint8_t num){
    return &valves[num];
}