/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_ENCRYPTION_H_
#define __LIBMRS_ENCRYPTION_H_

#include <stdint.h>


/**
 * Function type for decryption/encryption routines.
 */
typedef void (*MRS_ENCRYPTION_FUNC)(unsigned char*, uint32_t);

/**
 * Function type for signature checking routines.
 * \returns Must return a value different from `0` if signature is valid.
 */
typedef int (*MRS_SIGNATURE_FUNC)(enum mrs_signature_where_t, uint32_t);

/**
 * Indicates where signature is at.
 */
typedef enum mrs_signature_where_t mrs_signature_where_t;
enum mrs_signature_where_t {
    MRSSW_BASE_HDR        = 0x01, /**< Base header signature. */
    MRSSW_LOCAL_HDR       = 0x02, /**< Local header signature. */
    MRSSW_CENTRAL_DIR_HDR = 0x04, /**< Central Dir header signature. */
};

/**
 * Indicates where encryption is at.
 */
typedef enum mrs_encryption_where_t mrs_encryption_where_t;
enum mrs_encryption_where_t{
    /**< Base header encryption. */
    MRSEW_BASE_HDR        = 1,
    /**< Local header encryption. */
    MRSEW_LOCAL_HDR       = 2,
    /**< Central Dir header encryption. */
    MRSEW_CENTRAL_DIR_HDR = 4,
    /**< All headers encryption (Base, Local, Central Dir). */
    MRSEW_HEADERS         = MRSEW_BASE_HDR | MRSEW_LOCAL_HDR | MRSEW_CENTRAL_DIR_HDR,
    /**< Compressed file buffer encryption. */
    MRSEW_BUFFER          = 8,
    /** Everywhere. */
    MRSEW_ALL             = MRSEW_HEADERS | MRSEW_BUFFER
};


/**
 * Contains 4 functions that can be used at different steps of encryption/decryption.
 */
typedef struct mrs_encryption_t mrs_encryption_t;
struct mrs_encryption_t{
    /**< Used at base header of a MRS file. */
    MRS_ENCRYPTION_FUNC base_hdr;
    /**< Used at local header of a MRS file. */
    MRS_ENCRYPTION_FUNC local_hdr;
    /**< Used at central dir header of a MRS file. */
    MRS_ENCRYPTION_FUNC central_dir_hdr;
    /**< Used at compressed file buffer in a MRS file. */
    MRS_ENCRYPTION_FUNC buffer;
};

/**
 * Signatures for different fields.
 */
typedef struct mrs_signature_t mrs_signature_t;
struct mrs_signature_t{
    /**< Base header signature. */ 
    uint32_t base_hdr; 
    /**< Local header signature. */
    uint32_t local_hdr;
    /**< Central Dir header signature. */
    uint32_t central_dir_hdr;
};


/**
 * \brief Default signature check function of MRS file headers.
 * \param where Where `signature` is at.
 * \param signature The value of the signature.
 */
int mrs_default_signatures(mrs_signature_where_t where, uint32_t signature);

/**
 * \brief Default MRS decryption function of Gunz.
 * \param buffer Pointer to the buffer to be decrypted.
 * \param size Size of buffer.
 */
void mrs_default_decrypt(unsigned char* buffer, uint32_t size);

/**
 * \brief Default MRS encryption function of Gunz.
 * \param buffer Pointer to the buffer to be encrypted.
 * \param size Size of buffer.
 */
void mrs_default_encrypt(unsigned char* buffer, uint32_t size);

#endif