#include <stdio.h>
#include <string.h>
#include "unicode.h"

#define strsizeof(string) sizeof(string) - 1

int main() {
    char string[24];
    char* end = string;
    strcpy(string, "hello");
    end += strsizeof("hello");
    end += UNICODE_fromCodePoint(0x263a, end);
    end[0] = 0;
    printf("%s\n", string);
}
