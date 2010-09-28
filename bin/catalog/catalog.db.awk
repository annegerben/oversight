
# changes here should be reflected in db.c:write_row()
function createIndexRow(minfo,db_index,watched,locked,index_time,\
row,est,nfo,op,start) {

    # Estimated download date. cant use nfo time as these may get overwritten.
    est=file_time(minfo["mi_folder"]"/unpak.log");
    if (est == "") {
        est=file_time(minfo["mi_folder"]"/unpak.txt");
    }
    if (est == "") {
        est = minfo["mi_file_time"];
    }

    if (minfo["mi_file"] == "" ) {
        minfo["mi_file"]=getPath(minfo["mi_media"],minfo["mi_folder"]);
    }
    minfo["mi_file"] = clean_path(minfo["mi_file"]);

    if ((minfo["mi_file"] in g_fldrCount ) && g_fldrCount[minfo["mi_file"]]) {
        DEBUG("Adjusting file for video_ts");
        minfo["mi_file"] = minfo["mi_file"] "/";
    }

    op="update";
    if (db_index == -1 ) {
        db_index = ++gMaxDatabaseId;
        op="add";
    }
    row="\t"ID"\t"db_index;
    INF("dbrow "op" ["db_index":"minfo["mi_file"]"]");

    row=row"\t"CATEGORY"\t"minfo["mi_category"];

    if (index_time == "") {
        if (RESCAN == 1 ) {
            index_time = est;
        } else {
            index_time = NOW;
        }
    }

    row=row"\t"INDEXTIME"\t"shorttime(index_time);

    row=row"\t"WATCHED"\t"watched;
    row=row"\t"LOCKED"\t"locked;

    #Title and Season must be kept next to one another to aid grepping.
    #Put the overview items near the start to speed up scanning
    row=row"\t"TITLE"\t"minfo["mi_title"];
    if (minfo["mi_orig_title"] != "" && minfo["mi_orig_title"] != minfo["mi_title"] ) {
        row=row"\t"ORIG_TITLE"\t"minfo["mi_orig_title"];
    }
    if (minfo["mi_season"] != "") row=row"\t"SEASON"\t"minfo["mi_season"];

    row=row"\t"RATING"\t"minfo["mi_rating"];

    if (minfo["mi_episode"] != "") row=row"\t"EPISODE"\t"minfo["mi_episode"];

    row=row"\t"GENRE"\t"short_genre(minfo["mi_genre"]);
    row=row"\t"RUNTIME"\t"minfo["mi_runtime"];

    if (minfo["mi_parts"]) row=row"\t"PARTS"\t"minfo["mi_parts"];

    row=row"\t"YEAR"\t"short_year(minfo["mi_year"]);

    start=1;
    if (index(minfo["mi_file"],g_mount_root) == 1) {
        start += length(g_mount_root);
    }
    row=row"\t"FILE"\t"substr(minfo["mi_file"],start);

    if (minfo["mi_additional_info"]) row=row"\t"ADDITIONAL_INF"\t"minfo["mi_additional_info"];


    if (minfo["mi_imdb"] == "") {
        # Need to have some kind of id for the plot.
        minfo["mi_imdb"]=minfo["mi_tvid_plugin"]"_"minfo["mi_tvid"];
        if (minfo["mi_imdb"] == "") {
            # Need to have some kind of id for the plot.
            minfo["mi_imdb"]="ovs"PID"_"systime();
        }
    }
    row=row"\t"URL"\t"minfo["mi_imdb"];

    row=row"\t"CERT"\t"minfo["mi_certcountry"]":"minfo["mi_certrating"];
    if (minfo["mi_director"]) row=row"\t"DIRECTORS"\t"minfo["mi_director"];
    if (minfo["mi_actors"]) row=row"\t"ACTORS"\t"minfo["mi_actors"];
    if (minfo["mi_writers"]) row=row"\t"WRITERS"\t"minfo["mi_writers"];

    row=row"\t"FILETIME"\t"shorttime(minfo["mi_file_time"]);
    row=row"\t"DOWNLOADTIME"\t"shorttime(est);
    #row=row"\t"SEARCH"\t"g_search[i];


    if (minfo["mi_airdate"]) row=row"\t"AIRDATE"\t"minfo["mi_airdate"];

    if (minfo["mi_eptitle"]) row=row"\t"EPTITLE"\t"minfo["mi_eptitle"];
    nfo="";

    if (g_settings["catalog_nfo_write"] != "never" || is_file(minfo["mi_nfo_default"]) ) {
        nfo=minfo["mi_nfo_default"];
        gsub(/.*\//,"",nfo);
    }
    if (is_file(minfo["mi_folder"]"/"nfo)) {
        row=row"\t"NFO"\t"nfo;
    }
    if (minfo["mi_conn_follows"]) row=row"\t"CONN_FOLLOWS"\t"minfo["mi_conn_follows"];
    if (minfo["mi_conn_followed_by"]) row=row"\t"CONN_FOLLOWED"\t"minfo["mi_conn_followed_by"];
    if (minfo["mi_conn_remakes"]) row=row"\t"CONN_REMAKES"\t"minfo["mi_conn_remakes"];
    return row"\t";
}
function short_year(y) {
  return sprintf("%x",y-1900);
}
function short_genre(g,\
i,gnames,gcount) {
    gcount = split(g_settings["catalog_genre"],gnames,",");
    for(i = 1 ; i <= gcount ; i += 2) {
        if (match(g,"\\<"gnames[i]"o?\\>") ) {
           g = substr(g,1,RSTART-1) gnames[i+1] substr(g,RSTART+RLENGTH); 
       }
    }
    gsub(/[^-A-Za-z]+/,"|",g);
    return g;
}

# convert yyyymmddHHMMSS to bitwise yyyyyy yyyymmmm dddddhhh hhmmmmmm
function shorttime(t,\
y,m,d,hr,mn,r) {
    r = t;
    if (length(t) > 8 ) {

        y = n(substr(t,1,4))-1900;
        m = n(substr(t,5,2));
        d = n(substr(t,7,2));
        hr = n(substr(t,9,2));
        mn = n(substr(t,11,2));

        r = lshift(lshift(lshift(lshift(and(y,1023),4)+m,5)+d,5)+hr,6)+mn;
        r= sprintf("%x",r);
    }
    #INF("shorttime "t" = "r);
    return r;
}
function replace_database_with_new(newdb,currentdb,olddb) {

    INF("Replace Database ["newdb"] to ["currentdb"] to ["olddb"]");

    exec("cp -f "qa(currentdb)" "qa(olddb));

    touch_and_move(newdb,currentdb);

    set_permissions(qa(currentdb)" "qa(olddb));
}
function set_db_fields() {
    #DB fields should start with underscore to speed grepping etc.
    # Fields with @ are not written to the db.
    ID=db_field("_id","ID","",0);

    WATCHED=db_field("_w","Watched","watched") ;
    LOCKED=db_field("_l","Locked","locked") ;
    PARTS=db_field("_pt","PARTS","");
    FILE=db_field("_F","FILE","filenameandpath");
    NAME=db_field("_@N","NAME","");
    DIR=db_field("_@D","DIR","");
    EXT=db_field("_ext","EXT",""); # not a real field


    ORIG_TITLE=db_field("_ot","ORIG_TITLE","originaltitle");
    TITLE=db_field("_T","Title","title") ;
    DIRECTORS=db_field("_d","Director","director") ;
    ACTORS=db_field("_A","Actors","actors") ;
    WRITERS=db_field("_W","Writers","writers") ;
    CREATOR=db_field("_c","Creator","creator") ;
    AKA=db_field("_K","AKA","");

    CATEGORY=db_field("_C","Category","");
    ADDITIONAL_INF=db_field("_ai","Additional Info","");
    YEAR=db_field("_Y","Year","year") ;

    SEASON=db_field("_s","Season","season") ;
    EPISODE=db_field("_e","Episode","episode");

    GENRE=db_field("_G","Genre","genre") ;
    RUNTIME=db_field("_rt","Runtime","runtime") ;
    RATING=db_field("_r","Rating","rating");
    CERT=db_field("_R","CERT","mpaa"); #Not standard?

    PLOT=db_field("_P","Plot","plot");
    #EPPLOT=db_field("_ep","Plot","plot");

    URL=db_field("_U","URL","url");
    POSTER=db_field("_J","Poster","thumb");
    FANART=db_field("_fa","Fanart","fanart");

    DOWNLOADTIME=db_field("_DT","Downloaded","");
    INDEXTIME=db_field("_IT","Indexed","");
    FILETIME=db_field("_FT","Modified","");

    SEARCH=db_field("_SRCH","Search URL","search");
    PROD=db_field("_p","ProdId.","");
    AIRDATE=db_field("_ad","Air Date","aired");
    TVCOM=db_field("_tc","TvCom","");
    EPTITLE=db_field("_et","Episode Title","title");
    EPTITLEIMDB=db_field("_eti","Episode Title(imdb)","");
    AIRDATEIMDB=db_field("_adi","Air Date(imdb)","");
    NFO=db_field("_nfo","NFO","nfo");

    IMDBID=db_field("_imdb","IMDBID","id");
    TVID=db_field("_tvid","TVID","id");
    CONN_FOLLOWS=db_field("_a","FOLLOWS",""); # Comes After
    CONN_FOLLOWED=db_field("_b","FOLLOWED",""); # Comes Before
    CONN_REMAKES=db_field("_k","REMAKES",""); # Movies remaKes
}


#Setup db_field identifier, pretty name ,
# IN key = database key and html parameter
# IN name = logical name
# IN tag = xml tag in xmbc nfo files.
function db_field(key,name,tag) {
    g_db_field_name[key]=name;
    gDbTag2FieldId[tag]=key;
    gDbFieldId2Tag[key]=tag;
    return key;
}
##### LOADING INDEX INTO DB_ARR[] ###############################

#Used by generate nfo
function parseDbRow(row,arr,add_mount,\
fields,i,fnum) {

    delete arr;

    fnum = split(row,fields,"\t");
    for(i = 2 ; i-fnum <= 0 ; i+=2 ) {
        arr[fields[i]] = fields[i+1];
    }
    if (add_mount &&  arr[FILE] != "" && index(arr[FILE],"/") != 1 ) {
        arr[FILE] = g_mount_root arr[FILE];
    }
    arr[FILE] = clean_path(arr[FILE]);
}

function get_name_dir_fields(arr,\
f,fileRe) {

    f =  arr[FILE];

    if (isDvdDir(f)) {
        fileRe="/[^/]+/$"; # /path/to/name/[VIDEO_TS]
    } else {
        fileRe="/[^/]+$";  # /path/to/name.avi
    }

    if (match(f,fileRe)) {
        arr[NAME] = substr(f,RSTART+1);
        arr[DIR] = substr(f,1,RSTART-1);
    }
}

# Sort index by file path
function sort_index(file_in,file_out) {
    return exec("sed -r 's/(.*)(\t_F\t[^\t]*)(.*)/\\2\\1\\3/' "qa(file_in)" | sort > "qa(file_out)) == 0;
}

function get_dbline(file,\
line) {
    while ((getline line < file ) > 0 ) {
        if (index(line,"\t") == 1) {
            return line;
            break;
        }
    }
    close(file);
    #DEBUG("eof:"file);
    return "";
}

function keep_dbline(row,fields,\
result) {

    if (length(row) > g_max_db_len ) {

        INF("Row too long");

    } else if ( fields[DIR] ~ g_settings["catalog_ignore_paths"] ) {

        INF("Removing Ignored Path ["fields[FILE]"]");

    } else if ( fields[NAME] ~ g_settings["catalog_ignore_names"] ) {

        INF("Removing Ignored Name "fields[FILE]"]");

    } else {
        result = 1;
    }
    return result;
}

function write_dbline(fields,file,\
f) {
    for (f in fields) {
        if (f && index(f,"@") == 0) {
            printf "\t%s\t%s",f,fields[f] >> file;
        }
    }
    printf "\t\n" >> file;
}

function merge_index(file1,file2,file_out,person_extid2ovsid,\
row1,row2,fields1,fields2,action,max_id,total_unchanged,total_changed,total_new,total_removed,new_or_changed_line,ret) {

    ret = 1;
    id1("merge_index ["file1"]["file2"]");


    max_id = get_maxid(INDEX_DB);

    action = 3; # 0=quit 1=advance 1 2=advance 2 3=merge and advance both
    do {
        #INF("read action="action);
        if (and(action,1)) { 
            row1 = get_dbline(file1);
            parseDbRow(row1,fields1,1);
            INF("ORIGINAL:["fields1[FILE]"]");
        }
        if (and(action,2)) {
            row2 = get_dbline(file2);
            parseDbRow(row2,fields2,1);
            # change the external actor ids to oversight ids
            people_change_extid_to_ovsid(fields2,person_extid2ovsid);
            INF("NEW    :["fields2[FILE]"]");
        }

        if (row1 == "") {
            if (row2 == "") {
                # both finished
                action = 0;
            } else {
                action = 2;
            }
        } else {
            if (row2 == "") {
                action = 1;
            } else {
                # We compare the FILE field 

                if (fields1[FILE] == fields2[FILE]) {

                    action = 3;

                } else if (fields1[FILE] < fields2[FILE]) {
                    action = 1;
                } else {
                    action = 2;
                }
            }
        }

        #INF("merge action="action);
        new_or_changed_line = 0;
        if (action == 1) { # output row1
            if (keep_dbline(row1,fields1)) {
                total_unchanged++;
                print row1 >> file_out;
            } else {
                total_removed ++;
            }
            row1 = "";
        } else if (action == 2) { # output row2
            if (keep_dbline(row2,fields2)) {
                fields2[ID] = ++max_id;
                total_new++;
                write_dbline(fields2,file_out);
                new_or_changed_line = 1;
            }
            row2 = "";
        } else if (action == 3) { # merge
            # Merge the rows.
            fields2[WATCHED] = fields1[WATCHED];
            fields2[LOCKED] = fields1[LOCKED];
            fields2[FILE] = short_path(fields2[FILE]);

            if (keep_dbline(row2,fields2)) {
                total_changed ++;
                write_dbline(fields2,file_out);
                new_or_changed_line = 1;
            } else {
                total_removed++;
            }
        }

    } while (action > 0);

    close(file1);
    close(file2);
    close(file_out);

    set_maxid(INDEX_DB,max_id);

    INF("merge complete database:["file_out"]  unchanged:"total_unchanged" changed "total_changed" new "total_new" removed:"total_removed);
    id0(ret);
    return ret;
}

# Merge two index files together
function sort_and_merge_index(file1,file2,file1_backup,person_extid2name,\
file1_sorted,file2_sorted,file_merged,person_extid2ovsid) {

    id1("sort_and_merge_index ["file1"]["file2"]["file1_backup"]");

    file1_sorted = new_capture_file("dbsort1");
    file2_sorted = new_capture_file("dbsort2");
    file_merged =  new_capture_file("dbmerge");

    if (sort_index(file1,file1_sorted) )  {
        if (sort_index(file2,file2_sorted) )  {

            people_update_dbs(person_extid2name,person_extid2ovsid);

            if (merge_index(file1_sorted,file2_sorted,file_merged,person_extid2ovsid)) {

                replace_database_with_new(file_merged,file1,file1_backup);
            }
            
        }
    }
    rm(file1_sorted); 
    rm(file2_sorted); 
    rm(file_merged);
    id0("");
}

# Get all of the files that have already been scanned that start with the 
# same prefix.
function get_files_in_db(prefix,db,list,\
dbline,dbfields,err,count,filter) {

    count = 0;
    id1("get_files_in_db ["prefix"]");
    delete list;
    list["@PREFIX"] = prefix =  short_path(prefix);
    list["@REGEX"] = filter = "\t" FILE "\t" re_escape(prefix) "/?[^/]*\t";

    INF("filter=["filter"]");

    while((err = (getline dbline < db )) > 0) {

        if ( index(dbline,prefix) && dbline ~ filter ) {

            parseDbRow(dbline,dbfields,1);

            add_file(dbfields[FILE],list);

            count++;
        }
    }
    if (err >= 0 ) close(db);
    id0(count" files");
}



# Re-instate old pruning test with extra folder check for absent media
# Because we need to check every file in the database it can take some time
# also if using awk we want to avoid spawning a process (or two) for each check
# so ls is used. If a file is absent then it is removed only if its grandparent is 
# present - this is to allow for detached devices. (sort of)
function remove_absent_files_from_new_db(db,\
    tmp_db,dbfields,\
    list,f,shortf,last_shortf,maxCommandLength,dbline,keep,\
    gp,blacklist_re,blacklist_dir,timer) {
    list="";
    maxCommandLength=3999;

    INF("Pruning...");
    tmp_db = db "." JOBID ".tmp";

    # TODO if index is sorted by file we can do this a folder at a time.
    # TODO : not needed : get_files_in_db("",db);

    if (lock(g_db_lock_file)) {
        g_kept_file_count=0;
        g_absent_file_count=0;

        close(db);
        while((getline dbline < db ) > 0) {

            if ( index(dbline,"\t") != 1 ) { continue; }

            parseDbRow(dbline,dbfields,1);

            f = dbfields[FILE];
            shortf = short_path(f);
            #INF("Prune ? ["f"]");

            keep=1;

            # as db is in file order we can prune duplicates by comparing with last file
            if (shortf == last_shortf) {
                keep = 0;
                WARNING("Skipping "f" - duplicate");


            } else if (blacklist_re != "" && f ~ "NETWORK_SHARE/("blacklist_re")" ) {

                WARNING("Skipping "f" - blacklisted device");
            } else {

                timer = systime();
                if (is_file_or_folder(f) == 0 ) {
                    if (systime()-timer > 10) {
                        # error accessing nas - blacklist this path.
                        blacklist_dir = f;
                        if (match(f,"NETWORK_SHARE/[^/]+/")) {
                            blacklist_dir = substr(f,RSTART,RLENGTH);
                            sub(/.*NETWORK_SHARE/,"",blacklist_dir);
                            ERR("Unresponsive device : Blacklisting access to NETWORK_SHARE"blacklist_dir);
                            blacklist_re = blacklist_re "|" blacklist_dir;
                            DEBUG("re = "blacklist_re);
                        }
                    } else {
                        gp = mount_point(f);
                        if (gp != "/share" ) {
                            #if gp folder is present then delete
                            if (is_dir(gp) && !is_empty(gp)) {
                                keep=0;
                            } else {
                                INF("Not mounted?");
                            }
                        } else {
                            # just delete it.
                            keep=0;
                        }
                    }
                }
            }


            if (keep) {
                print dbline > tmp_db;
                g_kept_file_count++;
            } else {
                INF("Removing "f);
                g_absent_file_count++;
                
            }
            last_shortf = shortf;
        }
        close(tmp_db);
        close(db);
        INF("unchanged:"g_kept_file_count);
        INF("removed:"g_absent_file_count);
        replace_database_with_new(tmp_db,db,INDEX_DB_OLD);
        unlock(g_db_lock_file);
    }
}

