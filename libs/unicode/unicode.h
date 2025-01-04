#ifndef UNICODE_H
#define UNICODE_H

static char UNICODE_TEMPLATEBYTES[6] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

static int UNICODE_THRESHOLDS[6] = {
    0x80, 0x800, 0x10000, 0x200000, 0x4000000, 0x80000000
};

static char UNICODE_INITIALS[6] = {
    0x0, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
};

char* UNICODE_fromCodePoint(int);
int UNICODE_toCodePoint(char*);
#endif
