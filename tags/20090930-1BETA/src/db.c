#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <regex.h>
#include <assert.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>

#include "db.h"
#include "actions.h"
#include "dbfield.h"
#include "gaya_cgi.h"
#include "oversight.h"
#include "hashtable_loop.h"
#include "network.h"
#include "mount.h"

#define DB_ROW_BUF_SIZE 4000
#define QUICKPARSE

static long read_and_parse_row_ticks=0;
static long assign_ticks=0;
static long inner_date_ticks=0;
static long date_ticks=0;
static long filter_ticks=0;
static long discard_ticks=0;
static long read_ticks=0;
static long keep_ticks=0;
char *copy_string(int len,char *s);
DbRowId *db_rowid_new(Db *db);
DbRowId *db_rowid_init(DbRowId *rowid,Db *db);
DbRowId *read_and_parse_row(
        DbRowId *rowid,
        Db *db,
        FILE *fp,
        int *eof,
        int tv_or_movie_view // true if looking at tv or moview view.
        );
int in_idlist(int id,int size,int *ids);
void get_genre_from_string(char *gstr,struct hashtable **h);
void get_first_two_letters(char *t);

#define UNSET -2
static int use_folder_titles = UNSET;

#define COPY_STRING(len,from) ((from)?memcpy(MALLOC((len)+1),(from),(len)+1):NULL)

char *copy_string(int len,char *s)
{
    char *p=NULL;
    if (s) {
        p = MALLOC(len+1);
        memcpy(p,s,len+1);
    }
    return p;
}

int db_lock_pid(Db *db)
{

    int lockpid=0;

    if (is_file(db->lockfile)) {

        FILE *fp = fopen(db->lockfile,"r");

        fscanf(fp,"%d\n",&lockpid);
        fclose(fp);
    }
    return lockpid;
}

int db_is_locked_by_another_process(Db *db)
{

    int result=0;
    int lockpid =  db_lock_pid(db) ;

    if ( lockpid != 0 && lockpid != getpid() ) {
        char *dir;
        ovs_asprintf(&dir,"/proc/%d",lockpid);
        if (is_dir(dir)) {
            HTML_LOG(1,"Database locked by pid=%d current pid=%d",lockpid,getpid());
            result=1;
        } else {
            HTML_LOG(1,"Database was locked by pid=%d current pid=%d : releasing lock",lockpid,getpid());
        }
        FREE(dir);
    }
    return result;
}

int db_lock(Db *db)
{

    int backoff[] = { 10,10,10,10,10,10,20,30, 0 };

    int attempt;

    db->locked_by_this_code=0;
    for(attempt = 0 ; backoff[attempt] && db->locked_by_this_code ==0 ; attempt++ ) {

        if (db_is_locked_by_another_process(db)) {

            sleep(backoff[attempt]);
            HTML_LOG(1,"Sleeping for %d\n",backoff[attempt]);

        } else {
            db->locked_by_this_code=1;
        }
    }
    if (db->locked_by_this_code) {
        FILE *fp = fopen(db->lockfile,"w");
        fprintf(fp,"%d\n",getpid());
        fclose(fp);
        HTML_LOG(1,"Aquired lock [%s]\n",db->lockfile);
    } else {
        html_error("Failed to get lock [%s]\n",db->lockfile);
    }
    return db->locked_by_this_code;
}

int db_unlock(Db *db)
{

    db->locked_by_this_code=0;
    HTML_LOG(1,"Released lock [%s]\n",db->lockfile);
    return unlink(db->lockfile) ==0;
}

/*
 * Load the database. Each database entry will just be an ID and a pointer to the DB file position
 * (see DbRow)
 */
Db *db_init(char *filename, // path to the file - if NULL compute from source
        char *source       // logical name or tag - local="*"
        )
{

    Db *db = MALLOC(sizeof(Db));

    if (filename == NULL) {
        db->path = get_mounted_path(source,"/share/Apps/oversight/index.db");
    } else {
        db->path =  STRDUP(filename);
    }
    db->source= STRDUP(source);

    ovs_asprintf(&(db->backup),"%s.old",db->path);

    db->lockfile = replace_all(db->path,"index.db","catalog.lck",0);

    db->locked_by_this_code=0;
    return db;
}

#define DB_SEP '\t'

// Search for <tab>field_id<tab>field<tab>
int field_pos(char *field_id,char *buffer,char **start,int *length,int quiet) {
    char *p;
    assert(field_id);
    assert(buffer);
    assert(start);

    int fid_len = strlen(field_id);

    //We can increment search by fid_len as the field names cant overlap due to tabs(DB_SEP).
    for (p = strstr(buffer,field_id) ; p != NULL ; p = strstr(p+fid_len,field_id) ) {
        if (p[-1] == DB_SEP && p[fid_len] == DB_SEP ) {
            *start=p+fid_len+1;
            p=strchr(*start,DB_SEP);
            assert(p);
            *length = p - *start;
            return 1;
        }
    }
    if (!quiet) HTML_LOG(1,"ERROR: Failed to find field [%s]",field_id);
    return 0;
}

int parse_date(char *field_id,char *buffer,OVS_TIME *val_ptr,int quiet)
{

    char term='\0';
    int y,m,d;

    if (!*buffer) {
        // blank is OK
        return 1;
    } else if (sscanf(buffer,"%4d-%d-%d%c",&y,&m,&d,&term) < 3) {
        if (!quiet) html_error("ERROR: failed to extract date field %s",field_id);
    } else if (term != '\t' && term != '\0') {
        if (!quiet) html_error("ERROR: bad terminator [%c=%d] after date field %s = %d %d",term,term,field_id,y,m,d);
    } else {
        struct tm t;
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = 0;
        t.tm_min = 0;
        t.tm_sec = 0;
        *val_ptr = time_ordinal(&t);
        if (*val_ptr < 0 ) {
            HTML_LOG(1,"bad date %d/%02d/%02d = %s",y,m,d,asctime(&t));
        }
        return 1;
    }
    return 0;
}



int parse_timestamp(char *field_id,char *buffer,OVS_TIME *val_ptr,int quiet)
{
    assert(val_ptr);
    assert(field_id);
    assert(buffer);
    int y,m,d,H,M,S;
    char term='\0';
    if (!*buffer) {
        // blank is OK
        return 1;
    } else if (sscanf(buffer,"%4d%2d%2d%2d%2d%2d%c",&y,&m,&d,&H,&M,&S,&term) < 6) {
        if (!quiet) HTML_LOG(1,"failed to extract timestamp field %s",field_id);
    } else if (term != '\t' && term != '\0') {
        if (!quiet) HTML_LOG(1,"ERROR: bad terminator [%c=%d] after timestamp field %s = %d %d %d %d %d %d",term,term,field_id,y,m,d,H,M,S);
    } else {
        struct tm t;
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = H;
        t.tm_min = M;
        t.tm_sec = S;
        inner_date_ticks -= clock();
        *val_ptr = time_ordinal(&t);
        inner_date_ticks += clock();
        if (*val_ptr < 0 ) {
            HTML_LOG(1,"ERROR: bad timstamp %d/%02d/%02d %02d:%02d:%02d = %s",y,m,d,H,M,S,asctime(&t));
        }
        //*val_ptr = S+60*(M+60*(H+24*(d+32*(m+12*(y-1970)))));
        return 1;
    }
    return 0;
}

