/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#if defined(__LIBMRS_INTERNAL__) && !defined(__LIBMRS_DBG_H_)
#ifdef _LIBMRS_DBG
#define dbgprintf(...) printf("%s: ", __FUNCTION__); \
                       printf(__VA_ARGS__); \
                       printf("\n")
#else
#define dbgprintf(...)
#endif
#endif