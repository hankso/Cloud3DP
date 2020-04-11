/*************************************************************************
File: max6675.h
Author: Hankso
Webpage: http://github.com/hankso
Time: Mon 27 May 2019 15:47:10 CST
************************************************************************/

#ifndef TEMP_MAX6675_H
#define TEMP_MAX6675_H

#include <stdio.h>
#include <stdint.h>

/*
 * HSPI Pin Number
 *  MISO - GPIO12
 *  MOSI - GPIO13 (not used)
 *  SCLK - GPIO14
 *  QDWP - GPIO2 (not used)
 *  QDHD - GPIO4 (not used)
 * >CS1  - GPIO15
 * >CS2  - GPIO25
 * >CS3  - GPIO32
 */
extern const uint8_t HSPI_IOMUX_PIN_NUM_CSn[3];

/*
 * VSPI Pin Number
 *  MISO - GPIO19
 *  MOSI - GPIO23 (not used)
 *  SCLK - GPIO18
 *  QDWP - GPIO22 (not used)
 *  QDHD - GPIO21 (not used)
 * >CS4  - GPIO5
 * >CS5  - GPIO17
 * >CS6  - GPIO23
 */
extern const uint8_t VSPI_IOMUX_PIN_NUM_CSn[3];

/*
 * Initialize SPI driver and config bus for MAX6675
 */
void max6675_init();

/*
 * Data received from MAX6675
 *
 * +-----------+-------------+----------+-----------+--------+
 * | Dummy Bit | 12bits TEMP | T-Input  | Device ID | State  |
 * +-----------+-------------+----------+-----------+--------+
 * | D15 = 0   | D14 - D3    | D2 = 1/0 | D1 = 0    | D0 = ? |
 * +-----------+-------------+----------+-----------+--------+
 *
 */
void max6675_read();

size_t max6675_json(char*);

#endif // TEMP_MAX6675_H
