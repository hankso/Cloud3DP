#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BIT(n) (1UL << (n))

void test_setvbuf() {
    char *tmp = (char *)calloc(1024, sizeof(char));
    if (tmp != NULL) {
        setvbuf(stdout, tmp, _IOFBF, 1024);
        printf("test string\n");
        printf("%d | 0x%02x | %f\n", 1, 15, 3.1);
        // fflush(stdout);
        setvbuf(stdout, NULL, _IONBF, 0);
    }
    printf("Get result %lu:\n%s\n", strlen(tmp), tmp);
}

void test_memstream() {
    char *buf; size_t size;
    FILE *stdout_bak = stdout;
    stdout = open_memstream(&buf, &size);
    printf("printf: test string\n");
    fprintf(stdout, "fprintf: 12345678\n");
    fclose(stdout); stdout = stdout_bak;
    printf("%lu: `%s`\n", size, buf);
}

void record_ptr(char * *ptr) {
    char *buf1;
    printf("Buf1 %p: %u\n", buf1, buf1);
    *ptr = NULL;
}

static char *tmp;

char * test_pointer() {
    char *buf;
    printf("Buf %p: %u\n", buf, buf);
    tmp = buf;
    record_ptr(&buf);
    return buf;
}

typedef struct {
    union {
        struct {
            uint8_t r, g, b;
        };
        uint32_t color; // big-endian: r-g-b, little-endian: b-g-r
    };
    bool reset;
} rgb_color;

void test_color() {
    rgb_color c = { 0xff, 0x00, 0xff };
    c.color = 0x80aaee;
    printf("Struct pointer size: %lu\n", sizeof(rgb_color *));
    printf("Color: r(%02X) g(%02X) b(%02X) val: %u, #%02X, Struct size: %lu\n",
           c.r, c.g, c.b, c.color, c.color, sizeof(rgb_color));
    rgb_color *lst = (rgb_color *)calloc(3, sizeof(rgb_color));
    // rgb_color lst[3] = { 0 };
    for (uint8_t i = 0; i < 3; i++) {
        lst[i].color = 0x112233;
        rgb_color *pc = &lst[i];
        pc->reset = true;
        printf("Color: 0x%06X, %d\n", lst[i].color, lst[i].reset);
    }

}

typedef enum {
    U_Z = 5,
    U_START = 99,
    U_0, U_1, U_2, U_3,
    U_END
} MyNum;

void test_enum(MyNum n) {
    printf("Number: %d, param n: %d\n", U_0 + U_1, n);
}

int main() {
    // test_setvbuf();
    // test_memstream();
    // char *ptr = test_pointer();
    // printf("Out %p: %u", ptr, ptr);
    // printf("Out %p: %u", tmp, tmp);
    // printf("char: ``%c``\n", "asdf"[1]);
    // test_color();
    // test_enum(U_END);
    // printf("Number: `%u`\n", strtol("0x", NULL, 0));
    return 0;
}
