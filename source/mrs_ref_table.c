/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrs_internal.h"
#include "mrs_dbg.h"

void _mrs_ref_table_init(struct mrs_ref_table_t* r) {
    r->refs  = NULL;
    r->count = 0;
}

#ifdef _LIBMRS_DBG
extern void _hex_dump(const unsigned char* buf, size_t size);
#else
#define _hex_dump(...)
#endif

unsigned char* _mrs_ref_table_append(struct mrs_ref_table_t* r, const unsigned char* s, size_t len) {
    unsigned i;
    struct mrs_ref_t* cur;

    if (!r || !s)
        return NULL;

    dbgprintf("Looking for: ");
    _hex_dump(s, len);
    
    for (i = 0; i < r->count; i++) {
        cur = &r->refs[i];
        if (cur->len == len && !memcmp(cur->mem, s, len)) {
            dbgprintf("Found! Incrementing ref");
            cur->ref++;
            return cur->mem;
        }
    }

    dbgprintf("Not found, adding a new byte stream to the ref table");

    r->refs = (struct mrs_ref_t*)realloc(r->refs, sizeof(struct mrs_ref_t) * (r->count + 1));
    cur = &r->refs[r->count];
    cur->ref = 1;
    cur->mem = (unsigned char*)malloc(len);
    memcpy(cur->mem, s, len);
    cur->len = len;
    r->count++;

    return cur->mem;
}

int _mrs_ref_table_free(struct mrs_ref_table_t* r, unsigned char* s) {
    unsigned i;
    struct mrs_ref_t* cur;

    if (!r || !s)
        return 1;

    for (i = 0; i < r->count; i++) {
        cur = &r->refs[i];

        if (cur->mem == s){
            if (cur->ref > 1) {
                dbgprintf("More than 1 reference to %p found, decrementing it...", cur->mem);
                cur->ref--;
                return 0;
            }
            dbgprintf("No more references to %p, freeing it", cur->mem);
            free(cur->mem);
            cur->mem = NULL;
            cur->ref = 0;
            cur->len = 0;
            if (i + 1 < r->count)
                memmove(&r->refs[i], &r->refs[i + 1], (r->count - i - 1) * sizeof(struct mrs_ref_t));

            if (r->count > 1) 
                r->refs = (struct mrs_ref_t*)realloc(r->refs, sizeof(struct mrs_ref_t) * (r->count - 1));
            else {
                free(r->refs);
                r->refs = NULL;
            }

            r->count--;
            return 0;
        }
    }

    return 1;
}

void _mrs_ref_table_free_all(struct mrs_ref_table_t* r) {
    unsigned i;
    struct mrs_ref_t* cur;

    for (i = 0; i < r->count; i++) {
        cur = &r->refs[i];
        free(cur->mem);
        dbgprintf("Freed %p, which had %u reference%s", cur->mem, cur->ref, cur->ref == 1 ? "" : "s");
        cur->mem = NULL;
        cur->ref = 0;
        cur->len = 0;
    }

    free(r->refs);
    r->refs  = NULL;
    r->count = 0;
}