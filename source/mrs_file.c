/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include <stdlib.h>

#include "mrs.h"
#include "mrs_dbg.h"
#include "mrs_internal.h"

void _mrs_file_init(struct mrs_file_t* f){
    memset(f, 0, sizeof(struct mrs_file_t));
}

void _mrs_file_free(struct mrs_file_t* f) {
    if (f->lh.filename != f->dh.filename) {
        dbgprintf("We got different filenames between LOCAL and CENTRAL DIR headers");
        free(f->lh.filename);
        dbgprintf("Freed LOCAL filename");
    }
    free(f->dh.filename);
    dbgprintf("Freed CENTRAL DIR filename");
    f->lh.filename = NULL;
    f->dh.filename = NULL;
    f->lh.extra    = NULL;
    f->dh.extra    = NULL;
    f->dh.comment  = NULL;
}

void _mrs_files_init(struct mrs_files_t* f){
    f->files = NULL;
    f->count = 0;
}

void _mrs_files_append(struct mrs_files_t* f, const struct mrs_file_t* ff){
    size_t i = f->count;

    f->files = (struct mrs_files_t*)realloc(f->files, sizeof(struct mrs_file_t) * (i + 1));
    f->count++;

    memcpy(&f->files[i], ff, sizeof(struct mrs_file_t));
}

void _mrs_files_destroy(struct mrs_files_t* f, int freefiles){
    size_t i;
    if (freefiles) {
        for (i = 0; i < f->count; i++) 
            _mrs_file_free(&f->files[i]);
    }
    free(f->files);
}