/* 
 * File: console.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 15:57:35
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

// Config and init console. commands are registered at the end.
void console_initialize();

/* (R) Read from console stream (wait until command input).
 * (E) parse and Execute the command (call console_handle_command).
 * (P) then Print the result.
 */
void console_handle_one();

// (L) endless Loop of console_handle_one.
void console_handle_loop(void*);

// Create a FreeRTOS Task on console_handle_loop.
void console_loop_begin(int xCoreID = 1);

// Stop loopTask created by console_loop_begin.
void console_loop_end();


// When calling console_handle_command, remember to free the buffer after usage
char * console_handle_command(char* cmd, bool hist = true);
// Save result in buf and append the command to linenoise's history (default).
void console_handle_command(char *, char *buf, size_t len, bool hist = true);

#endif // _CONSOLE_H
