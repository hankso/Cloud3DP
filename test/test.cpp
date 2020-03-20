#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_setvbuf() {
    char *tmp = (char *)calloc(1024, sizeof(char));
    if (tmp != NULL) {
        setvbuf(stdout, tmp, _IOFBF, 1024);
        printf("asdfasdf\n");
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
    printf("printf: asdfadsf\n");
    fprintf(stdout, "fprintf: 12345678\n");
    fclose(stdout); stdout = stdout_bak;
    printf("%lu: `%s`\n", size, buf);
}

void record_ptr(char * *ptr) {
    char *buf1;
    printf("Buf1 %p: %u\n", buf1, buf1);
    *ptr = NULL;
}

char * test_pointer() {
    char *buf;
    printf("Buf %p: %u\n", buf, buf);
    record_ptr(&buf);
    return buf;
}

int main() {
    // test_setvbuf();
    // test_memstream();
    // char *ptr = test_pointer();
    // printf("Out %p: %u", ptr, ptr);
    printf("char: ``%c``\n", "asdf"[1]);
    return 0;
}
