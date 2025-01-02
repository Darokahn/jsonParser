#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "json.h"

/*
 * THIS IS CURRENTLY MARK 0
 * it is a naive little baby program. It believes anything you give it to parse is valid JSON.
 * if you give it invalid JSON, the little baby will segfault.
 *
 * current limitations:
 * * parser doesn't convert escaped sequences in strings to their literal byte values
 * * fails unpredictably if invalid json is passed
 * * a few important functions have not been written (JSON_remove, JSON_deepAccess, JSON_deepWaccess, JSON_perror)
*/

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
    JSON_free(entry->data.array.items[index], true);
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
    JSON_free(entry->data.object.values->data.array.items[i], true);
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

static char* unescapeString(char* string) {
}

JSON_entry* JSON_newString(char* base) {
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    entry->type = STRING;

    entry->data.string.str = strdup(base);
    entry->data.string.length = strlen(base) + 1;
    entry->data.string.capacity = entry->data.string.length;
    return entry;
}

void JSON_strCat(JSON_entry* entry, char* newStr) {
    if (entry->type != STRING) {
        JSON_ERROR = CATNONSTRING;
        return;
    }
    entry->data.string.capacity = entry->data.string.capacity + strlen(newStr);
    entry->data.string.str = realloc(entry->data.string.str, entry->data.string.capacity);
    entry->data.string.length = entry->data.string.capacity;
    strcat(entry->data.string.str, newStr);
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
            fprintf(buffer, "uh oh");
            break;
    }
}

int JSON_free(JSON_entry* entry, bool base) {
    switch (entry->type) {
        case STRING:
            free(entry->data.string.str);
            break;
        case NUMBER:
            break;
        case OBJECT:
            JSON_free(entry->data.object.values, false);
            free(entry->data.object.keys);
            break;
        case ARRAY:
            for (int i = 0; i < entry->data.array.length; i++) {
                JSON_free(entry->data.array.items[i], false);
            }
            free(entry->data.array.items);
            break;
        case BOOL:
            break;
        case NULLTYPE:
            break;
        default:
            break;
    }
    if (!base) {
        free(entry);
    }
    else {
        *entry = (JSON_entry) {
            .type = NULLTYPE
        };
    }
}

JSON_entry* JSON_deepAccess(JSON_entry* entry, char* accessString) {
}

JSON_entry** JSON_deepWaccess(JSON_entry* entry, char* accessString) {
}

// all skip methods assume you have properly aligned `start` to the beginning of a value.

static char* skipArray(char*);
static char* skipObject(char*); // forward declarations

static char* skipToComma(char* start) {
    if (start == NULL) {
        return NULL;
    }
    for (; *start != 0; start++) {
        if (*start == ',') return start;
    }
    return NULL;
}

static char* skipToColon(char* start) {
    if (start == NULL) {
        return NULL;
    }
    for (; *start != 0; start++) {
        if (*start == ':') return start;
    }
    return NULL;
}

static bool isFloatChar(char c) {
    return isdigit(c) || c == '.' || c == 'e' || c == '-';
}

static char* skipNumber(char* start) {
    while (isFloatChar(*start)) start++;
    return start;
}

static char* skipBool(char* start) {
    char trueText[] = "true";
    char falseText[] = "false";
    if (strncmp(start, trueText, sizeof(trueText) - 1) == 0) {
        start += sizeof(trueText) - 1;
    }
    else if (strncmp(start, falseText, sizeof(falseText) - 1) == 0) {
        start += sizeof(falseText) - 1;
    }
    return start;
}

char* skipNull(char* start) {
    char nullText[] = "null";
    if (strncmp(start, nullText, sizeof(nullText) - 1) == 0) {
        start += sizeof(nullText) - 1;
    }
    return start;
}

static char* skipString(char* start) {
    start++; // skip first quote
    while (true) {
        if (*start == '\\') {
            start += 2;
        }
        if (*start == '\"') {
            break;
        }
        if (*start == 0) {
            return NULL;
        }
        start++;
    }
    start++; // skip last quote
    return start;
}

static char* skipArray(char* start) {
    int open = 0;
    for (; *start != 0; start++) {
        if (*start == '\"') {
            start = skipString(start);
        }
        if (*start == '[') {
            open++;
        }
        else if (*start == ']') {
            open--;
        }
        if (open == 0) {
            start++; // skip final bracket;
            return start;
        }
    }
    return NULL;
}

static char* skipObject(char* start) {
    int open = 0;
    for (; *start != 0; start++) {
        if (*start == '\"') {
            start = skipString(start);
        }
        if (*start == '{') {
            open++;
        }
        else if (*start == '}') {
            open--;
        }
        if (open == 0) {
            start++; // skip final bracket;
            return start;
        }
    }
    return NULL;
}

