/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include "mrs_internal.h"
#include "mrs_dbg.h"

void mrs_central_dir_hdr_dump(const struct mrs_central_dir_hdr_t* h){
    dbgprintf("Signature...: %08x", h->signature);
    dbgprintf("Ver made....: %#02x", h->version_made);
    dbgprintf("Ver need....: %#02x", h->version_needed);
    dbgprintf("Flags.......: %#04x", h->flags);
    dbgprintf("Compression.: %x", h->compression);
    dbgprintf("File time...: %#04x", h->filetime.time);
    dbgprintf("File date...: %#04x", h->filetime.date);
    dbgprintf("CRC32.......: %#08x", h->crc32);
    dbgprintf("C.size......: %u", h->compressed_size);
    dbgprintf("U.size......: %u", h->uncompressed_size);
    dbgprintf("Filename len: %u", h->filename_length);
    dbgprintf("Extra len...: %u", h->extra_length);
    dbgprintf("Comment len.: %u", h->comment_length);
    dbgprintf("Disk start..: %u", h->disk_start);
    dbgprintf("Int.attr....: %u", h->int_attr);
    dbgprintf("Ext.attr....: %u", h->ext_attr);
    dbgprintf("Offset......: %#08x", h->offset);
}

void mrs_central_dir_hdr_ex_dump(const struct mrs_central_dir_hdr_ex_t* h){
    mrs_central_dir_hdr_dump((struct mrs_central_dir_hdr_t*)h);
    dbgprintf("Filename....: %.*s {%p}", h->h.filename_length, h->filename, h->filename);
    dbgprintf("Extra.......: %.*s {%p}", h->h.extra_length, h->extra, h->extra);
    dbgprintf("Comment.....: %.*s {%p}", h->h.comment_length, h->comment, h->comment);
}

void mrs_local_hdr_dump(const struct mrs_local_hdr_t* h){
    dbgprintf("Signature...: %08x", h->signature);
    dbgprintf("Version.....: %#02x", h->version);
    dbgprintf("Flags.......: %#04x", h->flags);
    dbgprintf("Compression.: %#04x", h->compression);
    dbgprintf("File time...: %#04x", h->filetime.time);
    dbgprintf("File date...: %#04x", h->filetime.date);
    dbgprintf("CRC32.......: %#08x", h->crc32);
    dbgprintf("C.Size......: %u", h->compressed_size);
    dbgprintf("U.Size......: %u", h->uncompressed_size);
    dbgprintf("Filename len: %u", h->filename_length);
    dbgprintf("Extra len...: %u", h->extra_length);
}

void mrs_local_hdr_ex_dump(const struct mrs_local_hdr_ex_t* h){
    mrs_local_hdr_dump((struct mrs_local_hdr_t*)h);
    dbgprintf("Filename....: %.*s {%p}", h->h.filename_length, h->filename, h->filename);
    dbgprintf("Extra.......: %.*s {%p}", h->h.extra_length, h->extra, h->extra);
}

void mrs_file_dump(const struct mrs_file_t* f){
    mrs_central_dir_hdr_ex_dump(&f->dh);
    mrs_local_hdr_ex_dump(&f->lh);
}