#define FIELD_TYPE_NONE '-'
#define FIELD_TYPE_STR 's'
#define FIELD_TYPE_DOUBLE 'f'
#define FIELD_TYPE_CHAR 'c'
#define FIELD_TYPE_LONG 'l'
#define FIELD_TYPE_INT 'i'
#define FIELD_TYPE_DATE 'd'
#define FIELD_TYPE_TIMESTAMP 't'

int db_rowid_get_field_offset_type(DbRowId *rowid,char *name,void **offset,char *type,int *overview) {

    *offset=NULL;
    *type = FIELD_TYPE_NONE;
    *overview = 0;

    assert(name[0]=='_');

    switch(name[1]) {
        case 'a':
            if (name[2] == 'i' ) { // _ai
                *offset=&(rowid->additional_nfo);
                *type = FIELD_TYPE_STR;

            } else if (name[2] == 'd' ) { // _ad...

                if (name[3] == '\0') { // _ad

                    *offset=&(rowid->airdate);
                    *type = FIELD_TYPE_DATE;

                } else if (name[3] == 'i') { // _adi

                    *offset=&(rowid->airdate_imdb);
                    *type = FIELD_TYPE_DATE;

                }
            }
            break;
        case 'C':
            if (name[2] == '\0') { // _C
                *offset=&(rowid->category);
                *type = FIELD_TYPE_CHAR;
                *overview = 1;
            }
            break;
        case 'd':
            if (name[2] == '\0') { // _d
                *offset=&(rowid->director);
                *type = FIELD_TYPE_STR;
            }
            break;
        case 'D':
            if (name[2] == 'T' ) {
                *offset=&(rowid->downloadtime);
                *type = FIELD_TYPE_TIMESTAMP;
                *overview = 1;
            }
            break;
        case 'e':
            if (name[2] == '\0') { // _e
                *offset=&(rowid->episode);
                *type = FIELD_TYPE_STR;
            }else if (name[2] == 't') {
                if (name[3] == '\0') { // _et
                    *offset=&(rowid->eptitle);
                    *type = FIELD_TYPE_STR;
                } else if (name[3] == 'i') { // _eti
                    *offset=&(rowid->eptitle_imdb);
                    *type = FIELD_TYPE_STR;
                }
            }else if (name[2] == 'p') { // _ep
                *offset=&(rowid->episode_plot);
                *type = FIELD_TYPE_STR;
            }
            break;
        case 'f':
                if (name[2] == 'a') {
                    *offset=&(rowid->fanart);
                    *type = FIELD_TYPE_STR;
                }
            break;
        case 'F':
            if (name[2] == '\0') {
                *offset=&(rowid->file);
                *type = FIELD_TYPE_STR;
                *overview = 1;
            } else if (name[2] == 'T' ) {
                *offset=&(rowid->filetime);
                *type = FIELD_TYPE_TIMESTAMP;
                *overview = 1;
            }
            break;

        case 'G':
            if (name[2] == '\0') {
                *offset=&(rowid->genre);
                *type = FIELD_TYPE_STR;
                *overview = 1;
            }
            break;
        case 'J':
            if (name[2] == '\0') {
                *offset=&(rowid->poster);
                *type = FIELD_TYPE_STR;
            }
            break;
        case 'i':
            if (name[2] == 'd') {
                *offset=&(rowid->id);
                *type = FIELD_TYPE_LONG;
                *overview = 1;
            }
            break;
        case 'I':
            if (name[2] == 'T') {
                *offset=&(rowid->date);
                *type = FIELD_TYPE_TIMESTAMP;
                *overview = 1;
            }
            break;
        case 'n':
                if (name[2] == 'f') {
                    *offset=&(rowid->nfo);
                    *type = FIELD_TYPE_STR;
                }
            break;
        case 'o':
            // do nothing - _ot ORIGINAL_TITLE
            break;
        case 'p':
                if (name[2] == 't') {
                    *offset=&(rowid->parts);
                    *type = FIELD_TYPE_STR;
                }
            break;
        case 'P':
                if (name[2] == '\0') {
                *offset=&(rowid->plot);
                *type = FIELD_TYPE_STR;
                }
            break;
        case 'r':
            if (name[2] == '\0') {
                *offset=&(rowid->rating);
                *type = FIELD_TYPE_DOUBLE;
                *overview = 1;
            }
            break;
        case 'R':
            if (name[2] == '\0') {
                *offset=&(rowid->certificate);
                *type = FIELD_TYPE_STR;
                *overview = 1;
            }
            break;
        case 's':
            if (name[2] == '\0') {
                *offset=&(rowid->season);
                *type = FIELD_TYPE_INT;
                *overview = 1;
            }
            break;
        case 't':
            //do nothing - TVCOM
            break;
        case 'T':
            if (name[2] == '\0') {
                *offset=&(rowid->title);
                *type = FIELD_TYPE_STR;
                *overview = 1;
            }
            break;
        case 'U':
            if (name[2] == '\0') {
                *offset=&(rowid->url);
                *type = FIELD_TYPE_STR;
                *overview = 1;
            }
            break;
        case 'w':
            if (name[2] == '\0') {
                *offset=&(rowid->watched);
                *type = FIELD_TYPE_INT;
                *overview = 1;
            }
            break;
        case 'Y':
            if (name[2] == '\0') {
                *offset=&(rowid->year) ;
                *type = FIELD_TYPE_INT;
                *overview = 1;
            }
            break;
        default:
            HTML_LOG(-1,"Unknown field [%s]",name);
    }
    return *type != FIELD_TYPE_NONE;

}
// This will take ownership of the val - freeing it if necessary.
char * db_rowid_get_field(DbRowId *rowid,char *name) {

    char *result=NULL;
    void *offset;
    char type;
    int overview;

    if (!db_rowid_get_field_offset_type(rowid,name,&offset,&type,&overview)) {
        return NULL;
    }

    //HTML_LOG(0,"db_rowid_get_field of [%s] %d=%d?",rowid->title,I//);

    switch(type) {
        case FIELD_TYPE_STR:
            ovs_asprintf(&result,"%s",NVL(*(char **)offset));
            break;
        case FIELD_TYPE_CHAR:
            ovs_asprintf(&result,"%c",*(char *)(offset));
            break;
        case FIELD_TYPE_DOUBLE:
            ovs_asprintf(&result,"%.1lf",*(double *)offset);
            break;
        case FIELD_TYPE_INT:
            ovs_asprintf(&result,"%d",*(int *)offset);
            break;
        case FIELD_TYPE_LONG:
            ovs_asprintf(&result,"%ld",*(long *)offset);
            break;
        case FIELD_TYPE_DATE:
            ovs_asprintf(&result,"%s",fmt_date_static(*(OVS_TIME *)offset));
            break;
        case FIELD_TYPE_TIMESTAMP:
            ovs_asprintf(&result,"%s",fmt_timestamp_static(*(OVS_TIME *)offset));
            break;
        default:
            HTML_LOG(0,"Bad field type [%c]",type);
            assert(0);
    }
    return result;
}

