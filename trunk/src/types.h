#ifndef __OVS_TYPES_H__
#define __OVS_TYPES_H__

// Types will be migrated here over time.


#define OVS_TIME unsigned long


typedef struct EnumString_struct {
    int id ;
    const char *str;
} EnumString;

typedef enum GridDirection_enum {
    GRID_ORDER_DEFAULT,
    GRID_ORDER_HORIZONTAL ,
    GRID_ORDER_VERTICAL
} GridDirection;

typedef struct array_str {
    void **array;
    int size;
    int mem_size;
    void (*free_fn)(void *);
} Array;

#define DB_MEDIA_TYPE_TV 1
#define DB_MEDIA_TYPE_FILM 2
#define DB_MEDIA_TYPE_ANY 3
#define DB_MEDIA_TYPE_OTHER 4



typedef struct ViewMode_struct {
    char *name;
    int view_class;
    int row_select; // How to select rows TV=by title_season tvboxset=by title  anything else by id;
    int has_playlist;
    char *dimension_cell_suffix; // get image dimensions from config file
    int media_type;
    int (*default_sort)(); // how to sort elements of this view
    //int (*default_sort)(DbItem **item1,DbItem **item2); // how to sort elements of this view
    int (*item_eq_fn)(void *,void *); // used to build hashtable of items
    unsigned int (*item_hash_fn)(void *); // used to build hashtable of items
} ViewMode;

#define ROW_BY_ID 0
#define ROW_BY_TITLE 1
#define ROW_BY_SEASON 2

#define VIEW_CLASS_MENU 0
#define VIEW_CLASS_ADMIN 1
#define VIEW_CLASS_BOXSET 2
#define VIEW_CLASS_DETAIL 3


typedef enum MovieBoxsetMode_enum {
    MOVIE_BOXSETS_UNSET , 
    MOVIE_BOXSETS_NONE ,
    MOVIE_BOXSETS_FIRST , // Box sets are related by first movie connection
    MOVIE_BOXSETS_LAST , // Box sets are related by last movie connection 
    MOVIE_BOXSETS_ANY    // Box sets are related by any movie connection
} MovieBoxsetMode;

typedef struct Dbrowid_struct {

    long id;
    struct Db_struct *db;
    long offset;
    OVS_TIME date;
    int watched;
    char *title;
    char *orig_title;
    char *poster;
    char *genre;
    char category;
    int season;

    // Add ext member etc but only populate if postermode=0 as the text mode has this extra detail
    char *file;
    char *ext;
    char *certificate;
    int year;
    int runtime;

    char *url;
    int external_id;

    /*
     * plot_key is derived from URL tt0000000
     * movie plot_key[MAIN]=_@tt0000000@@@_
     * tv plot_key[MAIN]=_@tt0000000@season@@_
     * tv plot_key[EPPLOT]=_@tt0000000@season@episode@_
     */
    char *plotkey[2];
    char *fanart;
    char *parts;
    char *episode;

    //only populate if deleting
    char *nfo;

    OVS_TIME airdate;
    OVS_TIME airdate_imdb;

    char *eptitle;
    char *eptitle_imdb;
    char *additional_nfo;
    double rating;

    OVS_TIME filetime;
    OVS_TIME downloadtime;

    //Only set if a row has a vodlink displayed and is added the the playlist
    Array *playlist_paths;
    Array *playlist_names;

    long plotoffset[2];
    char *plottext[2];

    // Set to 1 if item checked on HDD
    int delist_checked;

    // General flag
    int visited;

// warning TODO remove this link once lists are implemented
    struct Dbrowid_struct *linked;
    int link_count;

    struct DbGroupIMDB_struct *comes_after;
    struct DbGroupIMDB_struct *comes_before;
    struct DbGroupIMDB_struct *remakes;

    struct DbGroupIMDB_struct *directors;
    struct DbGroupIMDB_struct *actors;

    // Set for first row in the list.
    // These fields will be moved to the ItemList structure once I create a list of Items
    // for each Grid position.
   // char *drilldown_view_static;
    ViewMode *drilldown_view;

    // Holds hash value 
    unsigned int tmp_hash;

} DbItem;

// Expression 
typedef enum Op_enum {
    OP_VALUE=0,
    OP_ADD='+',
    OP_SUBTRACT='-',
    OP_MULTIPLY='*',
    OP_DIVIDE='/',
    OP_AND='&',
    OP_OR='O',
    OP_NOT='!',
    OP_EQ='=',
    OP_NE='~',
    OP_LE='{',
    OP_LT='<',
    OP_GT='>',
    OP_GE='}',
    OP_STARTS_WITH='^',
    OP_CONTAINS='#',
    OP_DBFIELD='_'
} Op;

typedef enum ValType_enum {
    VAL_TYPE_NUM='i',
    VAL_TYPE_STR='s'
} ValType;

typedef struct Value_struct {
    ValType type;
    double num_val;
    char *str_val;
    int free_str;

} Value;

typedef struct Exp_struct {

    Op op;
    struct Exp_struct *subexp[2];
    Value val;
} Exp;

#endif
