/* 
 * File: update.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-23 11:42:30
 */

#ifndef _UPDATE_H_
#define _UPDATE_H_

#include <stdint.h>
#include <stdlib.h>

void ota_initialize();

bool ota_updation_begin();
bool ota_updation_write(uint8_t *data, size_t len);
bool ota_updation_end();
const char * ota_updation_error();

void ota_info();

#endif // _UPDATE_H_
