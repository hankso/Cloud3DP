/* 
 * File: console.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-12 15:57:35
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

/*
 * Config and init console. console_register_commands is called at the end.
 */
void console_initialize();

/*
 * (E) parse and Execute the command.
 * Save result in ret and append the command to linenoise's history (default).
 */
char * console_handle_command(char* cmd, bool history = true);
void console_handle_command(char *cmd, char *ret, bool history = true);

/*
 * (R) Read from console stream (wait until command input).
 * (E) parse and Execute the command (call console_handle_command).
 * (P) then Print the result.
 */
void console_handle_one();

/*
 * (L) endless Loop of handle_console.
 */
void console_handle_loop(void*);

/*
 * Create a xTask of console_handle_loop.
 * Save TaskHandle if parameter pxCreatedTask specified.
 */
void console_loop_begin(TaskHandle_t *pxCreatedTask = NULL, int xCoreID = 0);

/*
 * Stop loopTask created by console_loop_begin (i.e. terminate the xTask).
 */
void console_loop_end();

#endif // _CONSOLE_H