// This will take ownership of the val - freeing it if necessary.
void db_rowid_set_field(DbRowId *rowid,char *name,char *val,int val_len,int tv_or_movie_view,int copy) {

    void *offset;
    char type;
    int overview;
    if (!db_rowid_get_field_offset_type(rowid,name,&offset,&type,&overview)) {
        return;
    }

    //Dont get the field if this is the menu view and it is not an overview field 
    if (!tv_or_movie_view && !overview) return;

    // Used to checl for trailing chars.
    char *tmps=NULL;
    int free_val=!copy; //if copying dont free

    assert(name[0]=='_');

    switch(type) {
        case FIELD_TYPE_STR:
            free_val=0;
            *(char **)offset = (copy?COPY_STRING(val_len,val):val);
            if (offset == &(rowid->file)) {
#if 0
                char *p = rowid->file + val_len;
                char *first = p - 6; // search backwards up to 6 chars for extension
                if (first < rowid->file ) first = rowid->file;
                while(p > first) {
                    if (*--p == '.') {
                        rowid->ext = p+1;
                        break;
                    }
                }
#else
                char *p = strrchr(rowid->file,'.');
                if (p) {
                    rowid->ext = p+1;
                }
#endif
            }
            break;
        case FIELD_TYPE_CHAR:
            *(char *)offset = *val;
            break;
        case FIELD_TYPE_INT:
            *(int *)offset=strtol(val,&tmps,10) ;
            break;
        case FIELD_TYPE_LONG:
            *(long *)offset=strtol(val,&tmps,10) ;
            break;
        case FIELD_TYPE_DOUBLE:
            sscanf(val,"%lf",(double *)offset);
            break;
        case FIELD_TYPE_DATE:
            parse_date(name,val,offset,0);
            break;
        case FIELD_TYPE_TIMESTAMP:
            parse_timestamp(name,val,offset,0);
            break;
        default:
            HTML_LOG(0,"Bad field type [%c]",type);
            assert(0);
    }
    if (free_val) FREE(val);
}

void set_title_as_folder(DbRowId *rowid)
{

    char *e=strrchr(rowid->file,'\0');
    char *s = NULL;
    int is_vob=0;

    if (e && e > rowid->file) {

        e--;
        s = e;

        if (e > rowid->file && *e == '/') {
            *e = '\0';
            s--;
            is_vob=1;
        }
        while(s > rowid->file && *s != '/') {
            s--;
        }
        if ( s >= rowid->file ) {
            HTML_LOG(1,"Title changed from [%s] to [%s]",rowid->title,s);
            FREE(rowid->title);
            rowid->title = STRDUP(s);
            if (is_vob) {
                *e='/';
            }
        }
    }
}

#define DB_NAME_BUF_SIZE 10
#define DB_VAL_BUF_SIZE 4000
#define RESET 1
#define RESET_AND_ADD 2

#define VAL_SZ_INIT 30
#define VAL_SZ_INC 50
#define STATE_START -1
#define STATE_NAME 0
#define STATE_VAR 1

void write_row(FILE *fp,DbRowId *rid) {
    fprintf(fp,"\t%s\t%ld",DB_FLDID_ID,rid->id);
    fprintf(fp,"\t%s\t%c",DB_FLDID_CATEGORY,rid->category);
    fprintf(fp,"\t%s\t%s",DB_FLDID_INDEXTIME,fmt_timestamp_static(rid->date));
    fprintf(fp,"\t%s\t%d",DB_FLDID_WATCHED,rid->watched);
    fprintf(fp,"\t%s\t%s",DB_FLDID_TITLE,rid->title);
    fprintf(fp,"\t%s\t%d",DB_FLDID_SEASON,rid->season);
    fprintf(fp,"\t%s\t%.1lf",DB_FLDID_RATING,rid->rating);
    fprintf(fp,"\t%s\t%s",DB_FLDID_EPISODE,rid->episode);
    fprintf(fp,"\t%s\t%s",DB_FLDID_POSTER,rid->poster);
    fprintf(fp,"\t%s\t%s",DB_FLDID_GENRE,rid->genre);
    fprintf(fp,"\t%s\t%s",DB_FLDID_PARTS,rid->parts);
    fprintf(fp,"\t%s\t%d",DB_FLDID_YEAR,rid->year);
    fprintf(fp,"\t%s\t%s",DB_FLDID_FILE,rid->file);
    fprintf(fp,"\t%s\t%s",DB_FLDID_ADDITIONAL_INFO,rid->additional_nfo);
    fprintf(fp,"\t%s\t%s",DB_FLDID_URL,rid->url);
    fprintf(fp,"\t%s\t%s",DB_FLDID_CERT,rid->certificate);
    fprintf(fp,"\t%s\t%s",DB_FLDID_FILETIME,fmt_timestamp_static(rid->filetime));
    fprintf(fp,"\t%s\t%s",DB_FLDID_DOWNLOADTIME,fmt_timestamp_static(rid->downloadtime));
    //fprintf(fp,"\t%s\t%s",DB_FLDID_PROD,rid->prod);
    fprintf(fp,"\t%s\t%s",DB_FLDID_AIRDATE,fmt_date_static(rid->airdate));
    fprintf(fp,"\t%s\t%s",DB_FLDID_EPTITLEIMDB,rid->eptitle_imdb);
    fprintf(fp,"\t%s\t%s",DB_FLDID_AIRDATEIMDB,fmt_date_static(rid->airdate_imdb));
    fprintf(fp,"\t%s\t%s",DB_FLDID_EPTITLE,rid->eptitle);
    fprintf(fp,"\t%s\t%s",DB_FLDID_NFO,rid->nfo);
    fprintf(fp,"\t%s\t%s",DB_FLDID_FANART,rid->fanart);
    fprintf(fp,"\t%s\t%s",DB_FLDID_PLOT,rid->plot);
    fprintf(fp,"\t\n");
    fflush(fp);
}

