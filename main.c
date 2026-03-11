#include <stdio.h>
#include <stddef.h>
#include "json.h"

int main(void) {
    JSON_entry* result = JSON_fromFile("test.json");
    JSON_write(stdout, result, 1);
}
