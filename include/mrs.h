/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_MRS_H_
#define __LIBMRS_MRS_H_

#include <stdio.h>

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

LIBMRS_DLLF MRS* mrs_init();

LIBMRS_DLLF int mrs_set_decryption(MRS* mrs, int where, MRS_ENCRYPTION_FUNC f);

LIBMRS_DLLF int mrs_set_encryption(MRS* mrs, int where, MRS_ENCRYPTION_FUNC f);

/**
 * \brief Add an item, or items, to the `mrs` handle.
 * \param mrs      `MRS` handle to add items to.
 * \param what     What kind of item to add.
 * \param on_dupe  What action to take when an item with the same name is found in `mrs`.
 * \param reserved Reserved for future use.
 * \param ...      Determined by the `what` parameter, see below.
 * \note If `what` is `MRSA_FILE`, the variadic parameters are `const char* filename, const char* final_name`, where:
 * \note >>> `filename`   – Name of the file to be read from disk and added as an item.
 * \note >>> `final_name` – Name of the item once it is inside the `mrs` handle. This parameter is optional, if `NULL` is
 * given, the name of the item will be the same as the filename+extension part given in `filename`.
 * \note ––––––––––––––––
 * \note If `what` is `MRSA_FOLDER`, the variadic parameters are `const char* foldername, const char* base_name`, where:
 * \note >>> `foldername` – Name of the folder that contains the files to be added as items.
 * \note >>> `base_name`  – Root (or base) name of the items once they are added in the `mrs` handle, for instance, if
 * `base_name` is `"system"`, all items will be named `"system/..."` in the `mrs` handle. This parameter is optional, if
 * `NULL` is given, no root name will be prepended for the items names, and their names will be added in `mrs` as is.
 * \note ––––––––––––––––
 * \note If `what` is `MRSA_MRS`, the variadic parameters are `const char* mrsname, const char* base_name`, where:
 * \note >>> `mrsname`   – Name of the MRS archive containing the files to be added.
 * \note >>> `base_name` – Same behavior as `base_name` parameter from `MRSA_FOLDER` parameters.
 */
LIBMRS_DLLF int mrs_add(MRS* mrs, enum mrs_add_t what, enum mrs_dupe_behavior_t on_dupe, void* reserved, ...);

// LIBMRS_DLLF int mrs_add(MRS* mrs, enum mrs_add_t what, void* param1, void* param2, enum mrs_dupe_behavior_t on_dupe);

LIBMRS_DLLF int mrs_set_signature(MRS* mrs, int where, uint32_t signature);

LIBMRS_DLLF int mrs_set_signature_check(MRS* mrs, MRS_SIGNATURE_FUNC f);

LIBMRS_DLLF int mrs_read(const MRS* mrs, unsigned index, unsigned char* buf, size_t buf_size, size_t* out_size);

LIBMRS_DLLF int mrs_write(MRS* mrs, unsigned index, const unsigned char* buf, size_t buf_size);

LIBMRS_DLLF int mrs_get_file_info(const MRS* mrs, unsigned index, enum mrs_file_info_t what, void* buf, size_t buf_size, size_t* out_size);

LIBMRS_DLLF int mrs_remove(MRS* mrs, unsigned index);

LIBMRS_DLLF int mrs_set_file_info(MRS* mrs, unsigned index, enum mrs_file_info_t what, const void* buf, size_t buf_size);