// TODO Change this back to static value and using copy_string() in db_rowid_set_field
DbRowId *read_and_parse_row(
        DbRowId *rowid,
        Db *db,
        FILE *fp,
        int *eof,
        int tv_or_movie_view // true if looking at tv or moview view.
        ) {

    db_rowid_init(rowid,db);

    char name[DB_NAME_BUF_SIZE];
    char *name_end = name+DB_NAME_BUF_SIZE;

    char value[DB_VAL_BUF_SIZE];
    char *value_end=value+DB_VAL_BUF_SIZE;

    register char next;

    //initially point p at the value buffer.
    //This initial value is discarded but it removes the need to 
    //check for p != NULL for every character iteration
    register char *p=NULL;
    char *end=NULL;

    int state=STATE_START;

    // Skip comment lines
    while ( (next = getc(fp) ) == '#' ) {
        if (fgets(value,DB_VAL_BUF_SIZE,fp) == NULL) break;
    }

    for(;;) {

        switch(next) {
        case EOF: goto eol; // Goto to avoid extra comparisons to break out of nested while/switch
        case '\n' : case '\r' : case '\0': // EOL terminators
            if (state == STATE_VAR) {
                *p = '\0';
                HTML_LOG(3,"parsed field %s=%s",name,value);
                db_rowid_set_field(rowid,name,value,p-value,tv_or_movie_view,1);
                state = STATE_START;
            }
            goto eol;
        case '\t':
            switch(state) {
                case STATE_START:

                    state=STATE_NAME;
                    p=name;
                    end=name_end;
                    break;

                case STATE_NAME:

                    //switch to STATE_VAR

                    *p = '\0';
                    state=STATE_VAR;
                    p=value;
                    end=value_end;
                    HTML_LOG(3,"name[%s]",name);
                    break;

                case STATE_VAR:
                    //switch to STATE_VAR if TAB is followed by _ (start of a name)
                    next = getc(fp);
                    if (next != '_' ) {
                        //If plot contains <tab> allow it if it is NOT followed by _
                        //which is the prefix for out html vars. This is a nasty hack due 
                        //to bad choice of field sep. TODO Make sure catalog.sh filters tabs out of plots.
                        ungetc(next,fp);
                    } else {
                        *p = '\0';
                        HTML_LOG(3,"val[%s]",value);
                        db_rowid_set_field(rowid,name,value,p-value,tv_or_movie_view,1);

                        state=STATE_NAME;
                        p=name;
                        end=name_end;
                        *p++='_';
                    }
                    break;
                default:
                    assert(0);
                    break;
            }
            break;
        default:
            // Add the character
            *p++ = next;
#if 1 //this should not really be commented out - could allow buffere overflow
            if (p >= end ) {
                // Name variable. This shouldnt happen - truncate
                p--;
                *p = '\0';
                break;
            }
#endif
        }
        next = getc(fp);
    }
eol:
    if (next == EOF) {
        *eof = 1;
    }
    if (p) {
        *p = '\0';
    }
    if (rowid->genre == NULL) {
        HTML_LOG(0,"no genre [%s][%s]",rowid->file,rowid->title);
    }
    if (use_folder_titles) {

        set_title_as_folder(rowid);
    }
    return rowid;
}

DbRowId *db_rowid_init(DbRowId *rowid,Db *db) {
    memset(rowid,0,sizeof(DbRowId));
    rowid->rating=0;

    rowid->db = db;
    rowid->season = -1;
    rowid->category='?';
    return rowid;
}
DbRowId *db_rowid_new(Db *db) {

    DbRowId *rowid = MALLOC(sizeof(*rowid));
    db_rowid_init(rowid,db);
    return rowid;
}

void db_rowid_dump(DbRowId *rid) {
    
    time_t t;
    HTML_LOG(1,"ROWID: id = %d",rid->id);
    HTML_LOG(1,"ROWID: watched = %d",rid->watched);
    HTML_LOG(1,"ROWID: title(%s)",rid->title);
    HTML_LOG(1,"ROWID: file(%s)",rid->file);
    HTML_LOG(1,"ROWID: ext(%s)",rid->ext);
    HTML_LOG(1,"ROWID: season(%d)",rid->season);
    HTML_LOG(1,"ROWID: episode(%s)",rid->episode);
    HTML_LOG(1,"ROWID: genre(%s)",rid->genre);
    HTML_LOG(1,"ROWID: ext(%c)",rid->category);
    HTML_LOG(1,"ROWID: parts(%s)",rid->parts);
    t = rid->date;
    HTML_LOG(1,"ROWID: date(%s)",asctime(localtime(&t)));
    HTML_LOG(1,"ROWID: eptitle(%s)",rid->eptitle);
    HTML_LOG(1,"ROWID: eptitle_imdb(%s)",rid->eptitle_imdb);
    HTML_LOG(1,"ROWID: additional_nfo(%s)",rid->additional_nfo);
    t = rid->airdate;
    HTML_LOG(1,"ROWID: airdate(%s)",asctime(localtime(&t)));
    t = rid->airdate_imdb;
    HTML_LOG(1,"ROWID: airdate_imdb(%s)",asctime(localtime(&t)));
    HTML_LOG(1,"----");
}


#define ALL_IDS -1
int parse_row(
        int num_ids, // number of ids passed in the idlist parameter of the query string. if ALL_IDS then id list is ignored.
        int *ids,    // sorted array of ids passed in query string idlist to use as a filter.
        int tv_or_movie_view, // true if looking at tv or moview view.
        char *buffer,  // The current buffer contaning a line of input from the database
        Db *db,        // the database
        DbRowId *rowid// current rowid structure to populate.
        ) {

    assert(db);
    assert(rowid);

    db_rowid_init(rowid,db);

    int result = 0;
    
    char *name_start = buffer;

    while(1) {

        char *name_end,*value_start,*value_end = NULL;

        //find start of name
        if (*name_start != '\t') {
            html_error("rowid %d: Tab expected before next field name",rowid->id);
            break;
        }

        name_start++;
        if (!*name_start || *name_start == 10 || *name_start == 13 ) {
            result = 1;
            break;
        }

        //find end of name00527SC 
        name_end=name_start;
        while(*name_end && *name_end != '\t') {
            name_end++;
        }
        if (*name_end != '\t') {
            HTML_LOG(-1,"rowid %d: Tab expected after next field name - got %c(%d) %c=%d",rowid->id,*name_start,*name_start,*name_end,*name_end);
            break;
        }
        *name_end = '\0';
        //HTML_LOG(-1,"fname[%s]",name_start);


        //find start of value
        value_end=value_start=name_end+1;
        while(*value_end) {
            if (*value_end == '\t') {
                // if the tab is followed by a field name or EOL then break.
                // This is added because some XML API return tabs. 
                // Really we should change separator to something else.
                switch(value_end[1]) {
                    case '_' : case '\n': case '\r' : case '\0' : 
                        goto got_value_end; //Yes it really is a goto
                }
            }
            value_end++;
        }
got_value_end:

        if (*value_end != '\t') {
            HTML_LOG(-1,"rowid %d: Tab expected after field value",rowid->id);
            break;
        }


        *value_end = '\0';

        int val_len=value_end-value_start;

        //HTML_LOG(-1,"fval[%s]",value_start);
        //
        //char *value_copy = MALLOC(val_len+1);
        //memcpy(value_copy,value_start,val_len+1);


        db_rowid_set_field(rowid,name_start,value_start,val_len,1,1);


        *name_end = *value_end = '\t';
        name_start = value_end;
    }
    // The folowing files are removed from the delete queue whenever they are parsed.
    delete_queue_unqueue(rowid,rowid->nfo);
    delete_queue_unqueue(rowid,rowid->poster);
    delete_queue_unqueue(rowid,rowid->fanart);

    result =   (result && (num_ids == ALL_IDS || in_idlist(rowid->id,num_ids,ids)) );
    if (!result) {
        db_rowid_free(rowid,0);
    }
    return result;
}

