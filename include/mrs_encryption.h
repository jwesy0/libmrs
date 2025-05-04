/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_ENCRYPTION_H_
#define __LIBMRS_ENCRYPTION_H_

#include <stdint.h>

/**
 * Function type for decryption/encryption routines
 */
typedef void (*MRS_ENCRYPTION_FUNC)(unsigned char*, uint32_t);

/**
 * Indicates where signature is at
 */
enum mrs_signature_where_t {
    MRSSW_BASE_HDR = 1,   /**< Base header signature */
    MRSSW_LOCAL_HDR,      /**< Local header signature */
    MRSSW_CENTRAL_DIR_HDR /**< Central Dir header signature */
};

/**
 * Function type for signature checking routines
 * Must return a value different from 0 if signature was valid
 */
typedef int (*MRS_SIGNATURE_FUNC)(enum mrs_signature_where_t, uint32_t);

/**
 * Indicates where encryption is at
 */
enum mrs_encryption_where_t{
    MRSEW_BASE_HDR        = 1, /**< Base header encryption */
    MRSEW_LOCAL_HDR       = 2, /**< Local header encryption */
    MRSEW_CENTRAL_DIR_HDR = 4, /**< Central Dir header encryption */
    MRSEW_HEADERS         = MRSEW_BASE_HDR | MRSEW_LOCAL_HDR | MRSEW_CENTRAL_DIR_HDR, /**< All headers encryption (Base, Local, Central Dir) */
    MRSEW_BUFFER          = 8, /**< Compressed file buffer encryption */
    MRSEW_ALL             = MRSEW_HEADERS | MRSEW_BUFFER /** Everywhere */
};

/**
 * Contains 4 functions that can be used at different steps of encryption/decryption
 */
struct mrs_encryption_t{
    MRS_ENCRYPTION_FUNC base_hdr; /**< Used at base header of a MRS file */
    MRS_ENCRYPTION_FUNC local_hdr; /**< Used at local header of a MRS file */
    MRS_ENCRYPTION_FUNC central_dir_hdr; /**< Used at central dir header of a MRS file */
    MRS_ENCRYPTION_FUNC buffer; /**< Used at compressed file buffer in a MRS file */
};

struct mrs_signature_t{
    uint32_t base_hdr;        /**< Base header signature */ 
    uint32_t local_hdr;       /**< Local header signature */
    uint32_t central_dir_hdr; /**< Central Dir header signature */
};

/**
 * \brief Default signature check function of MRS file headers
 * \param where Where `signature` is at
 * \param signature The value of the signature
 */
int mrs_default_signatures(enum mrs_signature_where_t where, uint32_t signature);

/**
 * \brief Default MRS decryption function of Gunz
 * \param buffer Pointer to the buffer to be decrypted
 * \param size Size of buffer
 */
void mrs_default_decrypt(unsigned char* buffer, uint32_t size);

/**
 * \brief Default MRS encryption function of Gunz
 * \param buffer Pointer to the buffer to be encrypted
 * \param size Size of buffer
 */
void mrs_default_encrypt(unsigned char* buffer, uint32_t size);

#endif