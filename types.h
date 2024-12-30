#ifndef JSON_TYPES_H
#define JSON_TYPES_H
#include <stdint.h>

typedef struct JSON_entry JSON_entry;
typedef double JSON_Number;

typedef struct {
    struct JSON_entry** items;
    int length;
    int capacity;
} JSON_Array;

typedef struct {
    char* str;
    int length;
    int capacity;
} JSON_String;

typedef struct {
    char** keys;
    JSON_entry* values; // stores the length and capacity
} JSON_Object; // not a hash table for now...

typedef uint8_t JSON_Bool;

typedef uint8_t JSON_Null; // not strictly necessary, but feels incomplete without

enum JSON_TYPE {
    STRING,
    NUMBER,
    OBJECT,
    ARRAY,
    BOOL,
    NULLTYPE
};

typedef union {
    JSON_String string;
    JSON_Number number;
    JSON_Object object;
    JSON_Array array;
    JSON_Bool boolean;
    JSON_Null null;
} JSON_any;

struct JSON_entry {
    enum JSON_TYPE type;
    JSON_any data;
};

struct JSON_repo {
    JSON_entry* trackedItems;
};

typedef enum {
    NONE,
    ACCESSNONOBJ,
    INDEXNONARR,
    UPDATENONOBJ,
    APPENDNONARR,
    INDEXOUTOFBOUNDS
} JSON_ERROR_ENUM;

JSON_ERROR_ENUM JSON_ERROR = NONE;


JSON_entry* JSON_fromString(char* string);

JSON_entry* JSON_access(JSON_entry* obj, char* key); // like object["key"] in javaScript

JSON_entry* JSON_index(JSON_entry* arr, int index);

JSON_entry** JSON_waccess(JSON_entry* obj, char* key); // recursively frees the resources for the entry associated with `key` so that you can overwrite it without leak.

JSON_entry** JSON_windex(JSON_entry* arr, int index); // array counterpart for `waccess`

JSON_entry* JSON_toString(JSON_entry* entry);

JSON_entry* JSON_deepAccess(JSON_entry* obj, char* accessString); // parses a string formatted like `["key"][index]...` and returns the value indicated.

JSON_entry* JSON_deepWaccess(JSON_entry* obj, char* accessString); // deep access version of `waccess`

JSON_entry* JSON_append(JSON_entry* arr, JSON_entry* item);

JSON_entry* JSON_update(JSON_entry* obj, char* key, JSON_entry* value); // add a new key-value pair to an object or update an existing one

void JSON_perror(void); // print custom messages based on the value held in `JSON_ERROR_ENUM JSON_ERROR`

JSON_entry* JSON_newObj(void);
JSON_entry* JSON_newArray(void);
JSON_entry* JSON_newString(char* base);

struct JSON_repo newRepo(void);
void commit(struct JSON_repo* repo, JSON_entry* item);
int audit(struct JSON_repo* repo);
int JSON_free(JSON_entry* entry);

JSON_entry JSON_NULLVAL = {
    .type = NULLTYPE,
};
#endif