DbRowSet *db_rowset(Db *db) {
    assert(db);

    DbRowSet *dbrs = MALLOC(sizeof(DbRowSet));
    memset(dbrs,0,sizeof(DbRowSet));
    dbrs->db = db;
    return dbrs;
}


//Save the first row as the default row to edit.
static DbRowId *g_first_row=NULL;
DbRowId *get_first_row() {
    return g_first_row;
}


int db_rowset_add(DbRowSet *dbrs,DbRowId *id) {

    assert(id);
    assert(dbrs);
    assert(id->db == dbrs->db);

    if (dbrs->size >= dbrs->memsize) {
        dbrs->memsize += 50;
        dbrs->rows = REALLOC(dbrs->rows,dbrs->memsize * sizeof(DbRowId));
    }
    DbRowId *insert = dbrs->rows + dbrs->size;
    *insert = *id;
    (dbrs->size)++;

    switch(id->category) {
        case 'T': dbrs->episode_total++; break;
        case 'M': dbrs->movie_total++; break;
        default: dbrs->other_media_total++; break;
    }

    return dbrs->size;
}

char *db_get_field(char *fieldid)
{
    if (g_first_row == NULL) return NULL;
    return db_rowid_get_field(g_first_row,fieldid);
}


char *localDbPath() {
    static char *a=NULL;
    if (a == NULL) {
        ovs_asprintf(&a,"%s/index.db",appDir());
    }
    return a;
}

static int g_db_size = 0;

int db_full_size() {
    return g_db_size;
}

// Return 1 if db should be scanned accoring to html get parameters.
int db_to_be_scanned(char *name) {
    static char *idlist = NULL;
    if (idlist == NULL) {
        idlist = query_val("idlist");
    }
    if (*idlist) {
        //Look for "name(" in idlist
        char *p = idlist;
        char *q;
        int name_len = strlen(name);

        while((q=strstr(p,name)) != NULL) {
            if (q[name_len] == '(') {
                return 1;
            }
            p = q+name_len;
        }
        return 0;
    } else {
        // Empty idlist parameter - always scan
        return 1;
    }
}

void db_scan_and_add_rowset(char *path,char *name,char *name_filter,int media_type,int watched,
        int *rowset_count_ptr,DbRowSet ***row_set_ptr) {

    HTML_LOG(1,"begin db_scan_and_add_rowset");
    if (db_to_be_scanned(name)) {

        Db *db = db_init(path,name);

        if (db) {

            int this_db_size=0;
            DbRowSet *r = db_scan_titles(db,name_filter,media_type,watched,&this_db_size);

            if ( r != NULL ) {

                (*row_set_ptr) = REALLOC(*row_set_ptr,((*rowset_count_ptr)+2)*sizeof(DbRowSet*));
                (*row_set_ptr)[(*rowset_count_ptr)++] = r;
                (*row_set_ptr)[(*rowset_count_ptr)]=NULL;

                g_db_size += this_db_size;

            }
        }
    }
    HTML_LOG(0,"end db_scan_and_add_rowset %d",*rowset_count_ptr);
}

void clear_title_letter_count() {
    memset(g_title_letter_count,0,NUM_TITLE_LETTERS);
}

char *next_space(char *p) {
    char *space=p;

    while(*space) {
        if (*space == ' ') return space;
        if (*space == '\\' ) space++;
        space++;
    }
    return NULL;
}

char *is_nmt_network_share(char *mtab_line) {


    char *space=next_space(mtab_line);

    HTML_LOG(3,"mtab looking at[%s]",space);
    if (space) {
        if (util_starts_with(space+1,NETWORK_SHARE)) {
            HTML_LOG(0,"mtab nmt share[%s]",mtab_line);
           return space+1;
        }
    }
    HTML_LOG(2,"mtab ignore[%s]",mtab_line);
    return NULL;
}


// Extract ip from mtab and try to ping it - corrupts buffer.
int is_pingable(char *mtab_line) {

    char *ip = mtab_line;
    // 192.168.88.13:/space /opt/sybhttpd/localhost.drives/NETWORK_SHARE/space nfs ....
    // or
    // \134\134192.168.88.6\134share /opt/sybhttpd/localhost.drives/NETWORK_SHARE/abc cifs rw,mand,nodiratime,unc=\\192.168.88.6\share,


    // Skip octal format esp space (\040),tab (\011), newline (\012) and backslash (\134) 
    while (*ip == '\\' ) {
      ip +=  4;
    }
    char  *ipend = ip;
    while(strchr("\\/:",*ipend) == NULL) {
        ipend++;
    }
    *ipend='\0';
    int result = (ping(ip,0) == 0);
    return result;
}

