#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "json.h"
#include "libs/unicode/unicode.h"

/*
 * to do:
 * - deepCopy
 *
 * There may seem to be inconsistencies with error handling and returns. However, bar mistakes,
 * a consistent scheme is used:
 * return NULL (if applicable) for error (such as an un-parseable string or if a malloc fails).
 * return &JSON_NULLVAL (if applicable) when a json entry was expected but not found.
 * use function `reject` if the error bars the function from being used in a pipeline (Indexing a non-array).
 * set JSON_ERROR without `reject` if the function can't fulfill its usual purpose, but can still return a logical value
 * * (Indexing an array out of bounds returns &JSON_NULLVAL).
*/

static int unescapeString(char*);
static int stripWhitespace(char*);
static char* skipString(char*);

enum FAILTYPE {
    PARSE,
    USAGE,
    ACCESS_STRING
};

static void reject(enum FAILTYPE, JSON_ERROR_ENUM);

static const char jsonEscapes[128] = {
    ['b'] = '\b',
    ['f'] = '\f',
    ['n'] = '\n',
    ['r'] = '\r',
    ['t'] = '\t',
    ['"'] = '"',
    ['\\'] = '\\',
    ['/'] = '/',
    ['u'] = 'u'
};

static const char jsonEscapesReverse[128] = {
    ['\b'] = 'b',
    ['\f'] = 'f',
    ['\n'] = 'n',
    ['\r'] = 'r',
    ['\t'] = 't',
    ['\"'] = '"',
    ['\\'] = '\\',
    ['/'] = '/',
    ['u'] = 'u'
};

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
        reject(USAGE, APPENDNONARR);
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
        reject(USAGE, INDEXNONARR);
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
        reject(USAGE, INDEXNONARR);
        return NULL;
    }
    if (index >= entry->data.array.length || index < 0) {
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

void JSON_remove (JSON_entry* entry, char* key) {
    int index = JSON_getKeyIndex(entry, key);
    memmove(&entry->data.object.keys[index], &entry->data.object.keys[index + 1], entry->data.object.values->data.array.length - index);
    JSON_pop(entry->data.object.values, index);
}

// string

JSON_entry* JSON_newString(char* base) {
    JSON_entry* entry = malloc(sizeof(JSON_entry));
    entry->type = STRING;

    entry->data.string.str = strdup(base);
    int newCapacity = unescapeString(entry->data.string.str) + 1;
    entry->data.string.str = realloc(entry->data.string.str, newCapacity);
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

// write JSON

static void _indent(FILE* buffer, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(buffer, "    ");
    }
}

int writeEscapeString(FILE* buffer, char* string) {
    char* start = string;
    while (true) {
        for (; (unsigned char) *string > 0x1f && *string != 0x7f; string++);
        fwrite(start, sizeof(char), string - start, buffer);
        if (*string == '\0') break;
        int code = UNICODE_toCodePoint(string);
        char c = jsonEscapesReverse[*string];
        if (c != 0) {
            fprintf(buffer, "\\%c", c);
        }
        else {
            fprintf(buffer, "\\u%04x", code);
        }
        string++;
        start = string;
    }
    if (fflush(buffer) != 0) {
        perror("writeEscapeString");
        return -1;
    }
}

void JSON_write(FILE* buffer, JSON_entry* entry, int indent) {
    switch (entry->type) {
        case STRING:
            fprintf(buffer, "\"");
            writeEscapeString(buffer, entry->data.string.str);
            fprintf(buffer, "\"");
            break;
        case NUMBER:
            fprintf(buffer, "%g", entry->data.number);
            break;
        case OBJECT:
            fprintf(buffer, "{\n");
            for (int i = 0; i < entry->data.object.values->data.array.length; i++) {
                _indent(buffer, indent);
                fprintf(buffer, "\"");
                writeEscapeString(buffer, entry->data.object.keys[i]);
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

// free

void JSON_free(JSON_entry* entry) {
    switch (entry->type) {
        case STRING:
            free(entry->data.string.str);
            break;
        case NUMBER:
            break;
        case OBJECT:
            JSON_free(entry->data.object.values);
            free(entry->data.object.keys);
            break;
        case ARRAY:
            for (int i = 0; i < entry->data.array.length; i++) {
                JSON_free(entry->data.array.items[i]);
            }
            free(entry->data.array.items);
            break;
        case BOOL:
            break;
        case NULLTYPE:
            return;
        default:
            break;
    }
    free(entry);
}

struct accessEntry {
    enum {INDEX, KEY} accessType;
    union {
        char* key;
        int index;
    } data;
    int keyLength;
};

static char* accessStringNext(char* accessString, struct accessEntry* entry) {
    if (*accessString == '[') {
        entry->accessType = INDEX;
        entry->data.index = strtol(accessString + 1, &accessString, 10);
        entry->keyLength = -1; // unused for indices
        if (*accessString != ']') {
            reject(ACCESS_STRING, INVALIDNUMBER);
            return NULL;
        }
        accessString++;
    }
    else if (*accessString == '.') {
        entry->data.key = accessString + 1;
        entry->accessType = KEY;
        accessString++;
        for (; *accessString != '.' && *accessString != '[' && *accessString != '\0'; accessString++) {
            if (*accessString == '\\') accessString++;
        }
        entry->keyLength = accessString - entry->data.key;
    }
    else {
        reject(ACCESS_STRING, EXPECTEDACCESSVAL);
        return NULL;
    }
    return accessString;
}

JSON_entry* JSON_deepAccess(JSON_entry* entry, char* accessString) {
    // rules for an access string:
    // - keys begin with a '.' and are assumed to continue until
    // -- another '.',
    // -- a '[' (indicating an index)
    // -- a null terminator.
    // - as suggested above, indices are encosed by '[]'
    accessString = strdup(accessString);
    stripWhitespace(accessString);
    char* original = accessString;
    struct accessEntry state;
    JSON_entry* proxy = entry;
    while (*accessString != '\0') {
        accessString = accessStringNext(accessString, &state);
        if (accessString == NULL) {
            free(original);
            return NULL;
        }
        if (state.accessType == KEY) {
            char temp = state.data.key[state.keyLength];
            state.data.key[state.keyLength] = '\0';
            proxy = JSON_access(proxy, state.data.key);
            if (proxy == NULL) return NULL;
            state.data.key[state.keyLength] = temp;
        }
        else if (state.accessType == INDEX) {
            proxy = JSON_index(proxy, state.data.index);
            if (proxy == NULL) return NULL;
        }
        else {
            reject(ACCESS_STRING, PANIC);
            return NULL;
        }
    }
    free(original);
    return proxy;
}

JSON_entry** JSON_deepWaccess(JSON_entry* entry, char* accessString) {
    accessString = strdup(accessString);
    stripWhitespace(accessString);
    char* original = accessString;
    char* last = accessString;
    JSON_entry* proxy = entry;
    for (; *accessString != '\0'; accessString++) {
        if (*accessString == '\\') continue;
        if (*accessString == '.' || *accessString == '[') last = accessString;
    }
    struct accessEntry state;
    accessStringNext(last, &state); // intentionally not assigning last to the result
    *last = '\0';
    last++;
    proxy = JSON_deepAccess(proxy, original);
    JSON_entry** returnVal;
    if (state.accessType == KEY) {
        returnVal = JSON_waccess(proxy, state.data.key);
    }
    else if (state.accessType == INDEX) {
        returnVal = JSON_windex(proxy, state.data.index);
    }
    free(original);
    return returnVal;
}

// parsing error handler

static void reject(enum FAILTYPE f, JSON_ERROR_ENUM error) {
    char* failtypeMessages[] = {
        "Parsing failed: ",
        "Incorrect usage: ",
        "Invalid access string: "
    };
    fprintf(stderr, "%s", failtypeMessages[f]);
    JSON_ERROR = error;
    JSON_perror();
}

// object text traversal
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
    else return NULL;
    return start;
}

char* skipNull(char* start) {
    char nullText[] = "null";
    if (strncmp(start, nullText, sizeof(nullText) - 1) == 0) {
        start += sizeof(nullText) - 1;
    }
    else return NULL;
    return start;
}

static char* skipString(char* start) {
    start++; // skip first quote
    while (true) {
        if (*start == '\\') {
            start += 2;
        }
        else if (*start == '\"') {
            break;
        }
        else if (*start == 0) {
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
            start = skipString(start) - 1;
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
            start = skipString(start) - 1;
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
    // returns pointer to the next entry, unless there is an error in the source string, in which case it returns NULL
    // determine that it has found the last entry by comparing *return to '\0'.
    char* original = current;
    state->name = NULL; // unused field when getting inside an array
    state->nameLength = -1;
    switch (*current) {
        case '\"':
            state->type = STRING;
            state->firstChar = current;
            current = skipString(current);
            if (current == NULL) {
                reject(PARSE, UNMATCHEDQUOTE);
                state->type = NONVAL;
                return NULL;
            }
            state->length = current - state->firstChar;
            break;
        case '[':
            state->type = ARRAY;
            state->firstChar = current;
            current = skipArray(current);
            if (current == NULL) {
                reject(PARSE, UNMATCHEDBRACKET);
                state->type = NONVAL;
                return NULL;
            }
            state->length = current - state->firstChar;
            break;
        case '{':
            state->type = OBJECT;
            state->firstChar = current;
            current = skipObject(current);
            if (current == NULL) {
                reject(PARSE, UNMATCHEDBRACE);
                state->type = NONVAL;
                return NULL;
            }
            state->length = current - state->firstChar;
            break;
        case 't':
        case 'f':
            state->type = BOOL;
            state->firstChar = current;
            current = skipBool(current);
            if (current == NULL) {
                reject(PARSE, INVALIDBOOL);
                state->type = NONVAL;
                return NULL;
            }
            state->length = current - state->firstChar;
            break;
        case 'n':
            state->type = NULLTYPE;
            state->firstChar = current;
            current = skipNull(current);
            if (current == NULL) {
                reject(PARSE, INVALIDNULL);
                state->type = NONVAL;
                return NULL;
            }
            state->length = current - state->firstChar;
            break;
        default:
            if (isFloatChar(*current)) {
                state->type = NUMBER;
                state->firstChar = current;
                current = skipNumber(current);
                if (current == NULL) {
                    reject(PARSE, INVALIDNUMBER);
                    state->type = NONVAL;
                    return NULL;
                }
                state->length = current - state->firstChar;
                break;
            }
            else {
                reject(PARSE, INVALIDVAL);
                state->type = NONVAL;
                return NULL;
            }
    }
    // all cases, if successful, should leave `current` pointing to either a comma or the null terminator of a string.
    if (*current != ',') {
        if (*current != '\0') {
            reject(PARSE, EXPECTEDCOMMA);
            state->type = NONVAL;
            return NULL;
        }
    }
    current++; // skip comma
    return current;
}

static char* objectNext(char* current, JSON_textEntry* state) {
    if (*current == 0) {
        state->type = NONVAL;
        return NULL;
    }
    char* name = current + 1;
    current = skipString(current);
    if (current == NULL) {
        reject(PARSE, UNMATCHEDQUOTE);
        state->type = NONVAL;
        return NULL;
    }
    int nameLength = current - name - 1;
    if (*current != ':') {
        reject(PARSE, EXPECTEDCOLON);
        state->type = NONVAL;
        return NULL;
    }
    if (current) current++;
    current = arrayNext(current, state);
    if (current == NULL) {
        return NULL;
    }
    state->name = name;
    state->nameLength = nameLength;
    return current;
}

static int getCodePoint(char* template) {
    // assumes `template` is aligned to the 'u' in a unicode escape sequence (\u0041)
    long int codePoint;
    template++;
    codePoint = strtol(template, NULL, 16);
    if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
        int extended = strtol(template + 6, NULL, 16);
        codePoint -= 0xd800;
        extended -= 0xdc00;
        codePoint <<= 10;
        codePoint |= extended;
        codePoint += 0x10000;
    }
    return (int) codePoint;
}

static int decodeUnicode(char* template) {
    // assumes `template` is aligned to the 'u' in a unicode escape sequence (\u0041)
    int codePoint = getCodePoint(template);
    char* unicode = UNICODE_fromCodePoint(codePoint);
    int length = strlen(unicode);
    int beginning = 5 - length;
    strcpy(template + beginning, unicode);

    free(unicode);
    return beginning + 1;
}

static int decodeEscape(char* template) {
    // assumes `template` is aligned to the indicating character in a unicode escape sequence (the `u` in \u0041)
    char val = jsonEscapes[template[0]];
    if (val == 'u') {
        return decodeUnicode(template);
    }
    else {
        template[0] = val;
        return 1;
    }
}

static int unescapeString(char* string) {
    char* readPtr;
    char* writePtr;
    for (readPtr = string; *readPtr != '\\' && *readPtr != 0; readPtr++); // align readPtr to the first backslash
    writePtr = readPtr;
    while (*readPtr != 0) {
        if (*readPtr == '\\') {
            int length = decodeEscape(readPtr + 1);
            readPtr += length;
        }
        *writePtr = *readPtr;
        readPtr++;
        writePtr++;
    }
    *writePtr = 0;
    return writePtr - string;
}

static char* escapeString(char* string) {
    void* original = string;
    int totalLength = strlen(string) + 1;

    char* endBuffer = malloc(totalLength * 6); // the largest an escaped string can be compared to the unescaped version is 6 times larger
    if (endBuffer == NULL) {
        perror("escapeString");
        return NULL;
    }

    char* writePtr = endBuffer;
    for (; *string != '\0'; string++) {
        bool printable = (*string > 0x1f && *string != 0x7f);
        if (!printable) {
            int code = UNICODE_toCodePoint(string);
            (*writePtr = '\\', writePtr++);
            char escape = jsonEscapesReverse[*string];
            if (escape != 0) (*writePtr = escape, writePtr++);
            else writePtr += sprintf(writePtr, "%04x", code);
        }
        else {
            *writePtr = *string;
            writePtr++;
        }
    }
    *writePtr = 0;
    printf("%s\n", endBuffer);
    endBuffer = realloc(endBuffer, writePtr - endBuffer + 1);
    return endBuffer;
}

static int stripWhitespace(char* string) {
    char* nextEmpty = string;
    char* head;
    for(; !isspace(*nextEmpty); nextEmpty++) if (*nextEmpty == 0) break;
    for(head = nextEmpty; *head != 0; head++) {
        if (isspace(*head)) continue;
        if (*head == '\"') {
            char* temp = head;
            head = skipString(head);
            if (head == NULL) {
                reject(PARSE, UNMATCHEDQUOTE); 
                return -1;
            }
            for (; temp < head; temp++) {
                *nextEmpty = *temp;
                nextEmpty++;
            }
        }
        if (isspace(*head)) continue;
        *nextEmpty = *head;
        nextEmpty++;
    }
    *nextEmpty = 0;
    return nextEmpty - string;
}

static JSON_entry* _JSON_fromString(char* string, bool base) {
    if (strncmp(string, "\"Payment", 8) == 0) {
        printf("okay\n");
    }
    JSON_textEntry e;
    JSON_textEntry* entry = &e;
    char* originalString;
    if (base) {
        string = strdup(string); // to ensure writability, but only at the top level
        originalString = string;
        if (stripWhitespace(string) == -1) {
            free(originalString);
            return NULL;
        }
    }
    string = arrayNext(string, entry);
    if (string == NULL) {
        return NULL;
    }
    JSON_entry* returnVal;
    char* nullTerminated = strndup(entry->firstChar, entry->length);
    int nullTerminatedLength = entry->length;
    char* originalNT = nullTerminated;
    switch (entry->type) {
        case NUMBER:
            double floatValue;
            floatValue = strtod(entry->firstChar, &entry->firstChar);
            returnVal = JSON_newNumber(floatValue);
            break;
        case STRING:
            nullTerminated[nullTerminatedLength - 1] = 0;
            nullTerminated++;
            returnVal = JSON_newString(nullTerminated);
            break;
        case OBJECT:
            // object does not need to be validated
            nullTerminated[nullTerminatedLength - 1] = 0;
            nullTerminated++;
            returnVal = JSON_newObj();
            do {
                nullTerminated = objectNext(nullTerminated, entry);
                if (nullTerminated == NULL) {
                    free(returnVal);
                    returnVal = NULL;
                    break;
                }
                char* key = strndup(entry->name, entry->nameLength);
                char* objValue = strndup(entry->firstChar, entry->length);
                JSON_update(returnVal, key, _JSON_fromString(objValue, false));
                free(key);
                free(objValue);
            } while (*nullTerminated != '\0');
            break;
        case ARRAY:
            // array does not need to be validated
            nullTerminated[nullTerminatedLength - 1] = 0;
            nullTerminated++;
            returnVal = JSON_newArray();
            do {
                nullTerminated = arrayNext(nullTerminated, entry);
                if (nullTerminated == NULL) {
                    free(returnVal);
                    returnVal = NULL;
                    break;
                }
                char* appendValue = strndup(entry->firstChar, entry->length);
                JSON_append(returnVal, _JSON_fromString(appendValue, false));
                free(appendValue);
            } while (*nullTerminated != '\0');
            break;
        case BOOL:
            returnVal = JSON_newBool(strcmp(nullTerminated, "true") == 0);
            break;
        case NULLTYPE:
            returnVal = &JSON_NULLVAL;
            break;
        default:
            reject(PARSE, PANIC);
    }
    free(originalNT);
    if (base) free(originalString);
    return returnVal;
}

JSON_entry* JSON_fromString(char* string) {
    return _JSON_fromString(string, true);
}

JSON_entry* JSON_fromFile(char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("JSON_fromFile open");
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("JSON_fromFile fstat");
        return NULL;
    }
    char* buffer = malloc(st.st_size + 1);
    if (buffer == NULL) {
        perror("JSON_fromFile malloc");
        return NULL;
    }
    read(fd, buffer, st.st_size);
    buffer[st.st_size] = 0;
    JSON_entry* returnVal = JSON_fromString(buffer);
    free(buffer);
    return returnVal;
}

void JSON_perror(void) {
    char* errors[] = {
        "Success",
        "Attempt to access non-object using JSON_access",
        "Attempt to index non-array using JSON_index",
        "Attempt to update non-object using JSON_update",
        "Attempt to append to non-array using JSON_append",
        "Index out of bounds",
        "Attempt to catenate to non-string using JSON_strCat",

        "Unmatched ' \" '",
        "Unmatched ' [ '",
        "Unmatched ' { ' ",
        "Number invalid",
        "Value invalid",
        "Value invalid",
        "Value invalid",
        "Value invalid",
        "Expected ' , '",
        "Expected ' : '",
        "Expected ' [ '",

        "Valid access value (string, int) expected",
        "Expected key; got empty string",

        "Something unexpected went wrong. Have fun"
    };

    fprintf(stderr, "%s\n", errors[JSON_ERROR]);
}

int main(void) {
    JSON_entry* result = JSON_fromFile("test.json");
    if (result == NULL) {
        return -1;
    }
    JSON_entry** downloads = JSON_deepWaccess(result, ".downloads.server.url");
    *downloads = JSON_newString("hello world");
    JSON_write(stdout, JSON_access(result, "downloads"), 1);
}
