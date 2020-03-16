/* 
 * File: globals.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:51:08
 *
 * Global variables are declared as extern in this header file.
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

// make it compatiable with both makeEspArduino & Kconfig.build
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_DEBUG
#define CONFIG_BLINK_GPIO  2
#endif

#include <Arduino.h>

#define PIN_LED            CONFIG_BLINK_GPIO
#define LIGHTON(...)       digitalWrite(PIN_LED, HIGH)
#define LIGHTOFF(...)      digitalWrite(PIN_LED, LOW)

inline void LIGHTBLK(uint32_t ms = 20, uint8_t n = 1) {
    while (n--) {
        LIGHTON();  delay(ms);
        LIGHTOFF(); delay(ms);
    }
}

#endif // _GLOBALS_H_
