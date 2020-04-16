/* 
 * File: globals.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2019-05-27 15:51:08
 *
 * Global variables are declared as extern in this header file.
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// make it compatiable with both makeEspArduino & Kconfig.build
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_DEBUG
#define CONFIG_BLINK_GPIO 2
#define CONFIG_CONSOLE_UART_NUM 0
#endif

// Utilities

char * cast_away_const(const char*);
char * cast_away_const_force(const char*);

const char * format_sha256(uint8_t *, size_t len = 64);
const char * format_mac(uint8_t *, size_t len = 17);

void task_info();
void memory_info();
void version_info();
void hardware_info();
void partition_info();

#endif // _GLOBALS_H_
