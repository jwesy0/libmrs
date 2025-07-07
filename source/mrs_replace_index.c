/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include <stdlib.h>

#include "mrs.h"
#include "mrs_internal.h"
#include "mrs_dbg.h"

extern int _mrs_replace_file(MRS* mrs, struct mrs_file_t* oldf, struct mrs_file_t* newf);

void _mrs_replace_index_list_dump(const struct mrs_replace_index_list_t* il){
    size_t i;
    for(i=0; i<il->cnt; i++){
        dbgprintf("%u: %u <-> %u", i, il->indices[i].old_index, il->indices[i].new_index);
    }
}

void _mrs_replace_index_list_init(struct mrs_replace_index_list_t* il){
    il->indices = NULL;
    il->cnt     = 0;
}

void _mrs_replace_index_list_add(struct mrs_replace_index_list_t* il, unsigned oldi, unsigned newi){
    size_t i = il->cnt;
    il->indices = (struct mrs_replace_index_t*)realloc(il->indices, (i+1)*sizeof(struct mrs_replace_index_t));
    il->indices[i].new_index = newi;
    il->indices[i].old_index = oldi;
    il->cnt++;
}

void _mrs_replace_index_list_remove(struct mrs_replace_index_list_t* il, unsigned index){
    if(index >= il->cnt)
        return;
    
    memset(&il->indices[index], 0, sizeof(struct mrs_replace_index_t));
    if(il->cnt == 1){
        free(il->indices);
        il->indices = NULL;
    }else{
        if(index < (il->cnt-1))
            memmove(&il->indices[index], &il->indices[index+1], (il->cnt - (index + 1))*sizeof(struct mrs_replace_index_t));
        il->indices = (struct mrs_replace_index_t*)realloc(il->indices, (il->cnt-1)*sizeof(struct mrs_replace_index_t));
    }
    
    il->cnt--;
}

int _mrs_replace_index_list_do_replace(struct mrs_replace_index_list_t* il, MRS* mrs, const struct mrs_file_t* f, size_t fcount, unsigned index){
    size_t i;
    
    if(index >= fcount)
        return 1; /// Invalid index of new file

    for(i=0; i<il->cnt; i++){
        if(index == il->indices[i].new_index){
            /// TODO: remove from replace index list
            if(il->indices[i].old_index >= mrs->_hdr.dir_count){
                _mrs_replace_index_list_remove(il, i);
                return 2; /// Invalid index of old file
            }
            dbgprintf("Found! Replacing %u with %u", il->indices[i].old_index, il->indices[i].new_index);
            _mrs_replace_file(mrs, &mrs->_files[il->indices[i].old_index], &f[index]);
            _mrs_replace_index_list_remove(il, i);
            return 0;
        }
    }

    return 3; /// Not found
}

void _mrs_replace_index_list_free(struct mrs_replace_index_list_t* il){
    free(il->indices);
}