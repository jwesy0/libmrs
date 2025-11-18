/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_DEFS_H_
#define __LIBMRS_DEFS_H_

#include <stdint.h>

/**
 * Indicates what action is being taken in progress callback function.
 */
typedef enum mrs_progress_t mrs_progress_t;
enum mrs_progress_t{
    /**< Begins to process the file, `param` is a `const char*` containing the file name. */
    MRSP_BEGIN = 1,
    /**< An error ocurred while processing the file, `param` is a `int` containing the error code. */
    MRSP_ERROR,
    /**< The file was processed successfully, `param` is a `const char*` containing the file name. */
    MRSP_END,
    /**< The progress is done, `param` is `NULL`. */
    MRSP_DONE
};

/**
 * Indicates what kind of item to add to a MRS handle.
 */
typedef enum mrs_add_t mrs_add_t;
enum mrs_add_t{
    /**< A file. */
    MRSA_FILE = 1,
    /**< Files from a folder. */
    MRSA_FOLDER,
    /**< Files from a MRS archive. */
    MRSA_MRS,
    /**< Files from a MRS* handle. */
    MRSA_MRS2,
    /**< File from a FILE pointer. */
    MRSA_FILEPTR,
    /**< File from a file descriptor. */
    MRSA_FILEDES,
    /**< File from memory buffer. */
    MRSA_MEMORY
};

/**
 * Indicates what to do when a duplicate is found.
 */
typedef enum mrs_dupe_behavior_t mrs_dupe_behavior_t;
enum mrs_dupe_behavior_t{
    /**< Keep new item. */
    MRSDB_KEEP_NEW = 0,
    /**< Keep old item. */
    MRSDB_KEEP_OLD,
    /**< Keep both new and old items. */
    MRSDB_KEEP_BOTH
};

/**
 * Indicates what info to get or set from a MRS handle.
 */
typedef enum mrs_file_info_t mrs_file_info_t;
enum mrs_file_info_t{
    /**< File name, returns a `char*`. */
    MRSFI_NAME = 1,
    /**< CRC32 checksum, returns a `uint32_t`. */
    MRSFI_CRC32,
    /**< File size, returns a `size_t`. */
    MRSFI_SIZE,
    /**< Compressed file size, returns a `size_t`. */
    MRSFI_CSIZE,
    /**< File modification time, returns a `time_t`. */
    MRSFI_TIME,
    /**< Local Header extra, returns a `unsigned char*`. */
    MRSFI_LHEXTRA,
    /**< Central Dir Header extra, returns a `unsigned char*`. */
    MRSFI_DHEXTRA,
    /**< Central Dir Header comment, returns a `unsigned char*`. */
    MRSFI_DHCOMMENT
};

/**
 * Indicates what we want to 'export' from our MRS handle.
 */
typedef enum mrs_save_t mrs_save_t;
enum mrs_save_t{
    /**< Creates a MRS file from our MRS handle. */
    MRSS_MRS = 1,
    /**< Creates a folder and put files from our MRS handle in it. */
    MRSS_FOLDER
};


typedef struct mrs_t MRS;

/**
 * Contains information of an archived file.
 */
typedef struct mrs_afile_t  mrs_afile_t;
struct mrs_afile_t{
    /**< File name. */
    const char* name;
    /**< Checksum. */
    uint32_t crc32;
    /**< Uncompressed file size. */
    size_t size;
    /**< Compressed file size. */
    size_t csize;
    /**< File modification time. */
    time_t ftime;
};
typedef mrs_afile_t         *MRSFILE;


/**
 * \brief Function for progress when compiling or decompiling a MRS archive.
 * \param progress Contains the progress between `0.f` and `1.f`, `0.f` being 0% and `1.f` being 100%.
 * \param index_item One-based index of the file being processed.
 * \param max_item Total number of files to be processed.
 * \param action Contains what action happened when this function was called.
 * \param param Depends on action, see `enum mrs_progress_t` values to know what `param` is set for each.
 */
typedef void (*MRS_PROGRESS_FUNC)(double progress, unsigned index_item,
                                  unsigned total_item, mrs_progress_t action,
                                  const void* param);

#endif