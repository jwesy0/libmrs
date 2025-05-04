/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#include "mrs_encryption.h"

// Base header signatures
#define MRSM_MAGIC1 0x5030207
#define MRSM_MAGIC2 0x5030208
#define MRSM_MAGIC3 0x6054b50
// Local header signatures
#define MRSM_LOCAL_MAGIC1 0x4034b50
#define MRSM_LOCAL_MAGIC2 0x85840000
// Central Dir header signatures
#define MRSM_CDIR_MAGIC1 0x2014b50
#define MRSM_CDIR_MAGIC2 0x5024b80

int mrs_default_signatures(enum mrs_signature_where_t where, uint32_t signature) {
    switch (where) {
    case MRSSW_BASE_HDR:
        switch (signature) {
        case MRSM_MAGIC1:
        case MRSM_MAGIC2:
        case MRSM_MAGIC3:
            return 1;
        }
        return 0;
    case MRSSW_LOCAL_HDR:
        switch (signature) {
        case MRSM_LOCAL_MAGIC1:
        case MRSM_LOCAL_MAGIC2:
            return 1;
        }
        return 0;
    case MRSSW_CENTRAL_DIR_HDR:
        switch (signature) {
        case MRSM_CDIR_MAGIC1:
        case MRSM_CDIR_MAGIC2:
            return 1;
        }
        return 0;
    }

    return 0;
}

void mrs_default_decrypt(unsigned char* buffer, uint32_t size){
    unsigned i;
    unsigned char c;
    for(i=0; i<size; i++){
        c = buffer[i];
        c = (c>>3) | (c<<5);
        buffer[i] = ~c;
    }
}

void mrs_default_encrypt(unsigned char* buffer, uint32_t size){
    unsigned i;
    unsigned char c;
    for(i=0; i<size; i++){
        c = ~buffer[i];
        c = (c<<3) | (c>>5);
        buffer[i] = c;
    }
}