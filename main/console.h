/* 
 * File: console.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 15:57:35
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

// Config and init console. commands are registered at the end.
void console_initialize();

// After calling console_handle_command, remember to free the returned buffer.
char * console_handle_command(const char *cmd, bool hist = true);

/* (R) Read from console stream (wait until command input).
 * (E) parse and Execute the command (call console_handle_command).
 * (P) then Print the result.
 */
void console_handle_one();

// (L) endless Loop of console_handle_one.
void console_handle_loop(void*);

// Create a FreeRTOS Task on function console_handle_loop.
void console_loop_begin(int xCoreID = 1);

// Light weight JSON RPC dispatcher: parse json -> execute -> pack result
char * console_handle_rpc(const char *json);

// Implemented in console_cmds.cpp
void console_register_commands();

#endif // _CONSOLE_H
