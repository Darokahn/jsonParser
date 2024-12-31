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

// string

JSON_entry* JSON_newString(char* base) {
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    entry->type = STRING;

    entry->data.string.str = strdup(base);
    entry->data.string.length = strlen(base);
    entry->data.string.capacity = entry->data.string.length;
    return entry;
}

// number, bool

JSON_entry* JSON_newNumber(double value) {
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    *entry = (JSON_entry) {
        .type = NUMBER,
        .data = {
            .number = value
        }
    };
    return entry;
}
JSON_entry* JSON_newBool(bool value) {
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    *entry = (JSON_entry) {
        .type = BOOL,
        .data = {
            .boolean = value
        }
    };
    return entry;
}

// generic

static void _indent(FILE* buffer, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(buffer, "    ");
    }
}

void JSON_write(FILE* buffer, JSON_entry* entry, int indent) {
    switch (entry->type) {
        case STRING:
            fprintf(buffer, "\"%s\"", entry->data.string.str);
            break;
        case NUMBER:
            fprintf(buffer, "%g", entry->data.number);
            break;
        case OBJECT:
            fprintf(buffer, "{\n");
            for (int i = 0; i < entry->data.object.values->data.array.length; i++) {
                _indent(buffer, indent);
                fprintf(buffer, "\"");
                fprintf(buffer, "%s", entry->data.object.keys[i]);
                fprintf(buffer, "\": ");
                JSON_write(buffer, entry->data.object.values->data.array.items[i], indent + 1);
                if (i != entry->data.object.values->data.array.length - 1) {
                    fprintf(buffer, ",\n");
                }
            }
            fprintf(buffer, "\n");
            _indent(buffer, indent - 1);
            fprintf(buffer, "}");
            break;
        case ARRAY:
            fprintf(buffer, "[\n");
            for (int i = 0; i < entry->data.array.length; i++) {
                _indent(buffer, indent);
                JSON_write(buffer, entry->data.array.items[i], indent + 1);
                if (i != entry->data.array.length - 1) {
                    fprintf(buffer, ",\n");
                }
            }
            fprintf(buffer, "\n");
            _indent(buffer, indent - 1);
            fprintf(buffer, "]");
            break;
        case BOOL:
            char* args[] = {"false", "true"};
            fprintf(buffer, "%s", args[entry->data.boolean]);
            break;
        case NULLTYPE:
            fprintf(buffer, "null");
            break;
        default:
            break;
    }
}

int main(void) {
    JSON_entry* obj = JSON_newObj();
    JSON_update(obj, "numbers", JSON_newObj());
    JSON_update(obj, "array", JSON_newArray());
    JSON_update(JSON_access(obj, "numbers"), "1", JSON_newNumber(69));
    JSON_update(JSON_access(obj, "numbers"), "2", JSON_newNumber(100));
    JSON_append(JSON_access(obj, "array"), JSON_newNumber(12));
    JSON_append(JSON_access(obj, "array"), JSON_newNumber(12));
    JSON_append(JSON_access(obj, "array"), JSON_newObj());
    JSON_update(JSON_index(JSON_access(obj, "array"), 2), "hello", JSON_newString("world"));
    JSON_write(stdout, obj, 1);
}
