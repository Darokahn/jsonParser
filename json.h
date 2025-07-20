#ifndef JSON_H
#define JSON_H
#include <stdint.h>
#include <stdbool.h>

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

typedef bool JSON_Bool;

typedef uint8_t JSON_Null; // not strictly necessary, but feels incomplete without

enum JSON_TYPE {
    STRING,
    NUMBER,
    OBJECT,
    ARRAY,
    BOOL,
    NULLTYPE,
    NONVAL
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
    int referenceCount;
};

typedef enum {

    // usage errors

    NONE,
    ACCESSNONOBJ,
    INDEXNONARR,
    UPDATENONOBJ,
    APPENDNONARR,
    INDEXOUTOFBOUNDS,
    KEYNOTFOUND,
    CATNONSTRING,

    // parsing errors

    UNMATCHEDQUOTE,
    UNMATCHEDBRACKET,
    UNMATCHEDBRACE,
    INVALIDNUMBER,
    INVALIDNAME,
    INVALIDBOOL,
    INVALIDNULL,
    INVALIDVAL,
    EXPECTEDCOMMA,
    EXPECTEDCOLON,

    // access string error

    EXPECTEDBRACKET,
    EXPECTEDACCESSVAL,
    EMPTYKEY,
    
    // unknown error

    PANIC

} JSON_ERROR_ENUM;

typedef struct {
    char* name;
    int nameLength;
    enum JSON_TYPE type;
    char* firstChar;
    int length;
} JSON_textEntry;

JSON_entry* JSON_fromString(char* string);

JSON_entry* JSON_fromFile(char* filename);

JSON_entry* JSON_access(JSON_entry* entry, char* key); // like object["key"] in javaScript

JSON_entry* JSON_index(JSON_entry* entry, int index);

JSON_entry** JSON_waccess(JSON_entry* entry, char* key); // recursively frees the resources for the entry associated with `key` so that you can overwrite it without leak. returns reference to the pointer rather than the pointer itself.

JSON_entry** JSON_windex(JSON_entry* entry, int index); // array counterpart for `waccess`

JSON_entry* JSON_toString(JSON_entry* entry);

JSON_entry* JSON_deepAccess(JSON_entry* entry, char* accessString); // parses a string formatted like `["key"][index]...` and returns the value indicated.

JSON_entry** JSON_deepWaccess(JSON_entry* entry, char* accessString); // deep access version of `waccess`

JSON_entry* JSON_append(JSON_entry* entry, JSON_entry* item);

JSON_entry* JSON_update(JSON_entry* entry, char* key, JSON_entry* value); // add a new key-value pair to an object or update an existing one

void JSON_remove(JSON_entry* entry, char* key);

void JSON_pop(JSON_entry* entry, int index);

void JSON_strCat(JSON_entry* entry, char* newStr); // dynamically concatenates the new string onto the entry.

void JSON_perror(void); // print custom messages based on the value held in `JSON_ERROR_ENUM JSON_ERROR`

void JSON_write(FILE* buffer, JSON_entry* entry, int indent);

JSON_entry* JSON_newObj(void);
JSON_entry* JSON_newArray(void);
JSON_entry* JSON_newString(char* base);

void JSON_free(JSON_entry* entry); // recursively free a JSON object

JSON_entry* JSON_deepCopy(JSON_entry* entry);

#endif
