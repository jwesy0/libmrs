/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#if defined(__LIBMRS_INTERNAL__) && !defined(__LIBMRS_DBG_H_)
#ifdef _LIBMRS_DBG
#define dbgprintf(...) fprintf(stderr, "%s: ", __FUNCTION__); \
                       fprintf(stderr, __VA_ARGS__); \
                       fprintf(stderr, "\n")
#else
#define dbgprintf(...)
#endif
#endif