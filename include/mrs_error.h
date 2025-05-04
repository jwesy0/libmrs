/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_ERROR_H_
#define __LIBMRS_ERROR_H_

#define MRSE_OK                 0  /**< Everything ok */
#define MRSE_UNITIALIZED        1  /**< Unitialized MRS handle */
#define MRSE_INVALID_PARAM      2  /**< Invalid parameter */
#define MRSE_NOT_FOUND          3  /**< File/directory not found */
#define MRSE_CANNOT_OPEN        4  /**< Cannot open file/directory */
#define MRSE_INVALID_FILENAME   5  /**< Invalid file name */
#define MRSE_DUPLICATE          6  /**< There is a file with the same name */
#define MRSE_INVALID_INDEX      7  /**< Out of bound index */
#define MRSE_INSUFFICIENT_MEM   8  /**< Insufficient memory to read */
#define MRSE_EMPTY_FOLDER       9  /**< Empty folder */
#define MRSE_INVALID_MRS        10 /**< Invalid MRS file or invalid encryption */
#define MRSE_INVALID_ENCRYPTION 11 /**< Invalid MRS encryption */
#define MRSE_INFO_NOT_AVAILABLE 12 /**< Given info not available or set */
#define MRSE_CANNOT_SAVE        13 /**< Cannot save file or folder */
#define MRSE_EMPTY              14 /**< Empty MRS file */
#define MRSE_NO_MORE_FILES      15 /**< No more files */
#define MRSE_CANNOT_UNCOMPRESS  16 /**< Error while trying to uncompress file */
#define MRSE_END                17

const char* mrs_error_str[] = {
    "Ok",
    "Unitialized MRS handle.",
    "Invalid parameter.",
    "File not found.",
    "Cannot open file.",
    "Invalid output file name.",
    "Duplicate file found.",
    "Out of bound index.",
    "Insufficient memory.",
    "Empty folder.",
    "Invalid MRS file or invalid encryption.",
    "Invalid MRS encryption.",
    "Given info not available or set.",
    "Cannot save file.",
    "Empty MRS file.",
    "No more files.",
    "Cannot uncompress file."
};

#endif