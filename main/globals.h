/* 
 * File: globals.h
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
#define CONFIG_BLINK_GPIO 2
#define CONFIG_CONSOLE_UART_NUM 0
#endif

#endif // _GLOBALS_H_
