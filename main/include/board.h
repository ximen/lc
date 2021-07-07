#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
#include "freertos/timers.h"

#define GPIO_SENSORS_PIN_SEL ((1ULL<<GPIO_NUM_12) | \
                              (1ULL<<GPIO_NUM_13) | \
                              (1ULL<<GPIO_NUM_14) | \
                              (1ULL<<GPIO_NUM_15) | \
                              (1ULL<<GPIO_NUM_16) | \
                              (1ULL<<GPIO_NUM_17))
#define GPIO_OUT_PIN_SEL    ((1ULL<<GPIO_NUM_32) | \
                            (1ULL<<GPIO_NUM_33) | \
                            (1ULL<<GPIO_NUM_26) | \
                            (1ULL<<GPIO_NUM_27) | \
                            (1ULL<<GPIO_NUM_18) | \
                            (1ULL<<GPIO_NUM_19))
typedef struct {
    gpio_num_t  gpio;
    bool        state;
} lc_sensor_t;

typedef struct {
    gpio_num_t      valve;
    gpio_num_t      power;
    TimerHandle_t   timer;
} lc_valve_t;

#define SENSORS_NUM     6
#define VALVES_NUM      2
#define VALVE_TIMEOUT   18000
#define BUZZER          GPIO_NUM_27
#define RELAY           GPIO_NUM_26
#define RESET           GPIO_NUM_23

void board_init();
uint8_t get_sensor_state(uint8_t num);
void valves_off();
void valves_on();
void valve_set(lc_valve_t *valve, uint8_t state);
void buzzer_set(uint8_t state);
void relay_set(uint8_t state);
bool get_reset();
lc_valve_t *get_valve(uint8_t num);