//
// Returns null terminated array of rowsets
DbRowSet **db_crossview_scan_titles(
        int crossview,
        char *name_filter,  // only load lines whose titles match the filter
        int media_type,     // 1=TV 2=MOVIE 3=BOTH 
        int watched         // 1=watched 2=unwatched 3=any
        ){
    int rowset_count=0;
    DbRowSet **rowsets = NULL;

    clear_title_letter_count();

    if (use_folder_titles == UNSET ) {
        use_folder_titles = *oversight_val("ovs_use_folders_as_title") == '1';
    }

    HTML_LOG(1,"begin db_crossview_scan_titles");
    // Add information from the local database
    db_scan_and_add_rowset(
        localDbPath(),"*",
        name_filter,media_type,watched,
        &rowset_count,&rowsets);

    if (crossview) {
        //get iformation from any remote databases
        //
        //TODO there may be a long time out if trying to access an unmounted device.
        //Better to scan /etc/mtab for shares then do a quick ping first.
        //make m=ping timeout configurable.
        if(1) {
            FILE *fp = fopen("/etc/mtab","r");
            if (fp) {
#define MNT_BUF_SZ 999
                char mnt_buf[MNT_BUF_SZ];
                while(fgets(mnt_buf,MNT_BUF_SZ,fp)) {

                    char *mount_point = is_nmt_network_share(mnt_buf);
                    if (mount_point) {
                        if (is_pingable(mnt_buf)) {
                            //Terminate the mount point string
                            //Note could use * in formatting instead of modifying buffer.
                            char *space_b4_mount_type = next_space(mount_point);
                            if (space_b4_mount_type) {
                                char *path;
                                *space_b4_mount_type='\0';
                                char *name = mount_point + strlen(NETWORK_SHARE);

                                ovs_asprintf(&path,"%s/Apps/oversight/index.db",mount_point);
                                if (is_file(path)) {

                                    HTML_LOG(0,"crossview [%s]",path);
                                    db_scan_and_add_rowset(
                                        path,name,
                                        name_filter,media_type,watched,
                                        &rowset_count,&rowsets);

                                } else {
                                    HTML_LOG(0,"crossview search [%s] doesnt exist",path);
                                }
                                FREE(path);
                            }

                        }
                    }

                }
                fclose(fp);
            }
        } else {
            DIR *d = opendir(NETWORK_SHARE);
            if (d) {
                struct dirent dent,*p;
                while((readdir_r(d,&dent,&p)) == 0) {

                    char *path=NULL;
                    char *name=dent.d_name;

                    ovs_asprintf(&path,NETWORK_SHARE "%s/Apps/oversight/index.db",name);

                    if (is_file(path)) {

                        HTML_LOG(0,"crossview [%s]",path);
                        db_scan_and_add_rowset(
                            path,name,
                            name_filter,media_type,watched,
                            &rowset_count,&rowsets);

                    } else {
                        HTML_LOG(0,"crossview search [%s] doesnt exist",path);
                    }
                    FREE(path);
                }
                closedir(d);
            }
        }
    }
    //Save the first row as the default row to edit.
    if (g_first_row == NULL && rowsets && rowsets[0]->rows ) {
        g_first_row = rowsets[0]->rows;
        HTML_LOG(0,"First row=[%s]",g_first_row->title);
        HTML_LOG(0,"First row=[%s]",db_get_field("_T"));
        HTML_LOG(0,"First row=[%s]",db_rowid_get_field(g_first_row,"_T"));
        HTML_LOG(0,"First row=[%s]",g_first_row->title);
    }

    HTML_LOG(0,"end db_crossview_scan_titles");
    return rowsets;
}

void db_free_rowsets_and_dbs(DbRowSet **rowsets) {
    if (rowsets) {
        DbRowSet **r;
        for(r = rowsets ; *r ; r++ ) {
            Db *db = (*r)->db;
            db_rowset_free(*r);
            db_free(db);
        }
        FREE(rowsets);
    }
}

//integer compare function for sort.
int id_cmp_fn(const void *i,const void *j) {
    return (*(const int *)i) - (*(const int *)j);
}

// For a given db name - extract and sort the list of ids in the idlist query param
// eg idlist=dbname(id1|id2|id3)name2(id4|id5)...
// if there is no idlist then ALL ids are OK. to indicate this num_ids = -1
int *extract_idlist(char *db_name,int *num_ids) {

    char *query = query_val("idlist");
    int *result = NULL;

    HTML_LOG(1,"extract_idlist from [%s]",query);

    if (*query) {
        *num_ids = 0;
        char *p = delimited_substring(query,')',db_name,'(',1,0);
        if (p) {
            p += strlen(db_name)+1;
            char *q = strchr(p,')');
            if (q) {
                *q = '\0';
                Array *idstrings = split(p,"|",0);
                *q = ')';
                if (idstrings) {
                    result = malloc(idstrings->size * sizeof(int));
                    int i;
                    for(i = 0 ; i < idstrings->size ; i++ ) {
                        char ch;
                        result[i]=0;
                        sscanf(idstrings->array[i],"%d%c",result+i,&ch);
                    }
                    // Sort the ids.
                    qsort(result,idstrings->size,sizeof(int),id_cmp_fn);
                    *num_ids = idstrings->size;

                    array_free(idstrings);
                }
            }
        }

    } else {
        *num_ids = ALL_IDS;
    }

    if (*num_ids == ALL_IDS) {
        HTML_LOG(1,"db: name=%s searching all ids",db_name);
    } else {
        int i;
        for(i  = 0 ; i < *num_ids ; i++ ) {
            HTML_LOG(1,"db: name=%s id %d",db_name,result[i]);
        }
    }

    return result;
}

// quick binary chop to search list.
int in_idlist(int id,int size,int *ids) {

    if (size == 0) return 0;
    if (size == ALL_IDS) return 1;

    int min=0;
    int max=size;
    int mid;
    do {
        mid = (min+max) / 2;

        if (id < ids[mid] ) {

            max = mid;

        } else if (id > ids[mid] ) {

            min = mid + 1 ;

        } else {

            HTML_LOG(1,"found %d",ids[mid]);
            return 1;
        }
    } while (min < max);

    //HTML_LOG("not found %d",id);
    return 0;
}

#define FIRST_TITLE_LETTER(title) \
        (!title?\
            '\0'\
        :\
            (*(title) == 'T' && util_starts_with((title),"The ") ?\
                (title)[4]\
             :\
             *title)\
        )

