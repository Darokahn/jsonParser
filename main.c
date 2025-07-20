#include <stdio.h>
#include <stddef.h>
#include "json.h"

int main(void) {
    JSON_entry* result = JSON_fromFile("test.json");
    result = JSON_deepCopy(result);
    if (result == NULL) {
        return -1;
    }
    JSON_write(stdout, result, 1);
}
