/* 
 * File: gpio.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-18 23:32:46
 */

#include "gpio.h"
#include "globals.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

gpio_num_t PIN_BLINK = static_cast<gpio_num_t>(CONFIG_BLINK_GPIO);

void gpio_initialize() {
    gpio_config_t out_conf = {
        .pin_bit_mask = BIT(PIN_BLINK),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);
}

void LIGHTON() { gpio_set_level(PIN_BLINK, 1); }

void LIGHTOFF() { gpio_set_level(PIN_BLINK, 0); }

void LIGHTBLK(uint32_t ms, uint8_t n) {
    while (n--) {
        LIGHTON();  vTaskDelay(ms / portTICK_PERIOD_MS);
        LIGHTOFF(); vTaskDelay(ms / portTICK_PERIOD_MS);
    }
}
