#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "unicode.h"

static int UNICODE_THRESHOLDS[6] = {
    0x80, 0x800, 0x10000, 0x200000, 0x4000000, 0x80000000
};

static char UNICODE_INITIALS[6] = {
    0x0, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
};

int UNICODE_fromCodePoint(int code, char* out) {
    if (code > 0x10ffff) return 0;

    int* threshold;
    for (threshold = UNICODE_THRESHOLDS; *threshold <= code; threshold++);

    int byteCount = (threshold - UNICODE_THRESHOLDS) + 1;
    memset(out, 0x80, byteCount);
    out[0] = UNICODE_INITIALS[byteCount - 1];

    char mask = 0x3f;
    if (byteCount == 1) mask = 0xFF;

    int i;
    for (i = 0; i < byteCount; i++) 
        out[i] |= (code >> (6 * (byteCount - i - 1))) & mask;
    return i;
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