DbRowSet * db_scan_titles(
        Db *db,
        char *name_filter,  // only load lines whose titles match the filter
        int media_type,     // 1=TV 2=MOVIE 3=BOTH 
        int watched,        // 1=watched 2=unwatched 3=any
        int *gross_size     // Full unfiltered size of database.
        ){

    regex_t pattern;
    DbRowSet *rowset = NULL;

    char *view=query_val("view");
    int tv_or_movie_view = (strcmp(view,"tv")==0 || strcmp(view,"movie") == 0);

    int num_ids;
    int *ids = extract_idlist(db->source,&num_ids);

    char *title_filter = query_val(QUERY_PARAM_TITLE_FILTER);

    HTML_LOG(3,"Creating db scan pattern..");

    if (name_filter && *name_filter) {
        // Take the pattern and turn it into <TAB>_ID<TAB>pattern<TAB>
        int status;

        if ((status = regcomp(&pattern,name_filter,REG_EXTENDED|REG_ICASE)) != 0) {

#define BUFSIZE 256
            char buf[BUFSIZE];
            regerror(status,&pattern,buf,BUFSIZE);
            fprintf(stderr,"%s\n",buf);
            assert(1);
            return NULL;
        }
        HTML_LOG(1,"filering by regex [%s]",name_filter);
    }

    char *watched_substring=NULL;
    switch(watched) {
        case DB_WATCHED_FILTER_NO : watched_substring = "\t" DB_FLDID_WATCHED "\t0\t"; break;
        case DB_WATCHED_FILTER_YES : watched_substring = "\t" DB_FLDID_WATCHED "\t1\t"; break;
        case DB_WATCHED_FILTER_ANY : watched_substring = NULL ; break;
        default: assert(watched == DB_WATCHED_FILTER_NO); break;
    }

    char *media_substring=NULL;
    switch(media_type) {
        case DB_MEDIA_TYPE_TV : media_substring = "\t" DB_FLDID_CATEGORY "\tT\t"; break;
        case DB_MEDIA_TYPE_FILM : media_substring = "\t" DB_FLDID_CATEGORY "\tM\t"; break;
        case DB_MEDIA_TYPE_ANY : media_substring = NULL ; break;
        default: assert(watched == DB_MEDIA_TYPE_TV); break;
    }

    char *genre_filter = query_val(DB_FLDID_GENRE);


    int row_count=0;
    int total_rows=0;

    HTML_LOG(3,"db scanning %s",db->path);

    FILE *fp = fopen(db->path,"r");
    HTML_LOG(3,"db scanning %s",db->path);
    //assert(fp);
HTML_LOG(3,"db fp.%ld..",(long)fp);
    if (fp) {
        rowset=db_rowset(db);

        int eof=0;
        DbRowId rowid;
        db_rowid_init(&rowid,db);

        unsigned char title_filter_start='\0';
        unsigned char title_filter_end=255;
        if (title_filter && *title_filter) {
            title_filter_start=*(unsigned char *)title_filter;
            title_filter_end=*(unsigned char *)(title_filter+strlen(title_filter)-1);
            HTML_LOG(0,"Title Filter [%c-%c]",title_filter_start,title_filter_end);
        }



        while (eof == 0) {
            total_rows++;
            read_and_parse_row_ticks -= clock();
            read_and_parse_row(&rowid,db,fp,&eof,tv_or_movie_view);
            read_and_parse_row_ticks += clock();
            filter_ticks -= clock();

            if (rowid.file) {
                int keeprow=1;

                // If there were any  deletes queued for shared resources, revoke them
                // as they are in use by this item.
                delete_queue_unqueue(&rowid,rowid.nfo);
                delete_queue_unqueue(&rowid,rowid.poster);
                delete_queue_unqueue(&rowid,rowid.fanart);

                // Check the device is mounted - if not skip it.
                if (!nmt_mount_quick(rowid.file)){

                    HTML_LOG(1,"Path not mounted [%s]",rowid.file);
                    keeprow=0;

                } else {

                    switch(rowid.category) {
                        case 'T': g_episode_total++; break;
                        case 'M': g_movie_total++; break;
                        default: g_other_media_total++; break;
                    }


                    if (rowid.genre) {
                        get_genre_from_string(rowid.genre,&g_genre_hash);
                    }

                    //get_first_two_letters(rowid.title);

                    unsigned char first_letter = FIRST_TITLE_LETTER(rowid.title);
                    g_title_letter_count[toupper(first_letter)]++;

                    if (first_letter) {
                        if (title_filter_start == '0' ) {
                            // Non alpahbetic
                            if (first_letter >= 'A' && first_letter <= 'Z') {
                                keeprow = 0;
                            }
                        } else if (first_letter < title_filter_start || first_letter > title_filter_end) {
                            keeprow = 0;
                        }
                    }

                    if (genre_filter && *genre_filter) {
                        if (!strstr(rowid.genre,genre_filter)) {
                            HTML_LOG(3,"Rejected [%s] as [%s] not in [%s]",rowid.title,genre_filter,rowid.genre);
                            keeprow=0;
                        }
                    }

                    if (keeprow && name_filter && *name_filter) {
                        int match= regexec(&pattern,rowid.title,0,NULL,0);
                        if (match != 0 ) {
                            HTML_LOG(5,"skipped %s!=%s",rowid.title,name_filter);
                            keeprow=0;
                        }
                    }
                    if (keeprow) {
                        switch(media_type) {
                            case DB_MEDIA_TYPE_TV : if (rowid.category != 'T') keeprow=0; ; break;
                            case DB_MEDIA_TYPE_FILM : if (rowid.category != 'M') keeprow=0; ; break;
                        }
                    }

                    if (keeprow) {
                        switch(watched) {
                            case DB_WATCHED_FILTER_NO : if (rowid.watched != 0 ) keeprow=0 ; break;
                            case DB_WATCHED_FILTER_YES : if (rowid.watched != 1 ) keeprow=0 ; break;
                        }
                    }
                    if (keeprow) {
                        if (num_ids != ALL_IDS && !in_idlist(rowid.id,num_ids,ids)) {
                            keeprow = 0;
                        }
                    }
                }

                if (keeprow) {
                    keep_ticks -= clock();
                    row_count = db_rowset_add(rowset,&rowid);
                    keep_ticks += clock();
                } else {
                    discard_ticks -= clock();
                    db_rowid_free(&rowid,0);
                    discard_ticks += clock();
                }

                if (gross_size != NULL) {
                    (*gross_size)++;
                }
            }
            filter_ticks += clock();
        }
        HTML_LOG(0,"read_and_parse_row_ticks %d",read_and_parse_row_ticks/1000);
        HTML_LOG(0,"inner_date_ticks %d",inner_date_ticks/1000);
        HTML_LOG(0,"date_ticks %d",date_ticks/1000);
        HTML_LOG(0,"assign_ticks %d",assign_ticks/1000);
        HTML_LOG(0,"filter_ticks %d",filter_ticks/1000);
        HTML_LOG(0,"keep_ticks %d",keep_ticks/1000);
        HTML_LOG(0,"discard_ticks %d",discard_ticks/1000);
        HTML_LOG(0,"read_ticks %d",read_ticks/1000);

        HTML_LOG(0,"First total %d",total_rows);
        fclose(fp);
    }
    if (rowset) {
        HTML_LOG(1,"db[%s] filtered %d of %d rows",db->source,row_count,total_rows);
    } else {
        HTML_LOG(1,"db[%s] No rows loaded",db->source);
    }
    FREE(ids);
    HTML_LOG(1,"return rowset");
    return rowset;
}

void db_free(Db *db) {


    if (db->locked_by_this_code) {
        db_unlock(db);
    }

    FREE(db->source);
    FREE(db->path);
    FREE(db->lockfile);
    FREE(db->backup);
    FREE(db);

}


void db_rowid_free(DbRowId *rid,int free_base) {

    assert(rid);

//    HTML_LOG(0,"%s %lu %lu",rid->title,rid,rid->file);
//    HTML_LOG(0,"%s",rid->file);
    FREE(rid->title);
    FREE(rid->poster);
    FREE(rid->genre);
    FREE(rid->file);
    FREE(rid->episode);
    //Dont free ext as it points to file.


    // Following are only set in tv/movie view
    FREE(rid->url);
    FREE(rid->parts);
    FREE(rid->fanart);
    FREE(rid->plot);
    FREE(rid->eptitle);
    FREE(rid->eptitle_imdb);
    FREE(rid->additional_nfo);

    ARRAY_FREE(rid->playlist_paths);
    ARRAY_FREE(rid->playlist_names);

    //Only populated if deleting
    FREE(rid->nfo);

    FREE(rid->certificate);
    if (free_base) {
        FREE(rid);
    }
}

void db_rowset_free(DbRowSet *dbrs) {
    int i;

    for(i = 0 ; i<dbrs->size ; i++ ) {
        DbRowId *rid = dbrs->rows + i;
        db_rowid_free(rid,0);
    }

    FREE(dbrs->rows);
    FREE(dbrs);
}


