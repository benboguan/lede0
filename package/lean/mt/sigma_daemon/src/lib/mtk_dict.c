#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "mtk_dict.h"
#include <ctype.h>
#define INITIAL_SIZE (1024)
#define GROWTH_FACTOR (2)
//#define MAX_LOAD_FACTOR (1)
#define MAX_LOAD_FACTOR (2)
/* dictionary initialization code used in both dict_create and grow */
static void str_lower(char* str)
{
    int i = 0;

    while(str[i]) {
        char c = tolower(str[i]);
        str[i] = c;
        i++;
    }
}


dict_t internal_dict_create(int size)
{
    dict_t d;
    int i;

    d = malloc(sizeof(*d));

    assert(d != 0);

    d->size = size;
    d->n = 0;
    d->table = malloc(sizeof(struct elt *) * d->size);

    assert(d->table != 0);

    for(i = 0; i < d->size; i++) d->table[i] = 0;

    return d;
}

dict_t dict_create(void)
{
    return internal_dict_create(INITIAL_SIZE);
}

void dict_destroy(dict_t d)
{
    int i;
    struct elt *e;
    struct elt *next;

    for(i = 0; i < d->size; i++) {
        for(e = d->table[i]; e != 0; e = next) {
            next = e->next;

            free(e->key);
            free(e->value);
            free(e);
        }
    }
    
    free(d->table);
    free(d);
    
    return;  
}

#define MULTIPLIER (1993)

static unsigned long
hash_function(const char *s)
{
    unsigned const char *us;
    unsigned long h;

    h = 0;

    for(us = (unsigned const char *) s; *us; us++) {
        h = h * MULTIPLIER + *us;
    }

    return h;
}

static void grow(dict_t d)
{
    dict_t d2;            /* new dictionary we'll create */
    struct dict swap;   /* temporary structure for brain transplant */
    int i;
    struct elt *e;

    d2 = internal_dict_create(d->size * GROWTH_FACTOR);

    for (i = 0; i < d->size; i++) {
        for (e = d->table[i]; e != 0; e = e->next) {
            /* note: this recopies everything */
            /* a more efficient implementation would
             * patch out the strdups inside dict_insert
             * to avoid this problem */
            dict_insert(d2, e->key, e->value);
        }
    }

    /* the hideous part */
    /* We'll swap the guts of d and d2 */
    /* then call dict_destroy on d2 */
    swap = *d;
    *d = *d2;
    *d2 = swap;

    dict_destroy(d2);
}

/* insert a new key-value pair into an existing dictionary */
void dict_insert(dict_t d, const char *key, const char *value)
{
    struct elt *e;
    unsigned long h;

    assert(key);
    assert(value);

    e = malloc(sizeof(*e));

    assert(e);

    e->key = strdup(key);
    e->value = strdup(value);

    h = hash_function(key) % d->size;

    e->next = d->table[h];
    d->table[h] = e;

    d->n++;

    /* grow table if there is not enough room */
    if (d->n >= d->size * MAX_LOAD_FACTOR) {
        grow(d);
    }
}

/* return the most recently inserted value associated with a key */
/* or 0 if no matching key is present */
const char * dict_search(dict_t d, const char *key)
{
    struct elt *e;

    if (key == NULL)
        return 0;

    for (e = d->table[hash_function(key) % d->size]; e != 0; e = e->next) {
        if(!strcmp(e->key, key)) {
            /* got it */
            return e->value;
        }
    }

    return 0;
}

/* delete the most recently inserted record with the given key */
/* if there is no such record, has no effect */
void dict_delete(dict_t d, const char *key)
{
    struct elt **prev;          /* what to change when elt is deleted */
    struct elt *e;              /* what to delete */

    for (prev = &(d->table[hash_function(key) % d->size]);
        *prev != 0; 
        prev = &((*prev)->next)) {
        if (!strcmp((*prev)->key, key)) {
            /* got it */
            e = *prev;
            *prev = e->next;

            free(e->key);
            free(e->value);
            free(e);

            return;
        }
    }
}
void dict_update(dict_t d, const char *key, const char *value) {
    // not exist,
    char* tmp_value; // need to add a '\n'

    if (key == NULL) {
        printf("key is NULL! Give up search!!!\n");
        return;
        }
    if (value == NULL) {
        printf("value is NULL! Give up search!!!\n");
        return;
        }
    tmp_value = malloc(strlen(value)+2); // need to add a '\n'
    memcpy(tmp_value, value, strlen(value));
    tmp_value[strlen(value)] = '\n';
    tmp_value[strlen(value)+1] = '\0';
    //printf("mod str with newline: %s", tmp_value);

    if (dict_search(d, key) != 0) {
        printf("replace pair ==> %s%s        with ==> %s%s\n", key, dict_search(d, key), key, value);
        dict_delete(d, key);
    } else {
        printf("adding new pair ==> %s%s\n", key, value);
    }

    dict_insert(d, key, tmp_value);
    free(tmp_value);
}

void dict_print(dict_t d)
{
    struct elt *e;
    int i;

    for (i = 0; i < d->size; i++) {
        for (e = d->table[i]; e != 0; e = e->next) {
            printf("(%s),(%s)\n", e->key, e->value);
        }
    }
}


const char * dict_search_lower(dict_t d, const char *key) {
    // not exist,
    // translate key
    char* lower_key = strdup(key);
    struct elt *e;

    str_lower(lower_key);

    for (e = d->table[hash_function(lower_key) % d->size]; e != 0; e = e->next) {
        if (!strcmp(e->key, lower_key)) {
            /* got it */
            return e->value;
        }
    }

    free(lower_key);

    return 0;
}

const char * table_search_lower(pstr_to_str_tbl_t tbl, const char *key) {
    char* lower_key = strdup(key);
    char* lower_table_key = strdup(tbl->str_key);
    pstr_to_str_tbl_t item;

    str_lower(lower_key);

    for (item = tbl; item->str_key[0] != 0; ++item) {
        str_lower(item->str_key);
        if (!strcmp(item->str_key, lower_key)) {
            /* got it */
            return item->str_val;
        }
    }

    free(lower_key);
    return 0;
}

const char * long_table_search_lower(pstr_to_str_long_tbl_t tbl, const char *key) {
    char* lower_key = strdup(key);
    char* lower_table_key = strdup(tbl->str_key);
    pstr_to_str_long_tbl_t item;

    str_lower(lower_key);

    for (item = tbl; item->str_key[0] != 0; ++item) {
        str_lower(item->str_key);
        if (!strcmp(item->str_key, lower_key)) {
            /* got it */
            return item->str_val;
        }
    }

    free(lower_key);
    return 0;
}

