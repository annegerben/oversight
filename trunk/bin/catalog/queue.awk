# This module has functions that initially move the scanned information into the various database files index.db, people, plot

function queue_minfo(minfo,qfile,person_extid2name,\
fld) {
    INF("queue:queue_minfo:"minfo["mi_media"]);

    person_add_db_queue(minfo,person_extid2name);

    # Write mi_folder and mi_media first for easy sorting.
    queue_fld(minfo,"mi_folder",qfile);
    queue_fld(minfo,"mi_media",qfile);
    for(fld in minfo) {
        if (fld != "mi_folder" && fld != "mi_media") {
            queue_fld(minfo,fld,qfile);
        }
    }
    print "" >> qfile;
}

function queue_fld(minfo,fld,qfile,\
txt) {
    txt=minfo[fld];

    #replace CR
    if (index(txt,"\n")) gsub(/\n/,"\\n",txt);

    printf "%s",fld SUBSEP txt SUBSEP >> qfile;
}

function read_minfo(qfile,minfo,\
line,i,tmp,num) {
    delete minfo;
    while ((getline line < qfile ) > 0 ) {
        #INF("queue: line=["line"]");
        if (index(line,SUBSEP) ) {

            #reinstate CR
            if (index(line,"\\n")) {
                gsub(/\\n/,"\n",line);
            }

            num = split(line,tmp,SUBSEP);
            for(i = 1 ; i<=num ; i+= 2) {
                minfo[tmp[i]] = tmp[i+1];
            }
            #INF("read "num" fields minfo media = "minfo["mi_media"]);
            return num;
            break;
        }
    }
    close(qfile);
    #DEBUG("eof:"file);
    return num;
}


function merge_queue(qfile,person_extid2name,\
total) {

    if (g_opt_dry_run) {

        INF("Database update skipped - dry run");

    } else {

        if(lock(g_db_lock_file)) {
            total += sort_and_merge_index(INDEX_DB,qfile,INDEX_DB_OLD,person_extid2name);
            unlock(g_db_lock_file);
        }
    }
    rm(qfile);
    delete person_extid2name;
    return total;
}

# Merge two index files together
function sort_and_merge_index(dbfile,qfile,file1_backup,person_extid2name,\
file1_sorted,file_merged,person_extid2ovsid,total) {

    id1("sort_and_merge_index ["dbfile"]["qfile"]["file1_backup"]");

    file1_sorted = new_capture_file("dbsort");
    file_merged =  new_capture_file("dbmerge");

    if (sort_index(dbfile,file1_sorted) )  {

        if (sort_file(qfile) )  {

            people_update_dbs(person_extid2name,person_extid2ovsid);

            total = merge_index(file1_sorted,qfile,file_merged,person_extid2ovsid);
            if (total) {

                replace_database_with_new(file_merged,dbfile,file1_backup);
            }
            
        }
    }
    rm(file1_sorted); 
    rm(qfile); 
    rm(file_merged);
    id0("");
    return total;
}

function merge_index(dbfile,qfile,file_out,person_extid2ovsid,\
row1,row2,fields1,fields2,action,max_id,total_unchanged,total_changed,total_new,total_removed,ret,plot_ids,minfo,changed_line) {

    id1("merge_index ["dbfile"]["qfile"]");


    max_id = get_maxid(INDEX_DB);

    action = 3; # 0=quit 1=advance 1 2=advance 2 3=merge and advance both
    do {
        #INF("read action="action);
        if (and(action,1)) { 
            row1 = get_dbline(dbfile);
            parseDbRow(row1,fields1,1);
        }
        if (and(action,2)) {
            if (read_minfo(qfile,minfo)) {
                row2 = createIndexRow(minfo,-1,0,0,"");
                parseDbRow(row2,fields2,1);
                INF("NEW    :["fields2[FILE]"]");
            }
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

        changed_line = "";

        #DEBUG("merge action="action);
        if (action == 1) { # output row1
            if (keep_dbline(row1,fields1)) {
                total_unchanged++;
                print row1 >> file_out;
                keep_plots(fields1,plot_ids);
            } else {
                total_removed ++;
            }
            row1 = "";
        } else if (action == 2) { # output row2

            if (keep_dbline(row2,fields2)) {

                changed_line = ++max_id;
                total_new++;
            }
            row2 = "";
        } else if (action == 3) { # merge
            # Merge the rows.
            fields2[WATCHED] = fields1[WATCHED];
            fields2[LOCKED] = fields1[LOCKED];
            fields2[FILE] = short_path(fields2[FILE]);

            if (keep_dbline(row2,fields2)) {
                changed_line = fields1[ID];
                total_changed ++;
            } else {
                total_removed++;
            }
            row1 = row2 = "";
        }

        if (changed_line != "") {
            fields2[ID] = minfo["mi_ovsid"] = changed_line;
            keep_plots(fields2,plot_ids);
            queue_plots(minfo,g_plot_file_queue);
            # change the external actor ids to oversight ids
            people_change_extid_to_ovsid(fields2,person_extid2ovsid);
            write_dbline(fields2,file_out);

            get_images(minfo);

            # TODO Pass plot. Change to use minfo ?
            generate_nfo_file_from_fields(g_settings["catalog_nfo_format"],fields2);
            new_content(fields2);
        }


    } while (action > 0);

    close(dbfile);
    close(qfile);
    close(file_out);
    close(g_plot_file_queue);

    set_maxid(INDEX_DB,max_id);

    update_plots(g_plot_file,g_plot_file_queue,plot_ids);

    INF("merge complete database:["file_out"]  unchanged:"total_unchanged" changed "total_changed" new "total_new" removed:"total_removed);
    ret = total_changed + total_new;
    id0(ret);
    return ret;
}
function set_maxid(file,max_id,\
filemax) {
    filemax = file".maxid";
    print max_id > filemax;
    close(filemax);
    INF("set_maxid["file"]="max_id);
}


function get_maxid(file,\
max_id,line,fields,filemax,tab) {
    max_id = 0;
    filemax = file".maxid";

    if (!is_file(filemax)) {
        if (is_file(file)) {
            if (file == INDEX_DB ) {
                # get mex id from main database index.db - using field _ID
                while ((line = get_dbline(file) ) != "") {
                    parseDbRow(line,fields,0);
                    if (fields[ID]+0 > max_id+0) {
                        max_id = fields[ID];
                    }
                }
            } else {
                # id is the first field
                while ((getline line < file ) > 0) {
                    if ((tab = index(line,"\t")) > 0) {
                        line = substr(line,1,tab-1);
                        if (index(line,"nm") == 1) {
                            #remove imdb nm prefix
                            line = substr(line,3);
                        }
                        if (line + 0 > max_id+0) {
                            max_id = line;
                        }
                    }
                    max_id = fields[1];
                }
            }
            close(file);
        }
        set_maxid(file,max_id);

    } else {
        getline max_id < filemax;
        close(filemax);
        max_id += 0;
        INF("get_maxid["file"]="max_id);
    }
    return max_id;
}