void db_rowset_dump(int level,char *label,DbRowSet *dbrs) {
    int i;
    if (dbrs->size == 0) {
        HTML_LOG(level,"Rowset: %s : EMPTY",label);
    } else {
        for(i = 0 ; i<dbrs->size ; i++ ) {
            DbRowId *rid = dbrs->rows + i;
            HTML_LOG(level,"Rowset: %s [%s - %c - %d %s ]",label,rid->title,rid->category,rid->season,rid->poster);
        }
    }
}

void db_set_fields_by_source(
        char *field_id,char *new_value,char *source,char *idlist,int delete_mode) {

HTML_LOG(1," begin db init");

    Db *db = db_init(NULL,source);

    int new_len = 0;
    
    if (new_value != NULL) {
        new_len = strlen(new_value);
    }

    char *id_regex_text;
    regex_t id_regex_ptn;

    char *regex_text;
    regex_t regex_ptn;

    char buf[DB_ROW_BUF_SIZE+1];

    regmatch_t pmatch[5];

HTML_LOG(1," begin db_set_fields_by_source ids %s(%s) %s=%s ",source,idlist,field_id,new_value);

    ovs_asprintf(&regex_text,"\t%s\t([^\t]+)\t",field_id);
    util_regcomp(&regex_ptn,regex_text,0);
    HTML_LOG(1,"regex filter [%s]",regex_text);

    ovs_asprintf(&id_regex_text,"\t%s\t(%s)\t",DB_FLDID_ID,idlist);
    util_regcomp(&id_regex_ptn,id_regex_text,0);
    HTML_LOG(1,"regex extract [%s]",id_regex_text);



    if (db && db_lock(db)) {
HTML_LOG(1," begin open db");
        FILE *db_in = fopen(db->path,"r");

        if (db_in) {
            char *tmpdb;
            ovs_asprintf(&tmpdb,"/share/Apps/oversight/index.db.tmp.%d",getpid());

            FILE *db_out = fopen(tmpdb,"w");

            if (db_out) {
                while(1) {
                    
                    buf[DB_ROW_BUF_SIZE]='\0';

                    if (fgets(buf,DB_ROW_BUF_SIZE,db_in) == NULL) {
                        break;
                    }

                    assert( buf[DB_ROW_BUF_SIZE] == '\0' );



                    if (regexec(&id_regex_ptn,buf,0,NULL,0) != 0 ) {

                        // No match - emit
                        fprintf(db_out,"%s",buf);

                    } else if (delete_mode == DELETE_MODE_REMOVE) {
                            // do nothing. line is not written 
                    } else if (delete_mode == DELETE_MODE_DELETE) {
                        DbRowId rid;
                        parse_row(ALL_IDS,NULL,0,buf,db,&rid);
                        delete_media(&rid,1);
                        db_rowid_free(&rid,0);

                    } else if ( regexec(&regex_ptn,buf,2,pmatch,0) == 0) {
                        // Field is present - change it
                        int spos=pmatch[1].rm_so;
                        int epos=pmatch[1].rm_eo;

HTML_LOG(1," got regexec %s %s from %d to %d ",id_regex_text,regex_text,spos,epos);

                        buf[spos]='\0';

                        fprintf(db_out,"%s%s%s",buf,new_value,buf+epos);

                    } else {
                        // field not present - we could append the field at this stage.
                        // but there is not calling function that adds fields on the fly!
                        // so just emit
                        fprintf(db_out,"%s",buf);

                    }

                }
                fclose(db_out);
            }

            fclose(db_in);
            util_rename(db->path,db->backup);
            util_rename(tmpdb,db->path);
            FREE(tmpdb);
        }
        db_unlock(db);
        db_free(db);
    }
HTML_LOG(1," end db_set_fields_by_source");
    regfree(&regex_ptn);
    regfree(&id_regex_ptn);
    FREE(regex_text);
    FREE(id_regex_text);

}

void db_remove_row(DbRowId *rid) {
    char idlist[20];
    sprintf(idlist,"%ld",rid->id);
    db_set_fields_by_source(DB_FLDID_ID,NULL,rid->db->source,idlist,DELETE_MODE_REMOVE);
}
void db_delete_row_and_media(DbRowId *rid) {
    char idlist[20];
    sprintf(idlist,"%ld",rid->id);
    db_set_fields_by_source(DB_FLDID_ID,NULL,rid->db->source,idlist,DELETE_MODE_DELETE);
}

void db_set_fields(char *field_id,char *new_value,struct hashtable *ids_by_source,int delete_mode) {
    struct hashtable_itr *itr;
    char *source;
    char *idlist;

HTML_LOG(1," begin db_set_fields");
    for(itr=hashtable_loop_init(ids_by_source) ; hashtable_loop_more(itr,&source,&idlist) ; ) {
        db_set_fields_by_source(field_id,new_value,source,idlist,delete_mode);
    }
HTML_LOG(1," end db_set_fields");
}

void get_genre_from_string(char *gstr,struct hashtable **h) {

    char *p;

    if (*h == NULL) {
        *h = string_string_hashtable(16);
    }

    for(;;) {
        //while (*gstr == ' ' ) gstr++; // eat space

        while (*gstr && strchr("|, ",*gstr) ) { gstr++; } // eat sep

        //while (*gstr == ' ' ) gstr++; // eat space - ltrim

        if (!*gstr) {
            break;
        }

        p = gstr;
        while ( *p && strchr("|, ",*p) == NULL) { p++; } // find end sep

        //while ( p > gstr && p[-1] == ' ' ) p--; // rtrim

        if (*gstr && p > gstr) {
            char save_c = *p;
            *p = '\0';

            // Exclude 'and' and 'Show' from genres
            if (strcmp(gstr,"and") != 0 && strcmp(gstr,"Show") != 0 ) {

                //HTML_LOG(1,"Genre[%s]",gstr);
                if (gstr && *gstr) {
                    char *g = hashtable_search(*h,gstr);
                    if (g==NULL) {
                        HTML_LOG(1,"added Genre[%s]",gstr);
                        char *g=STRDUP(gstr);
                        hashtable_insert(*h,g,g);
                    }
                }
            }

            *p = save_c;

            gstr = p;
        }
    }
}

void get_first_two_letters(char *t) {
    if (g_first_two_letters == NULL) {
        g_first_two_letters = string_string_hashtable(20);
    }

    char *p = t;
    char pair[3];
    p = t-1;
    while(p) {
        p++;
        if (!*p) break;
        pair[0]=toupper(*p);
        pair[1]=tolower(p[1]);
        pair[2]='\0';
        if (hashtable_search(g_first_two_letters,pair) == NULL) {
            char *q=STRDUP(pair);
            hashtable_insert(g_first_two_letters,q,q);
        }
        p = strchr(p,' ');
    }
}
