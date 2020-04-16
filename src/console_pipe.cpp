/* 
 * File: console_pipe.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-03-19 23:00:52
 *
 * Redirect STDOUT to a buffer, thus any thing printed to STDOUT will be
 * collected as a string. But don't forget to free the buffer after result 
 * is handled.
 *
 * 1. <stdio.h> setvbuf - store string in buffer (temporarily)
 *  pos: easy to set & unset, safe printing with buflen
 *  neg: cannot stop flushing to stdout
 *  e.g.:
 *      char *buf = (char *)calloc(1024, sizeof(char));
 *      setvbuf(stdout, buf, _IOFBF, 1024);
 *      printf("test string\n");
 *      setvbuf(stdout, NULL, _IONBF, 1024);
 *      printf("Get string from STDOUT %lu: `%s`", strlen(buf), buf);
 *
 * 2. <stdio.h> memstream - open memory as stream
 *  2.1 fmemopen(void *buf, size_t size, const char *mode)
 *  2.2 open_memstream(char * *ptr, size_t *sizeloc)
 *  pos: The open_memstream will dynamically allocate buffer and automatically
 *       grow. The fmemopen function support 'a'(append) mode.
 *  neg: Caller need to free the buffer after stream is closed. Stream opened
 *       by fmemopen function need buf config (i.e. setvbuf or fflush) and is
 *       limited by buffer size.
 *  e.g.:
 *      char *buf; size_t size;
 *      FILE *stdout_bak = stdout;
 *      stdout = open_memstream(&buf, &size);
 *      printf("hello\n");
 *      fclose(stdout); stdout = stdout_bak;
 *      printf("In buffer: size %d, msg %s\n", size, buf);
 *      free(buf);
 *
 * 3. <unistd.h> pipe & dup: this is not what we want
 *
 * 4. If pipe STDOUT to a disk file on flash or memory block using VFS, thing
 *    becomes much easier.
 *  e.g.:
 *      stdout = fopen("/spiffs/runtime.log", "w");
 *    But writing & reading from a file is slow and consume too much resources.
 *    A better way is memory mapping.
 *  e.g.:
 *      esp_vfs_t vfs_buffer = {
 *          .open = &my_malloc,
 *          .close = &my_free,
 *          .read = &my_strcpy,
 *          .write = &my_snprintf,
 *          .fstat = NULL,
 *          .flags = ESP_VFS_FLAG_DEFAULT,
 *      }
 *      esp_vfs_register("/dev/ram", &vfs_buffer, NULL);
 *      stdout = fopen("/dev/ram/blk0", "w");
 *
 * Currently implemented is method 2. Try method 4 if necessary in the future.
 */

#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    char *buf;  // dynamically sized buffer
    size_t idx; // current file offset
    size_t len; // size of buf
    FILE *bak;  // stdout backup
    FILE *obj;  // reference to memstream
} stdout_mstream;

static int _memstream_writefn(void *cookie, const char *data, int size) {
    size_t available = stdout_mstream.len - stdout_mstream.idx;
    if (available < size) {
        // progress with a step of 64
        size_t newsize = stdout_mstream.idx + size + 64;
        char *tmp = (char *)realloc(stdout_mstream.buf, newsize);
        if (tmp == NULL) return -1;
        stdout_mstream.buf = tmp;
        stdout_mstream.len = newsize;
        fprintf(stderr, "new size: %d\n", newsize);
    }
    memcpy(stdout_mstream.buf + stdout_mstream.idx, data, size);
    stdout_mstream.idx += size;
    fprintf(stderr, "%d/%d/%d: `%s`\n",
            stdout_mstream.idx, stdout_mstream.len, size, data);
    return size;
}

static int _memstream_closefn(void *cookie) {
    if (stdout_mstream.buf != NULL) {
        free(stdout_mstream.buf);
    }
    stdout_mstream = { NULL, 0, 0, NULL, NULL };
    return 0;
}

void console_pipe_init() {
#ifndef CONFIG_CONSOLE_PIPE_MSTREAM
    if (stdout_mstream.obj) return;
    stdout_mstream.obj = funopen(
        &stdout_mstream,                    // pointer to cookie
        (int (*)(void*, char*, int))0,      // read function
        &_memstream_writefn,                // write function
        (fpos_t (*)(void*, fpos_t, int))0,  // seek function
        &_memstream_closefn                 // close function
    );
    setvbuf(stdout_mstream.obj, NULL, _IONBF, 0);   // disable stream buffer
#endif
}

// See https://linux.die.net/man/3/fopencookie for reference
char * console_pipe_enter() {
    stdout_mstream.bak = stdout; return NULL;
#ifdef CONFIG_CONSOLE_PIPE_MSTREAM
    stdout = open_memstream(&stdout_mstream.buf, &stdout_mstream.idx);
    setvbuf(stdout, NULL, _IONBF, 0);
#else
    char *buf = (char *)malloc(64);
    if (buf == NULL) return NULL;
    stdout_mstream.buf = buf;
    stdout_mstream.idx = 0;
    stdout_mstream.len = 64;
    stdout = stdout_mstream.obj;
#endif
    return stdout_mstream.buf;
}

void console_pipe_exit() {
#ifdef CONFIG_CONSOLE_PIPE_MSTREAM
    fclose(stdout);
#else
    stdout_mstream.buf[stdout_mstream.idx] = '\0';
    stdout_mstream.idx = 0;
    stdout_mstream.len = 0;
#endif
    stdout = stdout_mstream.bak;
}

/* This is my version of open_memstream based on funopen
FILE open_memstream_writeonly(char * *ptr, size_t *size) {
    console_pipe_init();
    char *buf = (char *)malloc(64);
    if (buf == NULL) return NULL;
    stdout_mstream.buf = buf;
    stdout_mstream.idx = 0;
    stdout_mstream.len = 64;
    size = &stdout_mstream.len;
    ptr = &stdout_mstream.buf;
    return stdout_mstream.obj;
}
*/
