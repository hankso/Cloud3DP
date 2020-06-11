/* 
 * File: update.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-23 11:42:30
 */

#ifndef _UPDATE_H_
#define _UPDATE_H_

#include "globals.h"

void ota_initialize();

void ota_updation_reset();
bool ota_updation_begin(size_t size = 0);
bool ota_updation_write(uint8_t *, size_t);
bool ota_updation_end();
bool ota_updation_url(const char *url = NULL);
bool ota_updation_partition(const char *);

const char * ota_updation_error();

void ota_partition_info();
bool check_valid();

#endif // _UPDATE_H_