LIBMRS_DLLF int mrs_save(MRS* mrs, enum mrs_save_t type, const char* output, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_save_mrs_fp(MRS* mrs, FILE* output, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_find_file(const MRS* mrs, const char* s, unsigned* index);

LIBMRS_DLLF size_t mrs_get_file_count(const MRS* mrs);

LIBMRS_DLLF void mrs_free(MRS* mrs);

LIBMRS_DLLF int mrs_global_compile(const char* name, const char* out_name, struct mrs_encryption_t* encryption, struct mrs_signature_t* sig, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_global_decompile(const char* name, const char* out_name, struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sig_check, MRS_PROGRESS_FUNC pcallback);

LIBMRS_DLLF int mrs_global_list(const char* name, struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sig_check, MRSFILE* f);

LIBMRS_DLLF int mrs_global_list_next(MRSFILE f);

LIBMRS_DLLF int mrs_global_list_free(MRSFILE f);

LIBMRS_DLLF const char* mrs_get_error_str(unsigned e);

#endif

/***************************************************************                 

7hunna       [ARRIETA]    [B]rother    [Brit]King   [Carton]
[i]nstinct   [M]arcos[V]  _Arno_       _Tato_       Akibi
Altern4tive  Alxxx        Amores       Anette       AngelGabriel
Anonymusss   AppleCarrot  [A]sunaChan  avatarmetal  BabaYaGa001
BadKiller    BAZILIO      Bianca13     Blodreina    Bodiicia
Bonitaw      Boudicca     broky        Broo_4_Men   BRZkomarov
BUCEETA      buceto       Bunnycop     BYONIC_1     Canavaro
Celeb        Chabaras     Chelitooh    chiitar      chitar
ChronoKnight CHURIKEI     Cirujamona   Cisne        Cl0ud
Cliver       Cloud        cstdlib[h]   Cynthia      Danielitha_1
Declan       Dieguinhoo   Djanga       DoDiNa       domeshot
dpeme        E800         EazyKorey    Elapse       Eshare
F91          FbAds        FernAndaAk47 Fir3         Flade
fuckbird     FullBarGi    fuziooo      fuzioooo     Gabriel
Galak        gangjaaa     Gashetas     GAYBOY       Gianpaths
Gutorm       HAHASOBAD    Hatsune      Hierophant   Himeikyou
iBadBoy      Ic3DemoNLord iDevilKing   IGORVINIKS   ilyas
iMystic      iRukia       iSaynex      J1NX_        Jink00k
JizonBanTab  Jomo         Julie        JUNOHAIR     Kaiserkayu
KalifaGOGO   KarinexD     Kawaizinha   KaWaZaKy     KENSHIRO
Khoulling    Kimiifort    KimWooJun    KinG_KonG    kingofa
Kizo         knight_124   Kota         KsNojenta    KwiQ
leaf3        LeeNaKyoung  LilUglyMane  llPoseidonll lokololo(AR)
lokololo(PE) lordking2    lSammyl      Lucaslima    LunaMoon
Madaraxd     MAMAPIKA     MariaNa      Mariana_RD   matarindo155
MeduseR      Ment         Milei        minnhat      mizaki
Mobbin       Monseur      Monu         moroccoo     Mort
namuwikey    NAOPULA      Nathalia     Nickx        Ninjya_
NoBl00d      Noga         NurseMikey   oppenheimer  Pablito0800
PandiBiker   PapaiNoel    ParisParis   perlla       Pirocaland
PistolaoBR   Poon         PowerGuidoo  Profane      ProNoob
PruvinFreind PussyLicker  qcosasno     QueenLuna    QuinnOP
Rabbit999    RadioActive  Ragdoll      RainGuns     Ranyita
RealKenn     ReaLRaeL     Redgie       Roman_Reigns Rubix_
Rukia        S3XY_R0S3    seraraye     sexipro      ShadowDemon1
Shibanguinha SintetikoO   SirMikey     sjongejonge  Snewygirl
Sopa[D]Moron Sora2Hoshi   Sr_Descolado stigmata_    Stork013
Syquia       TAANJIRO     Tails        TaylorSwift  Team7Sakura
TEDROS       Tenzing      ThanatosSony THOyuki      Tidus
TMBlodyskz   Triwe        trunda       trytrytry    Uninspired
Urbanse      VadeInFace   VeneCat      Vitor        ViZiOoO
wo_cao       wowwow       XdionerX     xgunna       xLing
xswits       Y0na         Yezb         Yulbert31    Zake
zatel        ZecaPauGordn

            thank you all for all the nights of fun!            

***************************************************************/
