#ifndef MTK_DUT_MTK_DICT_H
#define MTK_DUT_MTK_DICT_H

#include <stdio.h>
typedef struct dict *dict_t;

struct elt {
    struct elt *next;
    char *key;
    char *value;
};

struct dict {
    int size;           /* size of the pointer table */
    int n;              /* number of elements stored */
    struct elt **table;
};

typedef struct str_to_str_tbl
{
    char str_key[40];
    char str_val[70];
} str_to_str_tbl_t, *pstr_to_str_tbl_t;

typedef struct str_to_str_long_tbl
{
    char str_key[40];
    char str_val[150];
} str_to_str_long_tbl_t, *pstr_to_str_long_tbl_t;


/* create a new empty dictionary */
dict_t dict_create(void);
/* destroy a dictionary */
void dict_destroy(dict_t);
/* insert a new key-value pair into an existing dictionary */
void dict_insert(dict_t, const char *, const char *);
/* return the most recently inserted value associated with a key */
/* or 0 if no matching key is present */
const char *dict_search(dict_t, const char *);
/* delete the most recently inserted record with the given key */
/* if there is no such record, has no effect */
void dict_delete(dict_t, const char *);
/*
 * update dictionay, if the key exist, replace with new value, if not exist, insert for it.
 */
void dict_update(dict_t, const char *, const char *);
const char * dict_search_lower(dict_t, const char *);
// print content
void dict_print(dict_t);
const char * table_search_lower(str_to_str_tbl_t *tbl, const char *key);
const char * long_table_search_lower(str_to_str_long_tbl_t *tbl, const char *key);
#endif // MTK_DUT_MTK_DICT_H
