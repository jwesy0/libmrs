/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_MRS_H_
#define __LIBMRS_MRS_H_

#include "mrs_error.h"
#include "mrs_defs.h"
#include "mrs_encryption.h"

#include "windows.h"

#ifdef LIBMRS_DLL
#ifdef LIBMRS_BUILD
#define LIBMRS_DLLF __declspec( dllexport )
#else
#define LIBMRS_DLLF __declspec( dllimport )
#endif
#else
#define LIBMRS_DLLF
#endif

/**
 * \brief Initializes a new MRS handle
 * \return A valid MRS handle on success, if any error ocurrs, a `NULL` pointer is returned
 */
LIBMRS_DLLF MRS* mrs_init();

/**
 * \brief Set decryption functions for the current MRS handle
 * \param mrs Pointer to the MRS handle to apply the changes for
 * \param where Must be a combination of `enum mrs_encryption_where_t` values
 * \param f Decryption function to be applied to the fields indicated in `where`,
 *          if a `NULL` pointer is given, the default decryption function, `mrs_default_decrypt`, will be set
 * \return `MRSE_OK` on success, any different error code when an error occurs
 */
LIBMRS_DLLF int mrs_set_decryption(MRS* mrs, int where, MRS_ENCRYPTION_FUNC f);

/**
 * \brief Set encryption functions for the current MRS handle
 * \param mrs Pointer to the MRS handle to apply the changes for
 * \param where Must be a combination of `enum mrs_encryption_where_t` values
 * \param f Encryption function to be applied to the fields indicated in `where`,
 *          if a `NULL` pointer is given, the default encryption function, `mrs_default_encrypt`, will be set
 * \return `MRSE_OK` on success, any different error code when an error occurs
 */
LIBMRS_DLLF int mrs_set_encryption(MRS* mrs, int where, MRS_ENCRYPTION_FUNC f);

/**
 * \brief Add a new item (file, directory or mrs archive) to the the current MRS handle
 * \param mrs Pointer to the MRS handle to apply the changes for
 * \param what Indicates what kind of item to add to `mrs`
 * \param name Name of the item to add to `mrs`
 * \param final_name Name of the item after being added to `mrs`.
 *                   If `what` is `MRSA_FILE`, this parameter sets the final name of the file in the MRS handle, if a `NULL` pointer is given, the final name will be the same as the one given in `name`.
 *                   If `what` is `MRSA_FOLDER` or `MRSA_MRS`, this parameters sets the base folder name of the files in the MRS handle, if a `NULL` pointer is given, no base name will be set to the files.
 * \param on_dupe Indicates what to do when a duplicate is found
 * \return `MRSE_OK` on success, any different error code when an error occurs
 */
LIBMRS_DLLF int mrs_add(MRS* mrs, enum mrs_add_t what, const char* name, const char* final_name, enum mrs_dupe_behavior_t on_dupe);

LIBMRS_DLLF int mrs_set_signature(MRS* mrs, int where, uint32_t signature);

LIBMRS_DLLF int mrs_set_signature_check(MRS* mrs, MRS_SIGNATURE_FUNC f);

LIBMRS_DLLF int mrs_read(const MRS* mrs, unsigned index, unsigned char* buf, size_t buf_size, size_t* out_size);

LIBMRS_DLLF int mrs_write(MRS* mrs, unsigned index, const unsigned char* buf, size_t buf_size);

LIBMRS_DLLF int mrs_get_file_info(const MRS* mrs, unsigned index, enum mrs_file_info_t what, void* buf, size_t buf_size, size_t* out_size);

LIBMRS_DLLF int mrs_remove(MRS* mrs, unsigned index);

LIBMRS_DLLF int mrs_set_file_info(MRS* mrs, unsigned index, enum mrs_file_info_t what, const void* buf, size_t buf_size);

