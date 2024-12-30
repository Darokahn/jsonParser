#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"

// generic

int JSON_free(JSON_entry* entry) {
    printf("not implemented\n");
}

// array

JSON_entry* JSON_newArray() {
    JSON_any arr;
    arr.array.length = 0;
    arr.array.capacity = 10;
    arr.array.items = malloc(arr.array.capacity * sizeof (JSON_entry*));
    if (!arr.array.items) {
        perror("JSON_newArray");
        return &JSON_NULLVAL;
    }
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    if (!entry) {
        perror("JSON_newArray");
        return &JSON_NULLVAL;
    }

    entry->type = ARRAY;
    entry->data = arr;
    return entry;
}

JSON_entry* JSON_append(JSON_entry* entry, JSON_entry* item) {
    if (entry->type != ARRAY) {
        JSON_ERROR = APPENDNONARR;
        return &JSON_NULLVAL;
    }
    JSON_Array* arr = &entry->data.array;
    if (arr->length == arr->capacity) {
        arr->capacity *= 2;
        if(!(arr->items = realloc(arr->items, arr->capacity * sizeof(JSON_entry*))))
            perror("JSON_append");
    }
    arr->items[arr->length] = item;
    arr->length++;
    return entry;
}

void JSON_pop(JSON_entry* entry, int index) {
    if (entry->type != ARRAY) {
        JSON_ERROR = APPENDNONARR;
        return;
    }
    JSON_entry** beginning = &(entry->data.array.items[index]);
    JSON_entry** end = &(beginning[1]);
    int length = entry->data.array.length - index - 1;
    length *= sizeof(JSON_entry*);
    memcpy(beginning, end, length);
    entry->data.array.length--;
}

JSON_entry* JSON_index(JSON_entry* entry, int index) {
    if (entry->type != ARRAY) {
        JSON_ERROR = INDEXNONARR;
        return &JSON_NULLVAL;
    }
    if (index > entry->data.array.length || index < 0) {
        JSON_ERROR = INDEXOUTOFBOUNDS;
        return &JSON_NULLVAL;
    }
    return entry->data.array.items[index];
}

JSON_entry** JSON_windex(JSON_entry* entry, int index) {
    if (entry->type != ARRAY) {
        JSON_ERROR = INDEXNONARR;
        return NULL;
    }
    if (index > entry->data.array.length || index < 0) {
        JSON_ERROR = INDEXOUTOFBOUNDS;
        return NULL;
    }
    JSON_free(entry->data.array.items[index]);
    return &(entry->data.array.items[index]);
}

// obj

JSON_entry* JSON_newObj(void) {
    JSON_any obj;
    obj.object.values = JSON_newArray();
    obj.object.keys = malloc(10 * sizeof(char*));
    if (!obj.object.keys) {
        perror("JSON_entry");
        return &JSON_NULLVAL;
    }
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    entry->type = OBJECT;
    entry->data = obj;
    return entry;
}

JSON_entry* JSON_access(JSON_entry* entry, char* key) {
    if (entry->type != OBJECT) { 
        JSON_ERROR = ACCESSNONOBJ;
        return &JSON_NULLVAL;
    }
    for (int i = 0; i < entry->data.object.values->data.array.length; i++) {
        if (strcmp(key, entry->data.object.keys[i]) == 0) {
            return JSON_index(entry->data.object.values, i);
        }
    }
    return &JSON_NULLVAL;
}

JSON_entry** JSON_waccess(JSON_entry* entry, char* key) {
    if (entry->type != OBJECT) { 
        JSON_ERROR = ACCESSNONOBJ;
        return NULL;
    }
    int i;
    for (i = 0; i < entry->data.object.values->data.array.length; i++) {
        if (strcmp(key, entry->data.object.keys[i]) == 0) {
            break;
        }
    }
    JSON_free(entry->data.object.values->data.array.items[i]);
    return &(entry->data.object.values->data.array.items[i]);

}

int JSON_getKeyIndex(JSON_entry* entry, char* key) {
    if (entry->type != OBJECT) { 
        JSON_ERROR = ACCESSNONOBJ;
        return -1;
    }
    int i;
    for (i = 0; i < entry->data.object.values->data.array.length; i++) {
        if (strcmp(key, entry->data.object.keys[i]) == 0) {
            return i;
        }
    }
    return -1;
}

JSON_entry* JSON_update(JSON_entry* entry, char* key, JSON_entry* value) {
    if (entry->type != OBJECT) { 
        JSON_ERROR = UPDATENONOBJ;
        return &JSON_NULLVAL;
    }
    int existing;
    if ((existing = JSON_getKeyIndex(entry, key)) != -1) {
        // reassign the existing JSON_entry structure to hold the data in the new one
        *JSON_windex(entry->data.object.values, existing) = value;
    }
    else if (entry->data.object.values->data.array.length == entry->data.object.values->data.array.capacity) {
        // create new key with dynamic resize
        entry->data.object.keys = realloc(entry->data.object.keys, entry->data.object.values->data.array.capacity * 2 * sizeof(char *));
        entry->data.object.keys[entry->data.object.values->data.array.length] = strdup(key);
        JSON_append(entry->data.object.values, value);
    }
    else {
        // create new key
        entry->data.object.keys[entry->data.object.values->data.array.length] = strdup(key);
        JSON_append(entry->data.object.values, value);
    }
}

int main(void) {
    JSON_entry* obj = JSON_newObj();
    JSON_entry newNumber1 = {
        .type = NUMBER,
        .data = {
            .number = 69
        }
    };
    JSON_entry newNumber2 = {
        .type = NUMBER,
        .data = {
            .number = 1
        }
    };
    JSON_entry newNumber3 = {
        .type = NUMBER,
        .data = {
            .number = 3
        }
    };
    JSON_update(obj, "hello", &newNumber1);
    JSON_update(obj, "hello", &newNumber1);
    JSON_update(obj, "hello!", &newNumber2);
    JSON_update(obj, "newkey", &newNumber3);
    JSON_update(obj, "1", &newNumber3);
    JSON_update(obj, "2", &newNumber3);
    JSON_update(obj, "3", &newNumber3);
    JSON_update(obj, "4", &newNumber3);
    JSON_update(obj, "5", &newNumber3);
    JSON_update(obj, "6", &newNumber3);
    JSON_update(obj, "7", &newNumber3);
    JSON_update(obj, "8", &newNumber3);
    JSON_update(obj, "9", &newNumber3);
    printf("%f\n", JSON_access(obj, "9")->data.number);
}
