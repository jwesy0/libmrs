/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#if defined(__LIBMRS_INTERNAL__) && !defined(__LIBMRS_INTERNAL_H_)
#define __LIBMRS_INTERNAL_H_

#include <stdint.h>
#include <stdio.h>

#include "dostime.h"
#include "mrs_encryption.h"

/*******************************
    TEMPORARY STORAGE METHODS
*******************************/
    /**< Uses a temp file for temporary storage */
#define MRSMT_TEMPFILE 0
    /**< Uses memory for temporary storage */
#define MRSMT_MEMORY   1

/*******************************
    COMPRESSION METHODS
*******************************/
    /**< STORE compression method */
#define MRSCM_STORE   0
    /**< DEFLATE compression method */
#define MRSCM_DEFLATE 8

/*******************************
    SIGNATURES
*******************************/
    /**< Base Header default signature values */
#define MRSM_MAGIC1 0x5030207
#define MRSM_MAGIC2 0x5030208
#define MRSM_MAGIC3 0x6054b50

    /**< Local Header default signature values */
#define MRSM_LOCAL_MAGIC1 0x4034b50
#define MRSM_LOCAL_MAGIC2 0x85840000

    /**< Central Dir Header default signature values */
#define MRSM_CDIR_MAGIC1 0x2014b50
#define MRSM_CDIR_MAGIC2 0x5024b80

/*******************************
    VERSIONS
*******************************/
    /**< Local Header default version */
#define MRSV_LOCAL        0x14

    /**< Central Dir Header default versions */
#define MRSV_CDIR_MADE   0x19
#define MRSV_CDIR_NEEDED 0x14

/*******************************
    HEADERS DEFINITIONS
*******************************/
#pragma pack(2)
    /**< Base header */
struct mrs_hdr_t{
    uint32_t signature;
    uint16_t disk_num;
    uint16_t disk_start;
    uint16_t dir_count;
    uint16_t total_dir_count;
    uint32_t dir_size;
    uint32_t dir_offset;
    uint16_t comment_length;
};

    /**< Local Header */
struct mrs_local_hdr_t{
    uint32_t signature;
    uint16_t version;
    uint16_t flags;
    uint16_t compression;
    struct dostime_t filetime;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_length;
};

    /**< Central Dir Header */
struct mrs_central_dir_hdr_t{
    uint32_t signature;
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    struct dostime_t filetime;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_length;
    uint16_t comment_length;
    uint16_t disk_start;
    uint16_t int_attr;
    uint32_t ext_attr;
    uint32_t offset;
};

    /**< Extended Local header, containing filename and extra */
struct mrs_local_hdr_ex_t{
    struct mrs_local_hdr_t h;
    char* filename;
    char* extra;
};

    /**< Extended Central Dir header, containing filename, extra and comment */
struct mrs_central_dir_hdr_ex_t{
    struct mrs_central_dir_hdr_t h;
    char* filename;
    char* extra;
    char* comment;
};
#pragma pack()

/*******************************
    REFERENCE TABLE
*******************************/
    /**< Reference item */
struct mrs_ref_t {
    unsigned char* mem;
    size_t         len;
    unsigned       ref;
};
    /**< Reference table */
struct mrs_ref_table_t {
    struct mrs_ref_t* refs;
    size_t count;
};

/*******************************
    FILES
*******************************/
    /**< Internal struct mrs_afile_t */
struct mrs_afile_internal_t{
    const char* name; /**< File name */
    uint32_t crc32;   /**< Checksum */
    size_t size;      /**< Uncompressed file size */
    size_t csize;     /**< Compressed file size */
    time_t ftime;     /**< File modification time */
    unsigned char* obuf;
    unsigned char* buf;
    unsigned i;
    unsigned cnt;
    MRS_SIGNATURE_FUNC sig;
    void* ptr;
};

    /**< Internal structure to store files in a MRS handle */
struct mrs_file_t{
    struct mrs_central_dir_hdr_ex_t dh;
    struct mrs_local_hdr_ex_t lh;
};

/*******************************
    MRS HANDLE
*******************************/
    /**< The internal MRS definition */
struct mrs_t{
    struct mrs_encryption_t _dec;
    struct mrs_encryption_t _enc;
    MRS_SIGNATURE_FUNC      _sig;
    uint32_t _sigs[3]; /**< 0 being BASE HEADER, 1 being LOCAL HEADER, 2 being CENTRAL DIR HEADER */
    struct mrs_hdr_t _hdr;
    struct mrs_ref_table_t _reftable;
    struct mrs_file_t* _files;
    union{
        FILE* _fbuf;
        unsigned char* _mbuf;
    };
    int _mtype;
    size_t _mbuf_size;
    void* _ptr;
};

/*******************************
    DUPLICATE INDEX
*******************************/
    /**< Hold the index of the item to replace and the one to replace with */
struct mrs_replace_index_t{
    unsigned old_index;
    unsigned new_index;
};
    /**< List of `struct mrs_replace_index_t` */
struct mrs_replace_index_list_t{
    struct mrs_replace_index_t* indices;
    size_t cnt;
};

#endif