LIBMRS_DLLF int mrs_save(MRS* mrs, enum mrs_save_t type, const char* output, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_find_file(const MRS* mrs, const char* s, unsigned* index);

LIBMRS_DLLF size_t mrs_get_file_count(const MRS* mrs);

/**
 * \brief Frees a MRS handle
 * \param mrs Pointer to the MRS handle to be freed
 */
LIBMRS_DLLF void mrs_free(MRS* mrs);

LIBMRS_DLLF int mrs_global_compile(const char* name, const char* out_name, struct mrs_encryption_t* encryption, struct mrs_signature_t* sig, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_global_decompile(const char* name, const char* out_name, struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sig_check, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_global_list(const char* name, struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sig_check, MRSFILE* f);

LIBMRS_DLLF int mrs_global_list_next(MRSFILE f);

LIBMRS_DLLF int mrs_global_list_free(MRSFILE f);

#endif

/***************************************************************
Sending love to:
7hunna, [ARRIETA], [B]rother, [Brit]King, [Carton],
[ElectroPro], [i]nstinct, [M]arcos[V], _Arno_, _Tato_, Akibi,
Altern4tive, Alxxx, Amores, Anette, AngelGabriel, Anonymusss,
AppleCarrot, Ashar / [A]sunaChan, avatarmetal, BabaYaGa001,
BadKiller, BAZILIO, Bianca13, Blodreina, Bodiicia, Bonitaw,
Boudicca, broky, Broo_4_Men, BRZkomarov, BUCEETA, buceto,
Bunnycop / Grarmasin, BYONIC_1, Canavaro, Celeb, Chabaras,
Chelitooh, chiitar, chitar, ChronoKnight, CHURIKEI, Cirujamona,
Cisne, Cl0ud, Cliver, Cloud, cstdlib[h], Cynthia, Danielitha_1,
Declan, Dieguinhoo, Djanga, DoDiNa, domeshot, dpeme / RealAreum,
E800, EazyKorey, Elapse, Eshare, F91, FbAds / Tartaro,
FernAndaAk47, Fir3 / BYONIC / [KOM]Dasher, Flade, fuckbird,
FullBarGi, fuziooo, fuzioooo, Gabriel, Galak, gangjaaa,
Gashetas, GAYBOY, Gianpaths, Gutorm / SIRGUGU, HAHASOBAD,
Hatsune, Hierophant, Himeikyou, iBadBoy, Ic3DemoNLord,
iDevilKing, IGORVINIKS, ilyas, iMystic, iRukia, iSaynex,
Jink00k, JizonBanTab, Jomo, Julie, JUNOHAIR, Kaiserkayu,
KalifaGOGO / kalifali, KarinexD, Kaskao / ViZiOoO, Kawaizinha,
KaWaZaKy, KENSHIRO, Khoulling, Kimiifort, KimWooJun, KinG_KonG,
kingofa, Kizo, knight_124, Kota, KsNojenta / Jor, KwiQ,
LeeNaKyoung, LilUglyMane, llPoseidonll, lokololo (Argentina),
lokololo (Peru), lordking2, lSammyl, Lucaslima, LunaMoon,
Madaraxd, MAMAPIKA, MariaNa, Mariana_RD, matarindo155, MeduseR,
Ment, Milei, minnhat, mizaki, Mobbin, Monseur, Monu, moroccoo,
Mort, namuwikey, NAOPULA, Nathalia, Nickx, Ninjya_, NoBl00d,
Noga, NurseMikey, oppenheimer, Pablito0800, PandiBiker,
PapaiNoel, ParisParis, perlla, Pirocaland, PistolaoBR, Poon,
PowerGuidoo, Profane, ProNoob, PruvinFreind, PussyLicker,
qcosasno, QueenLuna, Rabbit999, RadioActive, Ragdoll,
Ranyita / Kowalski, RealKenn, ReaLRaeL, Redgie, Roman_Reigns,
Rubix_, Rukia, S3XY_R0S3, seraraye, sexipro, Shibanguinha,
SirMikey, sjongejonge, Snewygirl, Sopa[D]Moron, Sora2Hoshi,
Sr_Descolado, stigmata_, Stork013, Syquia / iYoona, TAANJIRO,
Tails, TaylorSwift, Team7Sakura, TEDROS, Tenzing, ThanatosSony,
THOyuki, Tidus, TMBlodyskz, Triwe, trytrytry, Uninspired,
Urbanse, VadeInFace, VeneCat, Vitor, wo_cao, wowwow, XdionerX,
xgunna, xLing, xswits / Sex6, Y0na, Yezb, Yulbert31, Zake,
zatel, ZecaPauGordn
***************************************************************/
