BEGIN {
    g_has_ijson=1; # change to 1 to enable advanced mode
}

function fetch_ijson(fn,args,out,\
url,num,locales,i,ret) {

    ret = 0;
    id1("fetch_ijson "fn);

    num = get_locales(locales);
    for(i = 1 ; i<= num ; i++ ) {
        url = "http://a-pp.imdb.com/"fn"?api=v1&app-id=i-phone1&locale="locales[i]"&"args;
        gsub(/-/,"",url);
        if (fetch_json(url,"imdb",out)) {
            out["@locale@"] = locales[i];
            ret = 1;
            break;
        }
    }

    id0(ret);
    return ret;
}
	
function fetch_ijson_plot(id,minfo,\
json,ret) {
    
    fetch_ijson("title/plot","t-const="id,json);
    if ( "xxxx" in json) {
    }
    return ret;
}

function fetch_ijson_details(id,minfo,\
json,cast,ret,i,tag,minfo2) {
    
    id1("fetch_ijson_details "id);
    if (id && g_has_ijson) { #TODO fix disable of ijson

        if (!scrape_cache_get("imdb:"id,minfo2)) {
            if (fetch_ijson("title/main-details","t-const="id,json)) {

                if ( "data:year" in json) {
                    dump(0,"ijson",json);
                    ret = 1;
                    minfo2["mi_year"] = json["data:year"];
                    minfo2["mi_certrating"] = json["data:certificate:certificate"];
                    minfo2["mi_certcountry"] = substr(json["@locale@"],4);
                    minfo2["mi_poster"] = json["data:image:url"];
                    minfo2["mi_rating"] = json["data:rating"];
                    minfo2["mi_runtime"] = json["data:runtime:time"]/60;
                    minfo2["mi_title"] = json["data:title"];
                    minfo2["mi_plot"] = json["data:plot:outline"];

                    if (json["data:type"] == "tv_series" ) {
                        minfo2["mi_category"] = "T";
                    } else if (json["data:type"] == "feature" ) {
                        minfo2["mi_category"] = "M";
                    }

                    # Genres
                    for(i = 1 ; ; i++ ) {
                        tag = "data:genres#"i;
                        if (!(tag in json)) break;
                        minfo2["mi_genre"] = minfo2["mi_genre"] "|" json[tag];
                    }

                    set_ijson_people(json,"data:writers_summary",minfo2,"writer");
                    set_ijson_people(json,"data:directors_summary",minfo2,"director");
                    set_ijson_people(json,"data:cast_summary",minfo2,"actor");

    #                if (0) { #~~~~~~~~~~~~~~~~~~~~~~~~~~
    #                    #get cast
    #                    fetch_ijson("title/full-credits","t-const="id,cast);
    #                    for(i = 1 ; ; i++ ) {
    #                        tag = "data:credits#"i;
    #                        if (!(tag":token" in cast)) break;
    #                        if (cast[tag":token"] == "cast") {
    #                            set_ijson_people(cast,tag":list",minfo,"actor");
    #                            break;
    #                        }
    #                    }
    #                }
                    minfo_set_id("imdb",id,minfo2);
                }
            }
            scrape_cache_add("imdb:"id,minfo2);
        }
        minfo_merge(minfo,minfo2,"imdb");
    }
    ret= (minfo2["mi_year"] != "");
    id0(ret);
    return ret;
}

function set_ijson_people(json,json_tag,minfo,role,\
i,tag,img,id,total,max,mi_total,mi_names,mi_ids) {
    max = g_settings["catalog_max_"role"s"];
    if (!max) max=3;

    mi_total = "mi_"role"_total";
    mi_names = "mi_"role"_names";
    mi_ids = "mi_"role"_ids";

    total=minfo[mi_total];

    for(i = 1 ;  ; i++ ) {
        tag = json_tag "#"i ":name";
        if (!(tag":name" in json)) break;

        if (total+0 >= max) break;

        id = json[tag":nconst"];
        sub(/^nm/,"",id);

        if (index(minfo[mi_ids]"@","@"id"@") == 0) {

            total++;
            if (total == 1) {
                minfo[mi_names] = "imdb";
                minfo[mi_ids] = "imdb";
            }
            minfo[mi_names] = minfo[mi_names]"@"json[tag":name"];
            minfo[mi_ids] = minfo[mi_ids]"@"id;

            if (role == "actor") {
                img = tag":image:url";
                if (img in json) {
                    g_portrait_queue["imdb:"id] = json[img];
                }
            }
        }
    }
    minfo[mi_total] = total;
}
