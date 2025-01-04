#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "unicode.h"

void printBytes(char* string) {
    for (; *string != 0; string++) {
        printf("%x ", (uint8_t)*string);
    }
    printf("\n");
}

char* UNICODE_fromCodePoint(int code) {
    if (code > 0x10ffff) {
        // utf-8 defines a 4-byte limit for unicode characters, but still defines behavior for up to 6-byte characters.
        // this limit is arbitrary and can be changed if your application uses an expanded character encoding.
        fprintf(stderr, "UNICODE_fromCodePoint: code out of range\n");
        return NULL;
    }
    char* bytes = strndup(UNICODE_TEMPLATEBYTES, 6);
    if (bytes == NULL) {
        perror("UNICODE_fromCodePoint");
        return bytes;
    }

    int* threshold;
    for (threshold = UNICODE_THRESHOLDS; *threshold <= code; threshold++);

    int byteCount = (threshold - UNICODE_THRESHOLDS) + 1;
    bytes[0] = UNICODE_INITIALS[byteCount - 1];

    int i;
    char mask = 0x3f;

    if (byteCount == 1) mask = 0xFF;
    for (i = 0; i < byteCount; i++) {
        bytes[i] |= (code >> (6 * (byteCount - i - 1))) & mask;
    }
    bytes[i] = '\0';
    return bytes;
}

int UNICODE_toCodePoint(char* string) {
    uint8_t leading;

    int byteCount = 0;
    for (leading = string[0]; (leading << byteCount) & 0x80; byteCount++);
    if (byteCount == 0) byteCount = 1;

    int leadingMask = UNICODE_INITIALS[byteCount - 1];
    
    int code = 0;
    uint8_t c;
    for (int i = 0; i < byteCount; i++) {
        c = string[i];
        if (i == 0) c ^= leadingMask;
        else c ^= 0x80;
        code += c << (6 * (byteCount - i - 1));
    }
    return code;
}

int main() {
    char* a = UNICODE_fromCodePoint(0x41);
    char* b = UNICODE_fromCodePoint(0xe9);
    char* c = UNICODE_fromCodePoint(0x905);
    char* d = UNICODE_fromCodePoint(0x1f600);

    UNICODE_toCodePoint(a);
    UNICODE_toCodePoint(b);
    UNICODE_toCodePoint(c);
    UNICODE_toCodePoint(d);
    printf("%s %s %s %s\n", a, b, c, d);
}
