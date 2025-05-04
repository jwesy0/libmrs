/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_DEFS_H_
#define __LIBMRS_DEFS_H_

#include <stdint.h>

typedef struct mrs_t MRS;

/**
 * Indicates what action is being taken in progress callback function
 */
enum mrs_progress_t{
    MRSP_BEGIN = 1, /**< Begins to process the file, `param` is a `const char*` containing the file name */
    MRSP_ERROR,     /**< An error ocurred while processing the file, `param` is a `int` containing the error code */
    MRSP_END,       /**< The file was processed successfully, `param` is a `const char*` containing the file name */
    MRSP_DONE       /**< The progress is done, `param` is `NULL` */
};

/**
 * \brief Function for progress when compiling or decompiling a MRS archive.
 * \param progress Contains the progress between `0.f` and `1.f`, `0.f` being 0% and `1.f` being 100%
 * \param index_item One-based index of the file being processed
 * \param max_item Total number of files to be processed
 * \param action Contains what action happened when this function was called
 * \param param Depends on action, see `enum mrs_progress_t` values to know what `param` is set for each
 */
typedef void (*MRS_PROGRESS_FUNC)(double progress, unsigned index_item, unsigned total_item, enum mrs_progress_t action, const void* param);

/**
 * Indicates what kind of item to add to a MRS handle
 */
enum mrs_add_t{
    MRSA_FILE = 1,  /**< A file */
    MRSA_FOLDER,    /**< Files from a folder */
    MRSA_MRS        /**< Files from a MRS archive */
};

/**
 * Indicates what to do when a duplicate is found
 */
enum mrs_dupe_behavior_t{
    MRSDB_KEEP_NEW = 0, /**< Keep new item */
    MRSDB_KEEP_OLD,     /**< Keep old item */
    MRSDB_KEEP_BOTH     /**< Keep both new and old items */
};

/**
 * Indicates what info to get or set from a MRS handle
 */
enum mrs_file_info_t{
    MRSFI_NAME = 1,      /**< File name, returns a `char*` */
    MRSFI_CRC32,         /**< CRC32 checksum, returns a `uint32_t` */
    MRSFI_SIZE,          /**< File size, returns a `size_t` */
    MRSFI_CSIZE,         /**< Compressed file size, returns a `size_t` */
    MRSFI_TIME,          /**< File modification time, returns a `time_t` */
    MRSFI_LHEXTRA,       /**< Local Header extra, returns a `unsigned char*` */
    MRSFI_DHEXTRA,       /**< Central Dir Header extra, returns a `unsigned char*` */
    MRSFI_DHCOMMENT      /**< Central Dir Header comment, returns a `unsigned char*` */
};

/**
 * Indicates what we want to 'export' from our MRS handle
 */
enum mrs_save_t{
    MRSS_MRS = 1, /**< Creates a MRS file from our MRS handle */
    MRSS_FOLDER   /**< Creates a folder and put files from our MRS handle in it */
};

/**
 * Contains information of an archived file
 */
struct mrs_afile_t{
    const char* name; /**< File name */
    uint32_t crc32;   /**< Checksum */
    size_t size;      /**< Uncompressed file size */
    size_t csize;     /**< Compressed file size */
    time_t ftime;     /**< File modification time */
};
typedef struct mrs_afile_t* MRSFILE;

#endif