static char* arrayNext(char* current, JSON_textEntry* state) {
    state->name = NULL; // unused field when getting inside an array
    state->nameLength = -1;
    while (isspace(*current)) current++;
    switch (*current) {
        case '\"':
            state->type = STRING;
            state->firstChar = current;
            current = skipString(current);
            state->length = current - state->firstChar;
            break;
        case '[':
            state->type = ARRAY;
            state->firstChar = current;
            current = skipArray(current);
            state->length = current - state->firstChar;
            break;
        case '{':
            state->type = OBJECT;
            state->firstChar = current;
            current = skipObject(current);
            state->length = current - state->firstChar;
            break;
        case 't':
        case 'f':
            state->type = BOOL;
            state->firstChar = current;
            current = skipBool(current);
            state->length = current - state->firstChar;
            break;
        case 'n':
            state->type = NULLTYPE;
            state->firstChar = current;
            current = skipNull(current);
            state->length = current - state->firstChar;
            break;
        default:
            if (isFloatChar(*current)) {
                state->type = NUMBER;
                state->firstChar = current;
                current = skipNumber(current);
                state->length = current - state->firstChar;
                break;
            }
            else {
                state->type = NONVAL;
            }
    }
    current = skipToComma(current);
    if (current) current++;
    if (current == NULL || *current == 0) { // `current == NULL` to escape early if `*current` will segfault
        return NULL;
    }
    return current;
}

static char* objectNext(char* current, JSON_textEntry* state) {
    if (*current == 0) {
        state->type = NONVAL;
        return NULL;
    }
    char* name = current + 1;
    current = skipString(current);
    int nameLength = current - name - 1;
    current = skipToColon(current);
    if (current) current++;
    current = arrayNext(current, state);
    state->name = name;
    state->nameLength = nameLength;
    return current;
}

static int stripWhitespace(char* string) {
    int total = 0;
    char* nextEmpty = string;
    char* head;
    for(; !isspace(*nextEmpty); nextEmpty++) if (*nextEmpty == 0) break;
    for(head = nextEmpty; *head != 0; head++) {
        if (isspace(*head)) continue;
        total++;
        *nextEmpty = *head;
        nextEmpty++;
    }
    *nextEmpty = 0;
    return total;
}

JSON_entry* JSON_fromString(char* string) {
    JSON_textEntry* entry = malloc(sizeof(JSON_textEntry));
    string = strdup(string); // we need to write to it
    stripWhitespace(string);
    string = arrayNext(string, entry);
    JSON_entry* returnVal;
    char* nullTerminated = strndup(entry->firstChar, entry->length);
    int nullTerminatedLength = entry->length;
    char* original = nullTerminated;
    switch (entry->type) {
        case NUMBER:
            returnVal = JSON_newNumber(atof(nullTerminated));
            break;
        case STRING:
            nullTerminated[nullTerminatedLength - 1] = 0;
            nullTerminated++;
            returnVal = JSON_newString(nullTerminated);
            break;
        case OBJECT:
            nullTerminated[nullTerminatedLength - 1] = 0;
            nullTerminated++;
            returnVal = JSON_newObj();
            do {
                nullTerminated = objectNext(nullTerminated, entry);
                if (entry->type == NONVAL) {
                    break;
                }
                char* key = strndup(entry->name, entry->nameLength);
                char* objValue = strndup(entry->firstChar, entry->length);
                JSON_update(returnVal, key, JSON_fromString(objValue));
                free(key);
                free(objValue);
            } while (nullTerminated != NULL);
            break;
        case ARRAY:
            nullTerminated[nullTerminatedLength - 1] = 0;
            nullTerminated++;
            returnVal = JSON_newArray();
            do {
                nullTerminated = arrayNext(nullTerminated, entry);
                if (entry->type == NONVAL) {
                    break;
                }
                char* appendValue = strndup(entry->firstChar, entry->length);
                JSON_append(returnVal, JSON_fromString(appendValue));
                free(appendValue);
            } while (nullTerminated != NULL);
            break;
        case BOOL:
            returnVal = JSON_newBool(strcmp(nullTerminated, "true") == 0);
            break;
        case NULLTYPE:
            returnVal = &JSON_NULLVAL;
            break;
        default:
            break;
    }
    free(original);
    free(string);
    return returnVal;
}

JSON_entry* JSON_fromFile(char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("JSON_fromFile");
        return NULL;
    }
    size_t size;
    ioctl(fd, FIONREAD, &size);
    char* buffer = malloc(size + 1);
    read(fd, buffer, size);
    buffer[size] = 0;
    JSON_entry* returnVal = JSON_fromString(buffer);
    free(buffer);
    return returnVal;
}


void JSON_perror(void) {
}

int main(void) {
    JSON_entry* results = JSON_fromFile("test.json");
    JSON_write(stdout, results, 1);
    JSON_free(results, true);
}
