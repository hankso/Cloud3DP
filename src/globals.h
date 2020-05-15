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
#include <stdbool.h>

// make it compatiable with both makeEspArduino & Kconfig.build
#ifndef CONFIG_GPIO_RGB_LED
#define CONFIG_GPIO_RGB_LED 32
#define CONFIG_GPIO_ES_INT 35
#define CONFIG_GPIO_I2C_SDA 25
#define CONFIG_GPIO_I2C_SCL 26

#define CONFIG_GPIO_HSPI_MISO 12
#define CONFIG_GPIO_HSPI_MOSI 13
#define CONFIG_GPIO_HSPI_QDWP 2
#define CONFIG_GPIO_HSPI_QDHD 4
#define CONFIG_GPIO_HSPI_SCLK 14
#define CONFIG_GPIO_HSPI_CS0 15
#define CONFIG_GPIO_HSPI_CS1 22

#define CONFIG_RMT_CHANNEL 0
#define CONFIG_LED_NUM  20
#define CONFIG_I2C_NUM  0
#define CONFIG_UART_NUM 0
#define CONFIG_DEBUG
#endif

// make it compatiable with Arduino
#ifndef bitRead
#define bitRead(v, b)       ((v) & BIT(b))
#define bitSet(v, b)        ((v) |= BIT(b))
#define bitClear(v, b)      ((v) &= ~BIT(b))
#define bitWrite(v, b, l)   (l ? bitSet(v, b) : bitClear(v, b))
#endif

#include "SDSPIFS.h"
#define SDFS_MP "/sdcard"
#define SDFS SDSPIFS

#include <SPIFFS.h>
#define FFS_MP "/flashfs"
#define FFS SPIFFS

// Utilities
char * cast_away_const(const char*);
char * cast_away_const_force(const char*);

const char * format_sha256(const uint8_t*, size_t);
const char * format_mac(const uint8_t*, size_t);

void task_info();
void memory_info();
void version_info();
void hardware_info();
void partition_info();

#endif // _GLOBALS_H_
