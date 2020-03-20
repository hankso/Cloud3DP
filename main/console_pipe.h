/* 
 * File: console_pipe.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-19 23:17:11
 *
 * See more comments in console_pipe.cpp
 */

#ifndef _CONSOLE_PIPE_H_
#define _CONSOLE_PIPE_H_

#define CONFIG_CONSOLE_PIPE_MSTREAM

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void console_pipe_init();

// Call them inside task, because this may swap _getreent()->_stdout
char * console_pipe_enter();
void console_pipe_exit();

#endif // _CONSOLE_PIPE_H_
