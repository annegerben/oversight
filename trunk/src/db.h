#ifndef __DB_ALORD__
#define __DB_ALORD__

#include "hashtable.h"
#include "util.h"
#include "vasprintf.h"

typedef struct Dbrowid_struct {

    long id;
    long offset;
    long date;
    int watched;
    char *title;
    char *poster;


} DbRowId;

#define DB_MEDIA_TYPE_TV 1
#define DB_MEDIA_TYPE_FILM 2
#define DB_MEDIA_TYPE_ANY 3

#define DB_WATCHED_FILTER_YES 1
#define DB_WATCHED_FILTER_NO 2
#define DB_WATCHED_FILTER_ANY 3

typedef struct Db_struct {

    char *path;
    char *source;

} Db;

typedef struct DbResults_struct {
    Db *db;
    struct hashtable *rows; // This is a hash of RowIds
} DbRows;


#define DB_FLDID_ID "_id"

#define DB_FLDID_WATCHED "_w"
#define DB_FLDID_ACTION "_a"
#define DB_FLDID_PARTS "_pt"
#define DB_FLDID_FILE "_F"
#define DB_FLDID_NAME "_N"
#define DB_FLDID_DIR "_D"


#define DB_FLDID_ORIG_TITLE "_ot"
#define DB_FLDID_TITLE "_T"
#define DB_FLDID_AKA "_K"

#define DB_FLDID_CATEGORY "_C"
#define DB_FLDID_ADDITIONAL_INFO "_ai"
#define DB_FLDID_YEAR "_Y"

#define DB_FLDID_SEASON "_s"
#define DB_FLDID_EPISODE "_e"
#define DB_FLDID_SEASON0 "0_s"
#define DB_FLDID_EPISODE0 "0_e"

#define DB_FLDID_GENRE "_G"
#define DB_FLDID_RATING "_r"
#define DB_FLDID_CERT "_R"
#define DB_FLDID_PLOT "_P"
#define DB_FLDID_URL "_U"
#define DB_FLDID_POSTER "_J"

#define DB_FLDID_DOWNLOADTIME "_DT"
#define DB_FLDID_INDEXTIME "_IT"
#define DB_FLDID_FILETIME "_FT"

#define DB_FLDID_SEARCH "_SRCH"
#define DB_FLDID_PROD "_p"
#define DB_FLDID_AIRDATE "_ad"
#define DB_FLDID_TVCOM "_tc"
#define DB_FLDID_EPTITLE "_et"
#define DB_FLDID_EPTITLEIMDB "_eti"
#define DB_FLDID_AIRDATEIMDB "_adi"
#define DB_FLDID_NFO "_nfo"


#endif
