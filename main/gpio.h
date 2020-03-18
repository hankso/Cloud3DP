/* 
 * File: gpio.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-18 23:29:44
 */

#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

void gpio_initialize();

void LIGHTON();
void LIGHTOFF();
void LIGHTBLK(uint32_t ms = 20, uint8_t n = 1);

#endif // _GPIO_H_
