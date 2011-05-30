// $Id:$
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <time.h>
#include <ctype.h>

#include "types.h"
#include "display.h"
#include "gaya_cgi.h"
#include "util.h"
#include "array.h"
#include "db.h"
#include "dbplot.h"
#include "dboverview.h"
#include "oversight.h"
#include "hashtable.h"
#include "hashtable_loop.h"
#include "macro.h"
#include "mount.h"
#include "template.h"
//#include "exp.h"
#include "filter.h"
#include "abet.h"
    
// When user drills down to a new view, there are some navigation html parameters p (page) and idlist and view.
// The old values are prefixed with @ before adding new ones.
//
// If a paramter is in this list then it is effectively pushed onto a stack and cleared when the user drills down
// If not in the list then the perameter will continue to be fully active when drilling down. (persistent)
//
// eg if QUERY_PARAM_RATING is in the list , then when the user drills down to a move box set view they will
// only see items with the given rating.
//
// Drilldown (non-persistent) parameters do not apply to links on a detail page.
// This is because it is assumed that once at the detail page you have viewed your target information.
// (This needs some thought and discussion). eg.
// say you filter on all moves score > 8. You see The Matrix. Go to Keanu Reeves Bio. What do you want to see
// 1. All movies with Keanu.
// 2. Only those moves with score > 8. 
// Chances are 1. 
// So add Rating to DRILLDOWN links.

#define DRILLDOWN_CHAR '@'
// TODO: All drill down params should start with special character rather than having list below. eg
// top level has @xxx
#define DRILL_DOWN_PARAM_NAMES QUERY_PARAM_SELECTED","QUERY_PARAM_PAGE","QUERY_PARAM_IDLIST","QUERY_PARAM_VIEW"," QUERY_PARAM_TITLE_FILTER "," QUERY_PARAM_SEASON "," QUERY_PARAM_PERSON "," QUERY_PARAM_RATING "," QUERY_PARAM_ACTION"," QUERY_PARAM_CONFIG_HELP","QUERY_PARAM_CONFIG_TITLE","QUERY_PARAM_CONFIG_FILE

#define JAVASCRIPT_EPINFO_FUNCTION_PREFIX "tvinf_"
#define JAVASCRIPT_MENU_FUNCTION_PREFIX "t_"

char *td_mouse_event_fn(char *function_name_prefix,long function_id);
char *href_focus_event_fn(char *function_name_prefix,long function_id);
char *get_theme_image_link(char *qlist,char *href_attr,char *image_name,char *button_attr);
char *get_theme_image_tag(char *image_name,char *attr);
void util_free_char_array(int size,char **a);
char *get_date_static(DbItem *item);
DbItem **filter_page_items(int start,int num_rows,DbItem **row_ids,int max_new,int *new_num);
static inline ViewMode*get_drilldown_view(DbItem *item);
char *get_final_link_with_font(char *params,char *attr,char *title,char *font_attr);
static char *get_drilldown_name_static(char *root_name,int num_prefix);
void remove_blank_params_inplace(char *input);
void get_watched_counts(DbItem *item,int *watchedp,int *unwatchedp);
char *get_tv_drilldown_link(ViewMode *view,char *name,int season,char *attr,char *title,char *font_class,char *cell_no_txt);
char *get_tvboxset_drilldown_link(ViewMode *view,char *name,char *attr,char *title,char *font_class,char *cell_no_txt);
char *get_movie_drilldown_link(ViewMode *view,char *idlist,char *attr,char *title,char *font_class,char *cell_no_txt);
char *image_source(char *subfolder,char *image_name,char *ext);
int is_drilldown_of(char *param_name,char *root_name) ;
char *menu_js_fn(long fn_id,...);

char *get_play_tvid(char *text) {
    char *result;
    ovs_asprintf(&result,
        "<a href=\"file://" NMT_PLAYLIST "?start_url=\" vod=playlist tvid=\"_PLAY\">%s</a>",text);
    return result;
}

// Return a full path 
char *get_path(DbItem *item,char *path,int *freepath) {

TRACE;

    *freepath = 0;
    char *mounted_path=NULL;

    char *path_relative_to_host_nmt=NULL;

    assert(path);
    if (path[0] == '/' ) {

        path_relative_to_host_nmt = path;

    } else if (util_starts_with(path,"ovs:")) {

        ovs_asprintf(&path_relative_to_host_nmt,"%s/db/global/%s",appDir(),path+4);
        *freepath = 1;

    } else {

        char *d=util_dirname(item->file);
       ovs_asprintf(&path_relative_to_host_nmt,"%s/%s",d,path);
       FREE(d);
       *freepath = 1;
    }

    int free2;
    mounted_path=get_mounted_path(item->db->source,path_relative_to_host_nmt,&free2);
    if (free2) {
        *freepath = 1;
    }

    // Free intermediate result if necessary
    if (path_relative_to_host_nmt != path && path_relative_to_host_nmt != mounted_path) {
        FREE(path_relative_to_host_nmt);
    }

    return mounted_path;
}







#define XHTML
void tag(char *label,char *attr,va_list ap) {

#ifdef XHTML
    static int count = 0;
    static char *stack[50];
#endif

    if (attr == NULL) {
        printf("</%s>\n",label);
#ifdef XHTML
        count --;
        if (count >= 0) {
            FREE(stack[count]);
        } else {
            html_error("empty html stack for </%s>",label);
        }
#endif
    } else {
        
#ifdef XHTML
        if (count && STRCMP(label,stack[count]) == 0) {
            html_error("double nested <%s>",label); // div ok really
        }
#endif

        if (*attr == '\0') {
            printf("<%s>\n",label);
        } else {
            printf("<%s ",label);
            vprintf(attr,ap);
            printf(" >");
        }

#ifdef XHTML
        stack[count++] = STRDUP(label);
#endif
    }
}

void td(char *attr,...) {
    va_list ap;
    va_start(ap,attr);
    tag("td",attr,ap);
    va_end(ap);
}

// This used to be just "" but when replacing wget we need to send the full path
char *cgi_url(int full) {
    char *url = NULL;
    //HTML_LOG(0,"local_browser = [%d]",g_dimension->local_browser);
    if (g_dimension->local_browser || full) {
        url = getenv("SCRIPT_NAME");
        //HTML_LOG(0,"cgi_url = SCRIPT_NAME = [%s]",url);
    } else {
        url = "";
        //HTML_LOG(0,"cgi_url = [%s]",url);
    }
    return url;
}

// Merge the current query string with the parameters.
// Keep the parameters that are not in the new parameter list and are also not blank
// Also exclude "colour" because it represents a one off action and not at state.
#define MAX_PARAMS 50
char *self_url(char *new_params) {

    struct hashtable_itr *itr;
    char *param_name;
    char *param_value;

    // Store params here.
    int pcount=0;
    int psize=0;
    char *pname[MAX_PARAMS];
    char *pval[MAX_PARAMS];

    // Cycle through each of the existing parameters
    for(itr=hashtable_loop_init(g_query) ; hashtable_loop_more(itr,&param_name,&param_value) ; ) {

        if (!EMPTY_STR(param_value)) {

            // Ignore parameters colour  option_* and orig_option_*

            if (param_name[0] == 'c' && STRCMP(param_name,"colour") == 0) {
                // ignore
            } else if (param_name[0] == 'o' && 
                   (  util_starts_with(param_name,"option_" ) || util_starts_with(param_name,"orig_option_" ) ) ) {
                // ignore
            } else {
                //
                // search for pram_name in new_params

                // If the existing parameter name is also in the new parameter list then dont add it
                if (EMPTY_STR(new_params) || !delimited_substring(new_params,"&",param_name,"&=",1,1)) {

                    // Keep the old parameter
                    pname[pcount] = param_name;
                    pval[pcount] = param_value;
                    pcount++;
                    assert(pcount < MAX_PARAMS);
                    // add 3 *param length to allow for url_encoding
                    psize += strlen(param_name) + 3*strlen(param_value) + 3;
                }
            }
        }
    }

    char *tmp;
    char *self = cgi_url(0);
    char *new=MALLOC(strlen(self)+psize+strlen(new_params) + 3);
    tmp = new;
    tmp += sprintf(tmp,"%s",self);
    int i;
    for(i = 0 ; i < pcount ; i ++ ) {

        int free_param;
        char *value = url_encode_static(pval[i],&free_param);

        HTML_LOG(1,"encoded[%s]",value);
        tmp += sprintf(tmp,"%c%s=%s",(i==0?'?':'&'),pname[i],value);

        if (free_param) {
            FREE(value);
        }
    }
    sprintf(tmp,"%c%s",(pcount==0?'?':'&'),new_params);

    remove_blank_params_inplace(new);
    HTML_LOG(1,"encoded final[%s]",new);

    return new;
}

#define PARAM_SEP(c) (((c) == '\0') || ((c) == '&' ) || ((c) == ';' ))

void remove_blank_params_inplace(char *in)
{
    char *read_p=in;

    // read_block points to next pa
    char *next_parameter = NULL;
    char *out_end = NULL;

    while( (read_p = strchr(read_p,'=')) != NULL) {

        read_p++;
        if (out_end == NULL)  {
             // Start at first parameter.
             out_end = strchr(in,'?');
             assert(out_end);
             out_end ++;
             next_parameter = out_end;
        }

        if (PARAM_SEP(*read_p)) {
            // empty param - = followed by sep - skip forward.
            next_parameter = read_p;

        } else {
            // found a parameter
            if (  next_parameter == out_end ) {
                // Advance  both next_parameter and out_end to next parameter.
                out_end++; // skip past & or first char
                while (!PARAM_SEP(*out_end)) {
                    out_end++;
                }
                next_parameter = out_end;

            } else {
                // copy from next_parameter to next PARAM_SEP
                if (out_end[-1] == '?' ) {
                    next_parameter++; // skip ampersand if at start
                } else {
                    *out_end++ = *next_parameter++;
                }
                while (!PARAM_SEP(*next_parameter)) {
                    *out_end++ = *next_parameter++;
                }
            }
            read_p = next_parameter;
        }
    }
    // Add final null if there were any = in the string 
    if (out_end) *out_end = '\0';
}

static char *self_url2(char *q1,char *q2)
{

    int free_full=0;
    char *full_query_string = NULL;
    /**
     * Now append the drilldown parameters to the actual parameters for this link
     * It is expected that the actual parameters will not contain drilldown @ parameters.
     */
    if (!EMPTY_STR(q1)) {
        if (EMPTY_STR(q2)) {
            full_query_string = q1;
        } else {
            free_full=1;
            ovs_asprintf(&full_query_string,"%s&%s",q1,q2);
        }
    } else {
        full_query_string = q2;
    }

TRACE;
    char *result = self_url(full_query_string);
TRACE;
    if (free_full) {
TRACE;
        FREE(full_query_string);
TRACE;
    }
TRACE;

    return result;
}
Array *get_drilldown_root_names() {
    static Array *drilldown_root_names= NULL;
    if (drilldown_root_names == NULL) {
        drilldown_root_names = split(DRILL_DOWN_PARAM_NAMES,",",0);
    }
    return drilldown_root_names;
}
/*
 * 'new_params = query string selgment - ie p=1&q=2"
 * @param_list = list of parameters whose old values are kept in the url.
 *
 * eg given
 *
 * 'p=1 & @p=2 & @@p = 3
 *
 * '@p=1 & @@p=2 & @@@p = 3   
 *
 */
char *drill_down_url(char *new_params) 
{
    /*
     * for each parameter name pname in param list
     *    for each param Q in g_query do
     *       if Q.name matches ^@*(pname)$  eg , (pname) , @(pname) , @@(pname) etc.
     *          add @(Q.name)=(Q.value) to new parameter list.
     *       end if 
     *    end for
     * end for
     */


    char *new_drilldown_params = NULL;

    Array *drilldown_root_names= get_drilldown_root_names();


    int i;
    if (drilldown_root_names) {
        for(i = 0 ; i < drilldown_root_names-> size ; i++ ) {
            char *param_name = drilldown_root_names->array[i];
            if (param_name) {
                struct hashtable_itr *itr;
                char *qname;
                char *qval;
                int min_depth = 0;
                // we need to track the item with fewest DRILLDOWN_CHAR and remove it
                // eg @p=1&@@q=2 becomes
                // eg @@p=1&@@@q=2 becomes


                for(itr=hashtable_loop_init(g_query) ; hashtable_loop_more(itr,&qname,&qval) ; ) {
                    int depth = is_drilldown_of(qname,param_name);

// If item is more than this deep - ignore it                    
#define MAX_DRILLDOWN_DEPTH 4
                    if (depth >= 1 && depth <= MAX_DRILLDOWN_DEPTH ) {
                        char *tmp;

                        int free_val;
                        char *encoded_val = url_encode_static(qval,&free_val);

                        char *new_name = get_drilldown_name_static(param_name,depth);

                        ovs_asprintf(&tmp,"%s%s%s=%s",
                                    NVL(new_drilldown_params),
                                    (new_drilldown_params?"&":""),
                                    new_name,encoded_val);

                        FREE(new_drilldown_params);
                        new_drilldown_params = tmp;

                        if (free_val) FREE(encoded_val);

                        // track item with fewest DRILLDOWN_CHAR
                        if (depth < min_depth || min_depth == 0 ) {
                            min_depth = depth;
                        }
                    }
                }
                // Find item with fewest prefix. If it is not in the new_params then remove it
                if (min_depth) {
                    char *top_name = get_drilldown_name_static(param_name,min_depth-1);

                    if (!delimited_substring(new_params,"&",top_name,"&=",1,1)) {
                        char *tmp;
                        ovs_asprintf(&tmp,"%s&%s=",new_drilldown_params,top_name);
                        FREE(new_drilldown_params);
                        new_drilldown_params = tmp;
                    }
                }
            }
        }
    }

    /*
     * Compute the new url
     */
    char *final = self_url2(new_params,new_drilldown_params);

    FREE(new_drilldown_params);

    return final;
}

// this is a query string where existing @p,@view,@idlist parameters are moved back to
// p,view,idlist // so that we can return the the previous screen.
char *return_query_string() 
{

    /*
     * given parameter name p we want
     * p=1&@p=2&@@p=3 to become p=2&@p=3
     *
     * for each parameter pname in param list
     *    for each param Q in g_query do
     *       if Q.name = pname 
     *          if @name not in query then
     *             add Q.name=<blank>
     *          endif
     *       else if Q matches ^@+param$  eg , @param , @@param etc.
     *          add (Q.name)=((@Q).value) to new parameter list.
     *       end if 
     *    end for
     * end for
     *
 * e.g
 * '@p=1 & @@p=2 & @@@p = 3  
 *
 * becomes
 *
 * 'p=1 & @p=2 & @@p = 3
     */
    Array *drilldown_root_names= get_drilldown_root_names();

    Array *new_drilldown_params = array_new(free);

    int i;
    if (drilldown_root_names) {
        for(i = 0 ; i < drilldown_root_names-> size ; i++ ) {
            char *param_name = drilldown_root_names->array[i];

            if (param_name) {
                struct hashtable_itr *itr;
                char *qname;
                char *qval;

                int max_depth = 0; // track the deepest level parameter to remove it

                for(itr=hashtable_loop_init(g_query) ; hashtable_loop_more(itr,&qname,&qval) ; ) {

                    int depth = is_drilldown_of(qname,param_name);

                    if (depth > 1) {
                        char *tmp;

                        char *new_name = get_drilldown_name_static(param_name,depth -2);

                        int free_val;
                        char *encoded_val = url_encode_static(qval,&free_val);

                        ovs_asprintf(&tmp,"&%s=%s",new_name,encoded_val);
                        array_add(new_drilldown_params,tmp);

                        if (free_val) FREE(encoded_val);

                    }
                    if (depth > max_depth ) {
                        max_depth = depth;
                    }
                }
                // Now remove the deepest eg.
                // if p=1&@p=2&@@p=3 becomes p=2&@p=3 we need to add @@p=
                if (max_depth > 0 ) {
                    int num_prefix = max_depth - 1;
                    char *last_name = get_drilldown_name_static(param_name,num_prefix);

                    char *tmp;
                    ovs_asprintf(&tmp,"&%s=",last_name);
                    array_add(new_drilldown_params,tmp);
                }



            }
        }
    }
    char *result = arraystr(new_drilldown_params);
    FREE(new_drilldown_params);
    return result;
}

// This is the same as return_query_string() but acts on the current query parameters
// rather then generating a new URL
void query_pop() 
{

    /*
     * given parameter name p we want
     * p=1&@p=2&@@p=3 to become p=2&@p=3
     *
     * for each parameter pname in param list
     *    for each param Q in g_query do
     *       if Q.name = pname 
     *          if @name not in query then
     *             add Q.name=<blank>
     *          endif
     *       else if Q matches ^@+param$  eg , @param , @@param etc.
     *          add (Q.name)=((@Q).value) to new parameter list.
     *       end if 
     *    end for
     * end for
     *
 * e.g
 * '@p=1 & @@p=2 & @@@p = 3  
 *
 * becomes
 *
 * 'p=1 & @p=2 & @@p = 3
     */
    Array *drilldown_root_names= get_drilldown_root_names();

    int i;
    if (drilldown_root_names) {
        for(i = 0 ; i < drilldown_root_names-> size ; i++ ) {
            char *param_name = drilldown_root_names->array[i];

            if (param_name) {
                struct hashtable_itr *itr;
                char *qname;
                char *qval;

                int max_depth = 0; // track the deepest level parameter to remove it

                for(itr=hashtable_loop_init(g_query) ; hashtable_loop_more(itr,&qname,&qval) ; ) {

                    int depth = is_drilldown_of(qname,param_name);

                    if (depth > max_depth ) {
                        max_depth = depth;
                    }
                }
                // Now  shift them all up and remove the deepest eg.
                // if p=1&@p=2&@@p=3 becomes p=2&@p=3 we need to add @@p=
                if (max_depth > 0 ) {
                    char *name;
                    ovs_asprintf(&name,"%.*s%s",max_depth-1,"@@@@@@@@@@",param_name);
                    HTML_LOG(0,"query_pop:name=[%s]",name);
                    int i;

                    // eg if max_depth=2 name=@i new_name=i 
                    for(i = 1 ; i < max_depth ; i++ ) {
                        char *new_name = name+(max_depth-i);
                        char *old_name = new_name-1 ;
                        char *val = query_val(old_name);

                        HTML_LOG(0,"move [%s] from [%s] to [%s]",val,old_name,new_name);

                        query_update(STRDUP(new_name),STRDUP(val));
                    }
TRACE1;
                    query_remove(name);
TRACE1;
                    HTML_LOG(0,"query_pop:done=[%s]",name);
                }
            }
        }
    }
}

// Compute url to go back to previous link.
char *return_url() {
    /*
     * Compute the new url
     */
    char *tmp = return_query_string();
    char *final = self_url(tmp+1);
    HTML_LOG(0,"return url = [%s]",final);
    FREE(tmp);

    return final;
}

#define NEW_DRILLDOWN_STYLE
static char *get_drilldown_name_static(char *root_name,int num_prefix)
{
#define MAX_PARAM_NAME_LEN 30
    static char name[MAX_PARAM_NAME_LEN];
   
#ifdef NEW_DRILLDOWN_STYLE
    int i;
    if (num_prefix) {
        i =sprintf(name,"%c%c%s",DRILLDOWN_CHAR, 'A' + num_prefix -1 , root_name );
    } else {
        i =sprintf(name,"%s", root_name );
    }
    assert(i <= MAX_PARAM_NAME_LEN);
#else

    int i;
    for( i = 0 ; i < num_prefix ; i++ ) {
        name[i] = DRILLDOWN_CHAR;
    }
    strcpy(name+i,root_name);
#endif
    return name;
}
/*
 * True if param_name ~ ^DRILLDOWN_CHAR*root$
 * eg. @@@p is_drilldown_of p
 * @returns 0(no match) , 1=exact match , else 1+number of DRILLDOWN_CHAR
 */

int is_drilldown_of(char *param_name,char *root_name) 
{
#ifdef NEW_DRILLDOWN_STYLE
    int result=1;
    char *p=param_name;
    if (*p == DRILLDOWN_CHAR) {
        p++;
        result=*p-'A'+2;
        p++;
    } 
    if (STRCMP(p,root_name) != 0) {
        result = 0;
    }
#else
    int result=0;
    char *p=param_name;
    while (*p && *p == DRILLDOWN_CHAR ) {
        p++;
    }
    if ( STRCMP(p,root_name) ==0 ) {
        result = (p-param_name)+1;
    }
#endif
    //HTML_LOG(0,"is_drilldown_of(%s,%s)=%d",param_name,root_name,result);
    return result;
}


/**
 * link with all drilldown info removed
 */
char *final_url(char *new_params) 
{

    char *new_drilldown_params = NULL;
    Array *drilldown_root_names= get_drilldown_root_names();
    int i;
    if (drilldown_root_names) {
        for(i = 0 ; i < drilldown_root_names-> size ; i++ ) {
            char *param_name = drilldown_root_names->array[i];

            if (param_name) {
                struct hashtable_itr *itr;
                char *qname;
                char *qval;

                for(itr=hashtable_loop_init(g_query) ; hashtable_loop_more(itr,&qname,&qval) ; ) {

                    int depth = is_drilldown_of(qname,param_name);

                    if (depth) {
                        // remove it
                        char *tmp;

                        if (new_drilldown_params == NULL) {
                            ovs_asprintf(&new_drilldown_params,"%s=",qname);
                        } else {
                            ovs_asprintf(&tmp,"%s&%s=",new_drilldown_params,qname);
                            FREE(new_drilldown_params);
                            new_drilldown_params = tmp;
                        }
                    }
                }
            }
        }
    }
TRACE;
    /*
     * Compute the new url
     */

    char *final = self_url2(new_params,new_drilldown_params);
HTML_LOG(0,"XX [%s] [%s] to [%s]",new_params,new_drilldown_params,final);
    FREE(new_drilldown_params);

    return final;
}


char *get_self_link(char *params,char *attr,char *title) {

    assert(params);
    assert(attr);
    assert(title);
    char *result=NULL;

    if (strstr(params,FORM_PARAM_SELECT_VALUE_MARK)) {
        HTML_LOG(0," begin self link for params[%s] attr[%s] title[%s]",params,attr,title);
        html_hashtable_dump(0,"query",g_query);
    }

    char *url = self_url(params);
    if (strstr(params,FORM_PARAM_SELECT_VALUE_MARK))
        HTML_LOG(0," end self link [%s]",url);

    ovs_asprintf(&result,"<a href=\"%s\" %s>%s</a>",url,attr,title);

    FREE(url);
    return result;
}

/*
 * This type of link renames any parameters that are superceeded. eg
 * if p=1 and the drilldown link has p=2 then it becomes..
 * p=2&@p=1
 * this only happens for listed parameters.
 * @params - parameter list 'p=1&q=2'
 * @attr - <a> attributes.
 * @title  <a>title</a>
 */
char *drill_down_link(char *params,char *attr,char *title)
{
    assert(params);
    assert(attr);
    assert(title);
    char *result=NULL;

    HTML_LOG(1," begin drill down link for params[%s] attr[%s] title[%s]",params,attr,title);

    char *url = drill_down_url(params);
    HTML_LOG(2," end self link [%s]",url);

    ovs_asprintf(&result,"<a href=\"%s\" %s>%s</a>",url,attr,title);

    FREE(url);
    return result;
}

/*
 * This type of link undoes the effect of drill_down_url() ie.
 * p=2&@p=1 becomes p=1
 * this only happens for listed parameters.
 * @params - parameter list 'p=1&q=2'
 * @attr - <a> attributes.
 * @title  <a>title</a>
 * @param_list = list of parameters whose old values are kept in the url.
 */
char *return_link(char *attr,char *title) {
    assert(attr);
    assert(title);
    char *result=NULL;

    HTML_LOG(1," begin drill down link for attr[%s] title[%s]",attr,title);

    char *url = return_url();
    HTML_LOG(2," end drill down link [%s]",url);

    ovs_asprintf(&result,"<a href=\"%s\" %s>%s</a>",url,attr,title);

    FREE(url);
    return result;
}
// This is a link with all drill down parameters removed. This is for the final link
// to play the file.
char *final_link(char *params,char *attr,char *title) {
    assert(params);
    assert(attr);
    assert(title);
    char *result=NULL;

    HTML_LOG(1," begin final link for params[%s] attr[%s] title[%s]",params,attr,title);

    char *url = final_url(params);
    HTML_LOG(1," end final link [%s]",url);

    ovs_asprintf(&result,"<a href=\"%s\" %s>%s</a>",url,attr,title);

    FREE(url);
    return result;
}



//void playlist_close() {
//    if (playlist_fp) {
//        fclose(playlist_fp);
//        playlist_fp=NULL;
//    }
//}

FILE *playlist_open() {
    static FILE *fp=NULL;
    static FILE *j=NULL;
    fflush(stdout);
    //HTML_LOG(1,"play list fp is %ld %ld %ld",k,fp,j);
    //exit(1);
    if (fp == NULL) {
        if (unlink(NMT_PLAYLIST) ) {
            HTML_LOG(1,"Failed to delete ["NMT_PLAYLIST"]");
        } else {
            HTML_LOG(1,"deleted ["NMT_PLAYLIST"]");
        }
        j = fp = fopen(NMT_PLAYLIST,"w");
    }
    assert(fp == j); //DONT ASK! ok catch corruption of static area - maybe...
    return fp;
}

char *share_name(DbItem *r,int *freeme) {
    char *out = NULL;
    *freeme = 0;
    if (is_on_remote_oversight(r) ) {
        out = r->db->source;
    } else if (util_starts_with(r->file,NETWORK_SHARE)) {
        char *p = r->file + strlen(NETWORK_SHARE);
        char *q = strchr(p,'/');
        if (q == NULL) {
            out = p;
        } else {
            out = COPY_STRING(q-p,p);
            *freeme = 1;
        }
    }
    return out;
}

char *add_network_icon(DbItem *r,char *text) {

    char *icon;
    char *result=NULL;

    if (is_on_local_storage(r)) {

        //icon =  get_theme_image_tag("harddisk"," width=20 height=15 ");
        //ovs_asprintf(&result,"%s %s",icon,text);
        //FREE(icon);

    } else {

        icon =  get_theme_image_tag("network"," width=20 height=15 ");
        ovs_asprintf(&result,"%s%s%s",icon,(text?" ":""),NVL(text));
        FREE(icon);

    }

    return result;

}

char *vod_attr(char *file) {

    if (is_dvd(file)) {
        return "file=c ZCD=2";
    } else {
        return "vod file=c";
    }
}


/*
 * Convert 
 *    href="blah"
 *
 *    to
 *
 *    href="javascript:if(confirm('prompt')) location.href='blah';"
 */
char *add_confirm_link(char *prompt,char *link) 
{
#define HREF "href=\""
    char *result = NULL;
    if (link) {
        char *href = strstr(link,HREF);
        char *href_end;
        if (href) {
            href += strlen(HREF);
            href_end = strchr(href,'"');
            if (href_end) {
                ovs_asprintf(&result,"%.*sjavascript:if (confirm('%s')) location.href='%.*s';%s",
                    href-link,link,
                    prompt,
                    href_end-href,href,
                    href_end);
            }
        }
    }
    if (result == NULL) result = STRDUP(link);
    return result;
}

//T2 just to avoid c string handling in calling functions!
char *vod_link(DbItem *rowid,char *title ,char *t2,
        char *source,char *file,char *href_name,char *href_attr,char *class){

    assert(title);
    assert(t2);
    assert(source);
    assert(file);
    assert(href_name);
    assert(href_attr);
    assert(class);

    char *vod=NULL;
    int freepath;

    int add_to_playlist= !is_dvd(rowid->file);
    char *result=NULL;

    HTML_LOG(1,"VOD file[%s]",file);
    char *path = get_path(rowid,file,&freepath);
    HTML_LOG(1,"VOD path[%s]",path);

    nmt_mount(path);

    char *encoded_path = url_encode(path);


    if (!g_dimension->local_browser && browsing_from_lan()) {

        if (*oversight_val("ovs_tv_play_via_pc") == '1') {
            //If using a browser then VOD tags dont work. Make this script load the file into gaya
            //Note we send the view and idlist parameters so that we can render the original page 
            //in the brower after the infomation is sent to gaya.

            //This works by adding a parameter REMOTE_VOD_PREFIX1=filename
            //The script than captures this after clicking via do_actions,
            //this sends a url to gaya which points back to this script again but will just contain
            //small text to auto load a file using <a onfocusload> and <body onloadset>
            char *params =NULL;
            char *attr;

            ovs_asprintf(&params,REMOTE_VOD_PREFIX1"=%s",encoded_path);
            //
            //ovs_asprintf(&params,"idlist=&"QUERY_PARAM_VIEW"=&"REMOTE_VOD_PREFIX1"=%s",encoded_path);
            //
            ovs_asprintf(&attr,"%s %s",class,NVL(href_attr));

            // result = get_final_link_with_font(params,attr,title,class);
            result = get_self_link_with_font(params,class,title,class); 
            
            char *tmp = add_confirm_link("play?",result);
            FREE(result);
            result = tmp;

            FREE(attr);
            FREE(params);

        } else {
            ovs_asprintf(&result,"<font class=\"%s\">%s</font>",class,title);
        }

    } else {

        ovs_asprintf(&vod," %s name=\"%s?1\" %s ",vod_attr(file),href_name,href_attr);

        if (add_to_playlist) {
            //Build playlist array for this entry.
            if (rowid->playlist_names == NULL) rowid->playlist_names = array_new(free);
            if (rowid->playlist_paths == NULL) rowid->playlist_paths = array_new(free);
            array_add(rowid->playlist_names,util_basename(file));
            array_add(rowid->playlist_paths,STRDUP(path));
        }


        if (!EMPTY_STR(class)) {

            ovs_asprintf(&result,"<a href=\"file://%s\" %s %s><font %s>%s%s</font></a>",
                    encoded_path,vod,class,class,title,t2);
        } else {
            ovs_asprintf(&result,"<a href=\"file://%s\" %s>%s%s</a>",
                    encoded_path,vod,title,t2);
        }
    }

    FREE(encoded_path);
    if (freepath) FREE(path);
    FREE(vod);

    return result;
}

// this is a link where existing p,view,idlist parameters are moved to @p,@view,@idlist 
// so  after following this link we have enough info to generate a link to return
char *get_drilldown_link_with_font(char *params,char *attr,char *title,char *font_attr)
{
    assert(params);
    assert(attr);
    assert(title);
    assert(font_attr);
    char *title2=NULL;

    ovs_asprintf(&title2,"<font %s>%s</font>",font_attr,title);
    char *result = drill_down_link(params,attr,title2);

    FREE(title2);
    return result;
}

// This is a link with all drill down parameters removed. This is for the final link
// to play the file.
char *get_final_link_with_font(char *params,char *attr,char *title,char *font_attr)
{
    assert(params);
    assert(attr);
    assert(title);
    assert(font_attr);
    char *title2=NULL;

    ovs_asprintf(&title2,"<font %s>%s</font>",font_attr,title);
    char *result = final_link(params,attr,title2);

    FREE(title2);
    return result;
}

char *get_self_link_with_font(char *params,char *attr,char *title,char *font_attr)
{
    assert(params);
    assert(attr);
    assert(title);
    assert(font_attr);
    char *title2=NULL;

    ovs_asprintf(&title2,"<font %s>%s</font>",font_attr,title);
    char *result = get_self_link(params,attr,title2);

    FREE(title2);
    return result;
}


void display_self_link(char *params,char *attr,char *title)
{
    char *tmp;
    tmp=get_self_link(params,attr,title);
    printf("%s",tmp);
    FREE(tmp);
}


char *get_remote_button(char *button_colour,char *params,char *text)
{

    assert(button_colour);
    assert(params);
    assert(text);

    char *params2;
    char *attr;
    char *text2;

    ovs_asprintf(&params2,"%s&colour=%s",params,button_colour);
    ovs_asprintf(&attr,"tvid=\"%s\"",button_colour);
    ovs_asprintf(&text2,"<font class=\"%sbutton\">%s</font>",button_colour,text);

    char *result = get_self_link(params2,attr,text2);

    FREE(params2);
    FREE(attr);
    FREE(text2);
    return result;
}


char *get_toggle(char *button_colour,char *param_name,char *v1,char *text1,char *v2,char *text2)
{

    assert(button_colour);
    assert(param_name);
    assert(v1);
    assert(text1);
    assert(v2);
    assert(text2);

    char *param_value;
    char *params;
    char *text;
    char *next = v1;
    int v1current = 0;
    int v2current = 0;

    param_value = query_val(param_name);

    if (!*param_value) {

        //next = v1;
        //v1current = v2current = 0;
        v2current = 1;
        next = v1;

    } else if (STRCMP(param_value,v1)==0) {

        v1current = 1;
        next = v2;

    } else if (STRCMP(param_value,v2)==0) {
            
        v2current = 1;
        next = v1;
        //next = "";
    }

    ovs_asprintf(&params,"p=0&%s=%s",param_name,next);

    HTML_LOG(1,"params = [%s]",params);

    ovs_asprintf(&text,"%s%s%s<br>%s%s%s",
            (v1current?"<u><b>":""),text1,(v1current?"</b></u>":""),
            (v2current?"<u><b>":""),text2,(v2current?"</b></u>":""));

    HTML_LOG(1,"toggle text = [%s]",text);

    char *result = get_remote_button(button_colour,params,text);

    FREE(params);
    FREE(text);
    return result;
}

//Add current named html parameter as a hidden value
char * add_hidden(char *name_list) {
    char *output="";
    Array *names = split(name_list,",",0);
    int i;
    for(i = 0 ; i < names->size ; i++ ) {

        char *name =names->array[i];
        char *val = query_val(name);

        if (*val) {

            char *tmp;
            ovs_asprintf(&tmp,"%s<input type=hidden name=\"%s\" value=\"%s\" >\n",output,name,val);
            if (*output) FREE(output);
            output = tmp;
        }
    }
    if (!*output) {
        return NULL;
    } else {
        return output;
    }
}

void display_submit(char *name,char *value) {
    assert(name);
    assert(value);
    printf("<input type=submit name=\"%s\" value=\"%s\">",name,value);
}

void display_confirm(char *name,char *val_ok,char *val_cancel) {
    printf("<table width=100%%><tr><td align=center>");
    display_submit(name,val_ok);
    printf("</td><td align=center>");
    display_submit(name,val_cancel);
    printf("</td></tr></table>");
}


void display_footer(
        ) {
    printf("Footer here");
}

/*
 * html code for an image link where url will drillup - ie remap @@param to @param
 * and @param to param.
 */
char *get_theme_image_return_link(char *href_attr,char *image_name,char *button_attr)
{
    assert(image_name);

    char  *tag=get_theme_image_tag(image_name,button_attr);
    char *result = return_link(href_attr,tag);
    FREE(tag);
    return result;
}
char *get_theme_image_link(char *qlist,char *href_attr,char *image_name,char *button_attr)
{
    assert(qlist);
    assert(image_name);

    char  *tag=get_theme_image_tag(image_name,button_attr);
    char *result = get_self_link(qlist,href_attr,tag);
    FREE(tag);
    return result;
}
void display_theme_image_link(char *qlist,char *href_attr,char *image_name,char *button_attr)
{

    assert(qlist);
    assert(image_name);

    char  *tag=get_theme_image_tag(image_name,button_attr);
    display_self_link(qlist,href_attr,tag);
    FREE(tag);
}

char *add_scroll_attributes(int left_scroll,int right_scroll,int selected_cell,char *attrin)
{
    char *attr;
    ovs_asprintf(&attr,
            " %s%s%s %s ",
            (selected_cell? "name=selectedCell ":""),
            (left_scroll? "onkeyleftset=pgup1 ":""),
            (right_scroll? "onkeyrightset=pgdn1 ":""),

            (attrin != NULL?attrin:""));

    return attr;
}

char *get_empty(char *width_attr,int grid_toggle,char *height_attr,int left_scroll,int right_scroll,int selected_cell) {

    char *attr;

    attr=add_scroll_attributes(left_scroll,right_scroll,selected_cell,NULL);

    char *result;

    ovs_asprintf(&result,"\t\t<td %s %s class=empty%d><a href=\"\" %s></a>\n",
            width_attr,height_attr,grid_toggle,attr);

    FREE(attr);
    return result;
}


static int use_file_to_url_symlink=0;
void create_file_to_url_symlink() {
    // Create a symlink that gives a remote browse simple access to all
    // files via http://ip:8883/...
    // By default remote browsers can only see /opt/httpd/default and
    // only gaya can see the other media mount points eg /opt/sybhttpd/localhost.drives
    // (becuase sybhttpd checks the headers sent back by the browser (see mini-installer)
    // This link allows /NETWORK_SHARE/path/to/file to be accessed as
    // http://:8883/.network/path/to/file
    //
    // This is a potential security risk so well make it optional
    // Without it - remote browsers  access mounted images by passing them as an argument
    // to oversight.cgi which then 'cats' them to stdout - see main()
    if (!exists(NETWORK_SYMLINK)) {
        symlink(NETWORK_SHARE,NETWORK_SYMLINK);
    }
    use_file_to_url_symlink=1;
}

// returns a url path - with quotes.
char *file_to_url(char *path) {
    char *new = NULL;
    char *prefix="";

    if (path) {
        if (g_dimension->local_browser) {
            //If using gaya just go directly to the file system
            prefix="file://";

        } else if (util_starts_with(path,"/share/Apps/oversight")) {
            // if /share/Apps/oversight/file/path 
            // then use /oversight/file/path thanks to symlink 
            //  /opt/sybhttpd/default/oversight -> /share/Apps/oversight/
            path += 11;

        } else if (util_starts_with(path,"/opt/sybhttpd/default")) {
            // if in /opt/sybhttpd/default/file/path
            // then use /file/path
            path +=21;

        } else if (use_file_to_url_symlink && util_starts_with(path,NETWORK_SHARE)) {

            prefix="/.network/";
            path += strlen(NETWORK_SHARE);

        } else {
            // otherwise pass as a paramter to this script. It will cat jpg etc it to stdout
            prefix="?";
        }

        char *non_empty = path;
        // Gaya doesnt like empty paths
        if (strstr(path,"//")) {
            non_empty = replace_str(path,"//","/");
        }

        int free_path2;
        char *encoded_path = url_encode_static(non_empty,&free_path2);



        ovs_asprintf(&new,"%s%s",prefix,encoded_path);

        if (non_empty != path) FREE(non_empty);
        if (free_path2) FREE(encoded_path);
    }

    return new;

}

// Return html img tag to a template image. The url will point to the skin or fail back to default.
char * template_image_link(char *subfolder,char *name,char *ext,char *alt_text,char *attr)
{
    char *url = image_source(subfolder,name,ext);

    char *link=NULL;

    if (url) {
        ovs_asprintf(&link,"<img src=%s alt=\"%s\" %s \\>",
                url,
                (alt_text?alt_text:name),
                NVL(attr));

        FREE(url);
    }
    return link;
}

// Used for poster and fanart links.
char * get_local_image_link(char *path,char *alt_text,char *attr) {

    assert(alt_text);
    assert(attr);

    char *result;


    if (path == NULL || !exists(path) ) {
        result = STRDUP(alt_text);
        HTML_LOG(0,"%s doesnt exist",path);
    } else {
        char *img_src = file_to_url(path);

        ovs_asprintf(&result,"<img alt=\"%s\" src=\"%s\" %s >",alt_text,img_src,attr);

        FREE(img_src);
    }
    return result;
}


char *category(char cat) {
    switch(cat) {
        case 'T' : return "tv";
        case 'M' : return "movie";
        default : return "video";
    }
}

char *file_style_custom(DbItem *rowid,char *modifier) {

    static char grid_class[50],c='\0';

    if (g_dimension->poster_mode) {
        return " class=unwatched ";
    } else {

        /* Plan to depricate this one day */
        sprintf(grid_class," class=grid_%s%s%s ",
                category(rowid->category),
                (rowid->watched?"_watched":""),
                modifier);
        assert(c == 0);
    }

    return grid_class;
}
char *file_style(DbItem *rowid) {
    return file_style_custom(rowid,"");
}
char *file_style_small(DbItem *rowid) {
    return file_style_custom(rowid,"_small");
}

// Item is marked watched if all linked rows are watched.
int is_watched(DbItem *rowid) {
    int result=1;
    for( ; rowid ; rowid = rowid->linked ) {
        if (rowid->watched == 0) {
            result=0;
            break;
        }
    }
    return result;
}



int is_fresh(DbItem *rowid) {
    int result=0;
    static long fresh_time = -1;

    if (fresh_time == -1 ) {

        long fresh_days;
        fresh_time = 0;
        if (config_check_long(g_oversight_config,"ovs_new_days",&fresh_days)) {
            fresh_time = time(NULL) -fresh_days*24*60*60 ;
        } else {
            fresh_time = 0;
        }
    }

    // Item is marked fresh if any row is fresh
    if (fresh_time > 0) {
        result = (internal_time2epoc(*timestamp_ptr(rowid)) > fresh_time);
    }
    return result;
}

char *watched_style(DbItem *rowid) {

    if (is_watched(rowid)) {
        return " class=watched ";
    } else if (is_fresh(rowid) ) {
        return " class=fresh ";
    } else { 
        return file_style(rowid);
    }
}
char *watched_style_small(DbItem *rowid) {

    if (is_watched(rowid)) {
        return " class=watched_small ";
    } else if (is_fresh(rowid) ) {
        return " class=fresh_small ";
    } else { 
        return file_style_small(rowid);
    }
}

char *check_path(char *format, ... ) {
    char *p;
    va_list args;
    va_start(args,format);
    ovs_vasprintf(&p,format,args);

    va_end(args);

    if (!exists(p)) {
        HTML_LOG(1,"%s doesnt exist",p);
        FREE(p);
        p = NULL;
    } else {
        HTML_LOG(1,"%s exist",p);
    }
    return p;
}

/**
 * Get actor path.
 * A DbItem row must be passed as this will contain infomation to find the correct
 * oversight database. If using crossview.
 */
char *actor_image_path(DbItem *item,char *name_id)
{
    int free_path;

    char *logical_path;
    ovs_asprintf(&logical_path,"ovs:%s/%s%s.jpg",
            DB_FLDID_ACTOR_LIST,
            catalog_val("catalog_poster_prefix"),
            name_id);

    char *result = get_path(item,logical_path,&free_path);
    if (free_path) {
        FREE(logical_path);
    }

    return result;

}

char *internal_image_path_static(DbItem *item,ImageType image_type)
{
    // No pictures on filesystem - look in db
    // first build the name 
    // TV shows =
    // ovs:fieldid/prefix tt0000000 _ season.jpg
    // ovs:fieldid/prefix title _ year _ season.jpg
    //
    // films  =
    // ovs:fieldid/prefix tt0000000.jpg
    // ovs:fieldid/prefix title _ year.jpg
    //
#define INTERNAL_IMAGE_PATH_LEN 250
    static char path[INTERNAL_IMAGE_PATH_LEN+1];
    path[INTERNAL_IMAGE_PATH_LEN] = '\0';
    char *p = path;

    p += sprintf(p,"ovs:%s/%s",
            (image_type == FANART_IMAGE?DB_FLDID_FANART:DB_FLDID_POSTER),
            catalog_val("catalog_poster_prefix"));

    char *id=get_item_id(item,"imdb",0);
    if (id) {
        p += sprintf(p,"imdb_%s",id);
        FREE(id);
    } else {
            // Title - year
            // TODO Cope with UTF-8 Normalised/Expanded titles.


        char *t=item->title;

        if (t) {
            // Add title replacing all runs of non-alnum with single _
            int first_nonalnum = 1;
            while(*t) {
                if (isalnum(*t) || strchr("-_&",*t) || *t < 0 /* utf-8 high bit */ ) {
                    *p++ = *t;
                    first_nonalnum=1;
                } else if (first_nonalnum) {
                    *p++ = '_';
                    first_nonalnum=0;
                }
                t++;
            }
        }
        *p++ = '_';
        if (item->year) {
            p += sprintf(p,"%d",item->year);
        }
    }
    if (item->category == 'T' ) {
        p+= sprintf(p,"_%d",item->season);
    } 
    p += sprintf(p,".jpg");

    HTML_LOG(2,"internal_image_path_static [%s] = [%s]",item->title,path);
    assert(path[INTERNAL_IMAGE_PATH_LEN] == '\0');
    return path;
}

char *get_internal_image_path_any_season(int num_rows,DbItem **sorted_rows,ImageType image_type,ViewMode *newview);
char *get_existing_internal_image_path(DbItem *item,ImageType image_type,ViewMode *newview);

char *get_picture_path(int num_rows,DbItem **sorted_rows,ImageType image_type,ViewMode *newview)
{

    char *path = NULL;
    DbItem *item = sorted_rows[0];
    char *suffix ;     // given movie.avi look for movie.suffix.(jpg|png)
    char*default_name; // given movie.avi look for default_name.(jpg|png)

    char *ext[] = { ".jpg" , ".png" , NULL };
    int i;

    if (image_type == FANART_IMAGE) {
        suffix=".fanart";
        default_name="fanart";
    } else {
        suffix="";
        default_name="poster";
    }

    int freefile;
    // First check the filesystem. We do this via the mounted path.
    // This requires that the remote file is already mounted.
    char *file = get_path(item,item->file,&freefile);
TRACE;
    char *dir=NULL,*base=NULL;
    if (is_dvd_folder(file)) {

        // file=/some/path/file/


        // dir=/some/path/file
        dir = STRDUP(file);
        char *last = strrchr(dir,'/');
        if (last) *last = '\0';

        // base=/some/path/file
        last = strrchr(dir,'/');
        if (last) base = STRDUP(last+1);

    } else {
        // file=some/path/file.avi
        
        // dir=/some/path
        dir = util_dirname(file);

        // base=file.avi
        base = util_basename(file);
        // Find position of file extension.
        char *dot = NULL;

        // First look for file.modifier.jpg file.modifier.png
        if (item->ext != NULL) { 
            dot = strrchr(base,'.');
        }
        if (dot) {
            *dot = '\0';
        }
    }

    //check movie.fanart.(png|jpg) or movie.(png|jpg)

    for(i=0 ; path==NULL && ext[i] ; i++ ) {
        path=check_path("%s/%s%s%s",dir,base,suffix,ext[i]);
    }

    //check default_name.(png|jpg) ie. fanart.(png|jpg) or poster.(png|jpg)
    for(i=0 ; path==NULL && ext[i] ; i++ ) {
        path=check_path("%s/%s%s",dir,default_name,ext[i]);
    }
    FREE(dir);
    FREE(base);

    if (freefile) FREE(file);

    if (path == NULL) {

        // look in internal db
        path = get_internal_image_path_any_season(num_rows,sorted_rows,image_type,newview);
    }

    return path;
}

/**
 * If looking at the box set view try each season in turn
 */
char *get_internal_image_path_any_season(int num_rows,DbItem **sorted_rows,ImageType image_type,ViewMode *newview)
{

    int i;
    int season=-2;

    char * path = NULL;
    // Find first item that has some kind of season info - even movie where season=0 or -1
    for ( i= 0 ; path == NULL && i < num_rows ; i++ ) {
        if (season == -2 || sorted_rows[i]->season != season) {
            season = sorted_rows[i]->season;
            path = get_existing_internal_image_path(sorted_rows[i],image_type,newview);
        }
    }
    return path;
}

/*
 * Get the full internal image path if it exists.
 */
char *get_existing_internal_image_path(DbItem *item,ImageType image_type,ViewMode *newview)
{
    char *path = internal_image_path_static(item,image_type);

TRACE;
    if (path) {
        int freepath=0;
        path = get_path(item,path,&freepath);

        if (image_type == FANART_IMAGE ) {
            char *modifier=IMAGE_EXT_HD;

            if (g_dimension->scanlines == 0 ) {

                if (g_dimension->is_pal ) {
                    modifier=IMAGE_EXT_PAL;
                } else {
                    modifier=IMAGE_EXT_SD;
                }
            }
            char *tmp = util_change_extension(path,modifier);
            if(freepath) FREE(path);
            path = tmp;

        } else if (image_type == THUMB_IMAGE ) {

            char *ext_list[] = { IMAGE_EXT_THUMB_BOXSET, IMAGE_EXT_THUMB , NULL };
            int start_index = 1;

            if (newview->view_class == VIEW_CLASS_BOXSET) {
                start_index=0;
            }

            char *ext;
            int i;
            int found=0;
            for(i = start_index ; (ext = ext_list[i] ) != NULL ; i++ ) {
                char *tmp = util_change_extension(path,ext);
                if (exists(tmp)) {
                    if(freepath) FREE(path);
                    path = tmp;
                    found = 1;
                    break;
                } else {
                    FREE(tmp);
                }
            }
            if (!found) {
                // Use the original file
                if(!freepath && path) {
                    path=STRDUP(path);
                }
            }
        }
        if(!exists(path) ) {
            HTML_LOG(1,"[%s] doesnt exist",path);
            FREE(path);
            path=NULL;
        }
    }
    return path;
}


char * get_poster_image_tag(DbItem *rowid,char *attr,ImageType image_type,ViewMode *newview)
{

    assert(rowid);
    assert(attr);
TRACE;
    char *result = NULL;
    
    char *path = get_picture_path(1,&rowid,image_type,newview);
    if (path) {

        result = get_local_image_link(path,rowid->title,attr);

        FREE(path);
    }
    return result;
}

char *ovs_icon_type() {
    static char *icon_type = NULL;
    static char *i=NULL;
    if (icon_type == NULL) {
        if (!config_check_str(g_oversight_config,"ovs_icon_type",&icon_type)) {
            icon_type="png";
        }
        HTML_LOG(1,"icon type = %s",icon_type);
        i = icon_type;
    }
    assert(icon_type == i );
    return icon_type;
}

char *container_icon(char *image_name) {
    char *result = template_image_link("",image_name,NULL,NULL,"class=\"codec\"");
    return result;
}

//free result
char *icon_link(char *name) {

    char *result=NULL;

    if (name) {
        if (name[strlen(name)-1] == '/') {
            result = container_icon("video_ts");
        } else {
            //char *ext = name + strlen(name) - 5;
            char *ext = strrchr(name,'.');
            if (ext != NULL) {
                ext++;
                char *p;
                if ((p = strstr("|iso|img|mkv|avi|",ext)) != NULL && p[-1] == '|' && p[strlen(ext)] == '|' ) {
                    result = container_icon(ext);
                } else if (strcasecmp(ext,"avi") != 0) {
                    ovs_asprintf(&result,"<font size=\"-1\">[%s]</font>",ext);
                }
            }
        }
    }
    return result;
}



char *build_ext_list(DbItem *row_id) {

    HTML_LOG(3,"ext=%s",row_id->ext);
    char *ext_icons = icon_link(row_id->ext);
    HTML_LOG(3,"ext_icons=%s",ext_icons);

    DbItem *ri;
    for( ri = row_id->linked ; ri ; ri=ri->linked ) {
        if (ri->ext && (ext_icons==NULL || strstr(ext_icons,ri->ext) == NULL)) {
            char *new_ext;
            char *linked_icon = icon_link(ri->ext);
            if (linked_icon) {
                ovs_asprintf(&new_ext,"%s%s",
                        (ext_icons?ext_icons:""),
                        (linked_icon?linked_icon:""));
                FREE(linked_icon);
                FREE(ext_icons);
                ext_icons = new_ext;
            }
        }
    }
    return ext_icons;
}

char *add_one_source_to_idlist(DbItem *row_id,char *current_idlist,int *mixed_sources) {

    DbItem *ri;
    char *idlist=NULL;
    assert(row_id);
    assert(row_id->db);
    assert(row_id->db->source);

    ovs_asprintf(&idlist,"%s%s(%ld",
            NVL(current_idlist),
            row_id->db->source,row_id->id);

    FREE(current_idlist);

    if (mixed_sources) *mixed_sources=0;

    if (row_id->link_count == 0) {
        DbItem *r;
        for(r = row_id->linked ; r ; r=r->linked) {
            row_id->link_count++;
        }
    }

    int len = 10+10 * (1+row_id->link_count);
    char *out = MALLOC(len);

    char *p = out;

    *p = '\0';

    for( ri = row_id->linked ; ri ; ri=ri->linked ) {
        if (ri->db == row_id->db) {

            p += sprintf(p,"|%ld",ri->id);
        } else {
            if (mixed_sources) *mixed_sources=1;
        }
    }
    assert(p < out+len);

    ovs_asprintf(&p,"%s%s)",idlist,out);
    FREE(idlist);
    FREE(out);

    idlist = p;

    return idlist;
}


/*
 * Go through all Ids that are linked and create an id list of the form.
 * source(id1|id2|id3)source2(id4|id5|id6)
 * In the menu view all rows with the same season are linked.
 * in the tv view , rows with the same file are linked.
 * Dont free result - this will be freed when rows are freed.
 */
char *build_id_list(DbItem *row_id)
{

    int mixed_sources=0;
    char *idlist=NULL;
    assert(row_id);
    assert(row_id->db);
    assert(row_id->db->source);

    if (row_id->idlist == NULL) {
        idlist = add_one_source_to_idlist(row_id,NULL,&mixed_sources);

        idlist[strlen(idlist)-1] = ')';

        if (mixed_sources) {
            // Add rows from other sources. This could be merged with the loop above
            // but to help performance we only track sources if we need to as the loop
            // performance is O(n^2)
            struct hashtable *sources = string_string_hashtable("db_sources",4);
            hashtable_insert(sources,row_id->db->source,"1");
            DbItem *ri;

            for( ri = row_id->linked ; ri ; ri=ri->linked ) {
                if (hashtable_search(sources,ri->db->source) == NULL) {
                    hashtable_insert(sources,ri->db->source,"1");
                    idlist = add_one_source_to_idlist(ri,idlist,&mixed_sources);
                }
            }
            hashtable_destroy(sources,0,0);
        }
        row_id->idlist = idlist;
    }

    return row_id->idlist;
}

#define MAX_TITLE_LEN 50
char *trim_title(char *title) {
    char *out = STRDUP(title);
    if (strlen(out) > MAX_TITLE_LEN) {
        strcpy(out+MAX_TITLE_LEN-3,"..");
    }
    return out;
}


char *select_checkbox(DbItem *item,char *text) {
    char *result = NULL;
    char *select = query_select_val();

    if (*select) {

        g_item_count ++;

        char *id_list = build_id_list(item);

        int checked = 0;
        char *disabled = "";
        char *disabled_image="";

        if (STRCMP(select,FORM_PARAM_SELECT_VALUE_MARK) == 0) {

            checked = item->watched;

        } else if (STRCMP(select,FORM_PARAM_SELECT_VALUE_LOCK) == 0) {

            checked = is_locked(item);

        } else {
            // delist delete
            if (is_locked(item)) {
                disabled=" disabled ";
                disabled_image=get_theme_image_tag("security"," width=\"20px\" height=\"20px\" ");
            }
        }

        if (*disabled) {

            ovs_asprintf(&result,

                "%s<br><font class=%s>%s</font>",
                    disabled_image,
                    select,text);

        } else if (checked) {

            ovs_asprintf(&result,
                "<input type=checkbox name=\""CHECKBOX_PREFIX"%s\" CHECKED %s >%s"
                "<input type=hidden name=\"orig_"CHECKBOX_PREFIX"%s\" value=on>"
                "<font class=%s>%s</font>",
                    id_list,
                    disabled,
                    disabled_image,
                    id_list,
                    select,text);
        } else {

            ovs_asprintf(&result,
                "<input type=checkbox name=\""CHECKBOX_PREFIX"%s\" %s >%s"
                "<font class=%s>%s</font>",
                    id_list,
                    disabled,
                    disabled_image,
                    select,text);
        }
    } else {
        ovs_asprintf(&result,"<font class=Ignore>%s</font>",text);
    }
    return result;
}


#define JS_MOVIE_INFO_FN_PREFIX "set_info"
void add_movie_info_text(Array *js,long fn_id,char *info)
{
    char *text = clean_js_string(info);
    char *tmp;
    ovs_asprintf(&tmp,"\nfunction " JS_MOVIE_INFO_FN_PREFIX "%x() { set_info('%s'); }\n",fn_id,text);
    array_add(js,tmp);
    if (text != info) FREE(text);
}
void add_movie_part_row(Array *output,long fn_id,char *cell)
{
    char *tmp;
    char *focus=td_mouse_event_fn(JS_MOVIE_INFO_FN_PREFIX,fn_id);
    ovs_asprintf(&tmp,"\n<tr><td %s>%s</td></tr>\n",NVL(focus),cell);
    FREE(focus);
    array_add(output,tmp);
}

char *movie_listing(DbItem *rowid)
{
    int show_names;
   
    show_names = *oversight_val("ovs_movie_filename") == '1';


    db_rowid_dump(rowid);

    char *js_title = clean_js_string(rowid->title);

    printf("<script type=\"text/javascript\"><!--\ng_title='%s';\ng_idlist='%s';\n--></script>\n",
            js_title,build_id_list(rowid));

    if (js_title != rowid->title) FREE(js_title);


    char *select = query_select_val();
    char *style = watched_style(rowid);
    if (*select) {
        return select_checkbox(rowid,rowid->file);
    } else {
        Array *js = array_new(free);
        Array *output = array_new(free);

        Array *parts = split(rowid->parts,"/",0);
        HTML_LOG(1,"parts ptr = %ld",parts);

        char *label;

        if (show_names) {
            label = file_name(rowid->file);
        } else {
            if (parts && parts->size) {
                label = STRDUP("part 1");
            } else {
                label = STRDUP("movie");
            }
        }

        char *mouse=href_focus_event_fn(JS_MOVIE_INFO_FN_PREFIX,0);
        char *href_attr;
        ovs_asprintf(&href_attr,"onkeyleftset=up %s",NVL(mouse));
        add_movie_info_text(js,0,rowid->file);
        char *vod = vod_link(rowid,label,"",rowid->db->source,rowid->file,"0",href_attr,style);
        add_movie_part_row(output,0,vod);

        FREE(label);
        FREE(mouse);
        FREE(href_attr);

        // Add vod links for all of the parts
        
        if (parts && parts->size) {

            int i;
            for(i = 0 ; i < parts->size ; i++ ) {

                char i_str[10];
                sprintf(i_str,"%d",i);

                char *mouse=href_focus_event_fn(JS_MOVIE_INFO_FN_PREFIX,i+1);

HTML_LOG(0,"mouse[%s]",mouse);
                char *label;


                if (show_names) {
                    ovs_asprintf(&label,parts->array[i]);
                } else {
                    ovs_asprintf(&label,"part %d",i+2);
                }
                char *vod = vod_link(rowid,label,"",rowid->db->source,parts->array[i],i_str,NVL(mouse),style);
                FREE(label);

                add_movie_part_row(output,i+1,vod);
                add_movie_info_text(js,i+1,parts->array[i]);

                FREE(mouse);

            }
        }


        // Big play button
        char *play_button = get_theme_image_tag("player_play","");
        char *play_tvid;
        if (is_dvd(rowid->file)) {
            // DVDs are not added to the play list. So the play button just plays the dvd directly
            play_tvid = vod_link(rowid,play_button,"",rowid->db->source,rowid->file,"1","",style);
        } else {
            play_tvid = get_play_tvid(play_button);
        }

        char *vod_list;
        char *js_script = arraystr(js);
        char *result = arraystr(output);

        ovs_asprintf(&vod_list,
            "<script type=\"text/javascript\"><!--\n\n%s\n\n--></script>\n"
            "<table><tr><td>%s</td>"
            "<td><table>%s</table></td></table>",
            js_script,play_tvid,result);

        FREE(result);
        result = vod_list;
        FREE(js_script);
        FREE(play_button);
        FREE(play_tvid);

        array_free(output);
        array_free(js);

        return result;
    }
}


// Count number of unique seasons in the list.
int season_count(DbItem *item) {

#define MAX_SEASON 200
#define WORDBITS 16
#define WORDS ((MAX_SEASON/WORDBITS)+1)

    if (item->num_seasons == 0) {
        // First push seasons into a set (bits)
        unsigned int i=0;
        unsigned int j=0;
        unsigned long bitmask[WORDS+1];
        memset(bitmask,0,WORDS * sizeof(long));

        DbItem *item2;
        for( item2 = item  ; item2 ; item2=item2->linked) {
            if (item2->category == 'T') {
                i=item2->season / WORDBITS;
                j=item2->season % WORDBITS;
                bitmask[i] |= (1 << (j-1) ); // allow for season 0 - prequels - pilots.
            }
        }


        // Now count total bits set.
        int total=0;
        for(i=0 ; i < WORDS ; i++ ) {
            for(j=1<<(WORDBITS-1) ; j ; j = j >> 1 ) {
               if (bitmask[i] & j ) total++;
            }
        }
    
        item->num_seasons = total;
    }
    return item->num_seasons;
}

int group_count(DbItem *item) {
    int i=0;
    for(  ; item ; item=item->linked) {
        i++;
    }
    return i;
}

void get_watched_counts(DbItem *item,int *watchedp,int *unwatchedp) 
{
    int watched=0;
    int unwatched=0;
    for(  ; item ; item=item->linked) {
        if (item->watched ) {
            watched++;
        } else {
            unwatched++;
        }
    }
    if (watchedp) *watchedp = watched;
    if (unwatchedp) *unwatchedp = unwatched;
}
    
int unwatched_count(DbItem *item) {
    int i=0;
    get_watched_counts(item,NULL,&i);
    return i;
}

int watched_count(DbItem *item) {
    int i=0;
    get_watched_counts(item,&i,NULL);
    return i;
}

typedef enum { WATCHED , NORMAL , FRESH } ViewStatus;

int get_view_status(DbItem *rowid) {
    ViewStatus status = NORMAL;
    if (is_watched(rowid)) {
        status = WATCHED;
    } else if (is_fresh(rowid)) {
        status = FRESH;
    }
    return status;
}

int is_locked(DbItem *item)
{
    return item->locked;
}

char *get_poster_mode_item(DbItem *row_id,char **font_class,char **grid_class,ViewMode *newview) {

    char *title = NULL;
    HTML_LOG(2,"dbg: tv or movie : set details as jpg");
    ViewStatus status = get_view_status(row_id);



    // *font_class and *grid_class are returned to the caller to set the <a><font> class and the <td>
    switch(status) {
        case NORMAL:  *grid_class = "class=poster"; break;
        case FRESH:   *grid_class = "class=poster_fresh"; break;
        case WATCHED: *grid_class = "class=poster_watched"; break;
        default:
             assert(0);
    }
    *font_class = *grid_class;


#if 0
    char *attr;
    // The class is reused here to set the image tag
    // They just happen to have the same name - maybe there is a more css friendly way to do this!
    ovs_asprintf(&attr," width=%d height=%d %s ",
        g_dimension->current_grid->img_width,
        g_dimension->current_grid->img_height,
        *font_class);

    title = get_poster_image_tag(row_id,attr,THUMB_IMAGE);
    FREE(attr);
#else
    title = get_poster_image_tag(row_id,*font_class,THUMB_IMAGE,newview);
#endif

TRACE;
    return title;
}

char *get_poster_mode_item_unknown(DbItem *row_id,char **font_class,char **grid_class) {
    HTML_LOG(2,"dbg: unclassified : set details as title");
    // Unclassified

    char *title;
    title = row_id->title;
    if (title != NULL) {
        title = STRDUP(title);
    } else {
        title = util_basename(row_id->file);
    }

    if (strlen(title) > 20) {
        strcpy(title+18,"..");
    }
    if (is_watched(row_id)) {
        *grid_class = "class=poster_watched_unknown";
    } else if (is_fresh(row_id)) {
        *grid_class = "class=poster_fresh_unknown";
    } else {
        *grid_class = "class=poster_unknown";
    }
    *font_class = watched_style_small(row_id);
    return title;
}

char *get_text_mode_item(DbItem *row_id,char **font_class,char **grid_class,ViewMode *newview) {

    // TEXT MODE
    HTML_LOG(2,"dbg: get text mode details ");

    *font_class = watched_style(row_id);
    *grid_class = file_style(row_id);

    char *title = trim_title(row_id->title);
   
    char *tmp;
    if (newview->view_class == VIEW_CLASS_BOXSET) {
        if (strcmp(newview->name,"tvboxset") == 0) {

            ovs_asprintf(&tmp,"%s [%d Seasons]",title,season_count(row_id));
            FREE(title);
            title = tmp;

        } else {

            ovs_asprintf(&tmp,"[%s Boxset]",title);
            FREE(title);
            title = tmp;
        }

    } else {

       char *cert = row_id->certificate;
       if ((tmp=strchr(cert,':')) != NULL) {
           if (tmp[1] != '\0') {
               ovs_asprintf(&cert,"(%s)",tmp+1);
           } else {
               cert = NULL;
           }
       }

        if (row_id->category == 'T' && row_id->season >= 1) {
            //Add season
            char *tmp;
            ovs_asprintf(&tmp,"%s S%d",title,row_id->season);
            FREE(title);
            title=tmp;
        }

        HTML_LOG(2,"dbg: add certificate");
        //Add certificate and extension
        char *tmp;
        char *ext_icons=build_ext_list(row_id);
        HTML_LOG(2,"dbg: add extension [%s]",ext_icons);

        ovs_asprintf(&tmp,"%s %s %s",
                title,
                (cert?cert:""),
                (ext_icons?ext_icons:""));

        FREE(title);
        title=tmp;
        if (cert != row_id->certificate) FREE(cert);
        FREE(ext_icons);


        if (row_id->category == 'T') {
            HTML_LOG(2,"dbg: add episode count");
            //Add episode count

            int unwatched = unwatched_count(row_id);

            if (unwatched) {
                char *tmp;
                int total = group_count(row_id);
                ovs_asprintf(&tmp,"%s&nbsp;<font color=#AAFFFF size=-1>x%d of %d</font>",title,unwatched,total);
                FREE(title);
                title=tmp;
            }
        }

        long crossview=0;
        config_check_long(g_oversight_config,"ovs_crossview",&crossview);
        if (crossview == 1 && is_on_remote_oversight(row_id)) {
            HTML_LOG(2,"dbg: add network icon");
           char *tmp =add_network_icon(row_id,title);
           FREE(title);
           title = tmp;
        }
    }
    HTML_LOG(0,"title[%s] newview[%s] final title[%s]",row_id->title,newview,title);

    return title;
}


char *mouse_or_focus_event_fn(char *function_name_prefix,long function_id,char *on_event,char *off_event) {
    char *result = NULL;
    ovs_asprintf(&result," %s=\"%s%lx();\" %s=\"%s0();\"",
            on_event,function_name_prefix,function_id,
            off_event,function_name_prefix);
    return result;
}

// These are attributes of the href
char *href_focus_event_fn(char *function_name_prefix,long function_id)
{
    if (g_dimension->local_browser) {
        return mouse_or_focus_event_fn(function_name_prefix,function_id,"onfocus","onblur");
    } else {
        return NULL;
    }
}

// These are attributes of the cell text
char *td_mouse_event_fn(char *function_name_prefix,long function_id)
{
    if (g_dimension->local_browser) {
        return NULL; 
    } else {
        return mouse_or_focus_event_fn(function_name_prefix,function_id,"onmouseover","onmouseout");
    }
}

char *get_item(int cell_no,DbItem *row_id,int grid_toggle,char *width_attr,char *height_attr,
        int left_scroll,int right_scroll,int selected_cell,int select_mode)
{

    //TODO:Highlight matched bit
    HTML_LOG(2,"Item %d = %s %s %s",cell_no,row_id->db->source,row_id->title,row_id->file);

    char cell_no_txt[9];
    sprintf(cell_no_txt,"%d",cell_no);

    char *title=NULL;
    char *font_class="";
    char *grid_class="";

    char *cell_background_image=NULL;
    int displaying_text;

    //Gaya has a navigation bug in which highlighting sometimes misbehaves on links 
    //with multi-lines of text. This was not a problem until the javascript title display
    //was introduced. When the bug triggers all elements become unfocussed causing
    //navigation position to be lost. 
    //To circumvent bug - only the first word of the link is highlighted.
    char *first_space=NULL;
    int link_first_word_only = g_dimension->local_browser && g_dimension->title_bar;

    get_drilldown_view(row_id);
    ViewMode *newview = row_id->drilldown_view;

    if (IN_POSTER_MODE) {

        displaying_text=0;

        if ((title = get_poster_mode_item(row_id,&font_class,&grid_class,newview)) != NULL) {

            if (*title != '<' && !util_starts_with(title,"<img")) {
                displaying_text=1;
                first_space = strchr(title,' ');
            }

        } else {
            title = get_poster_mode_item_unknown(row_id,&font_class,&grid_class);
            displaying_text=1;
        }
        if (displaying_text) {

            if (link_first_word_only) {
                //
                //Reduce amount of text in link - to fix gaya navigation
                first_space = strchr(title,' ');
            }

            // Display alternate image - this has to be a cell background image
            // so ewe can overlay text on it. as NTM does not have relative positioning
            // the alternative is to render the page and then use javascript to inspect
            // the cell coordinates and then overlay the text. yuk
            switch (row_id->category) {
                case 'T':
                    cell_background_image=icon_source("tv"); break;
                case 'M':
                case 'F':
                    cell_background_image=icon_source("video"); break;
                default:
                    cell_background_image=icon_source("video"); break;
            }
        }

    } else {
        displaying_text=1;

        title = get_text_mode_item(row_id,&font_class,&grid_class,newview);
    }
    if (first_space) {
        // Truncate even more if the first space does not occur early enough in the title.
        if (first_space - title > 11 ) {
            first_space = title+11;
        }
        *first_space='\0';
    }
    char *cell_text=NULL;
    char *focus_ev = NULL;
    char *mouse_ev = NULL;

    if (g_dimension->title_bar && !select_mode) {

        focus_ev = href_focus_event_fn(JAVASCRIPT_MENU_FUNCTION_PREFIX,cell_no+1);
        mouse_ev = td_mouse_event_fn(JAVASCRIPT_MENU_FUNCTION_PREFIX,cell_no+1);
    }



    if (select_mode) {

        cell_text = select_checkbox(row_id,NVL(title));

    } else {

        char *title_change_attr;
        ovs_asprintf(&title_change_attr," %s %s" ,(grid_class?grid_class:""), NVL(focus_ev));


        char *attr = add_scroll_attributes(left_scroll,right_scroll,selected_cell,title_change_attr);
        FREE(title_change_attr);
        HTML_LOG(1,"dbg: scroll attributes [%s]",attr);


        switch(row_id->drilldown_view->row_select) {
            case ROW_BY_SEASON:
                cell_text = get_tv_drilldown_link(newview,row_id->title,row_id->season,attr,title,font_class,cell_no_txt);
                break;
            case ROW_BY_TITLE:
                cell_text = get_tvboxset_drilldown_link(newview,row_id->title,attr,title,font_class,cell_no_txt);
                break;
            default:
                cell_text = get_movie_drilldown_link(newview,build_id_list(row_id),attr,title,font_class,cell_no_txt);
        }
        FREE(attr);
    }

    // Add a horizontal image to stop cell shrinkage.
    char *add_spacer = "";
    if (IN_POSTER_MODE && displaying_text) {
        // NMT browser seems to collapse width of table cells that do no contain poster images.
        // It seems to ignore most common CSS to fix the width.
        // This problem occurs if an entire column has no posters. (set rows=1 to make this happen more often).
        // There may be a nice way to fix with css but in the mean time I'll use an image.
        //
        // I does work if the image doesnt exists but this slows the NMT browser down.
        char *img = get_theme_image_tag("1000x1",width_attr);
        ovs_asprintf(&add_spacer,"<br>%s",img);
        FREE(img);
    }

    char *result;

    char *background_prefix = "";
    char *background_suffix = "";
    if (cell_background_image) {
        background_prefix = "background=\"";
        background_suffix = "\"";
    }

    ovs_asprintf(&result,"\t<td %s%s%s class=grid%d %s >%s%s%s%s</td>\n",
            background_prefix,NVL(cell_background_image),background_suffix,
            //NVL(width_attr),
            //NVL(height_attr),
            grid_toggle,
            NVL(mouse_ev),
            
            cell_text,
            (first_space?" ":""),
            (first_space?first_space+1:""),
            add_spacer);

    if (!EMPTY_STR(add_spacer)) FREE(add_spacer);
    FREE(mouse_ev);
    FREE(focus_ev);
    if (!EMPTY_STR(cell_background_image)) FREE(cell_background_image);

    FREE(cell_text);
    FREE(title); // first_space points inside of title
    return result;
}

char *get_tv_drilldown_link(ViewMode *view,char *name,int season,char *attr,char *title,char *font_class,char *cell_no_txt)
{
    char *result = NULL;
    static char *link_template = NULL;
    if (link_template == NULL ) {

        // Note the Selected parameter is added with a preceding @. This ensures that it is present in the 
        // return link. 
        link_template = get_drilldown_link_with_font(
            QUERY_PARAM_VIEW "=@VIEW@&p=&"QUERY_PARAM_TITLE_FILTER"="QPARAM_FILTER_EQUALS QPARAM_FILTER_STRING "@NAME@&"QUERY_PARAM_SEASON"=@SEASON@&@CELLNO_PARAM@=@CELLNO@",
            "@ATTR@","@TITLE@","@FONT_CLASS@");
    }
    char season_txt[9];
    sprintf(season_txt,"%d",season);

    int free_name2;
    char *name2 = url_encode_static(name,&free_name2);

    result = replace_all_str(link_template,
            "@VIEW@",view->name,
            "@NAME@",name2,
            "@SEASON@",season_txt,
            "@ATTR@",attr,
            "@TITLE@",title,
            "@CELLNO_PARAM@",get_drilldown_name_static(QUERY_PARAM_SELECTED,1),
            "@CELLNO@",cell_no_txt,
            "@FONT_CLASS@",font_class,
            NULL);

    if (free_name2) FREE(name2);
    return result;
}

char *get_tvboxset_drilldown_link(ViewMode *view,char *name,char *attr,char *title,char *font_class,char *cell_no_txt)
{
    char *result = NULL;
    static char *link_template = NULL;
    if (link_template == NULL ) {

        // Note the Selected parameter is added with a preceding @. This ensures that it is present in the 
        // return link. 
        link_template = get_drilldown_link_with_font(
                QUERY_PARAM_VIEW "=@VIEW@&p=&"QUERY_PARAM_TITLE_FILTER"="QPARAM_FILTER_EQUALS QPARAM_FILTER_STRING "@NAME@&@CELLNO_PARAM@=@CELLNO@","@ATTR@","@TITLE@","@FONT_CLASS@");
    }

    int free_name2;
    char *name2 = url_encode_static(name,&free_name2);

    result = replace_all_str(link_template,
            "@VIEW@",view->name,
            "@NAME@",name2,
            "@ATTR@",attr,
            "@TITLE@",title,
            "@FONT_CLASS@",font_class,
            "@CELLNO_PARAM@",get_drilldown_name_static(QUERY_PARAM_SELECTED,1),
            "@CELLNO@",cell_no_txt,
            NULL);

    if (free_name2) FREE(name2);
    return result;
}

char *get_movie_drilldown_link(ViewMode *view,char *idlist,char *attr,char *title,char *font_class,char *cell_no_txt)
{
    char *result = NULL;
    static char *link_template = NULL;
    if (link_template == NULL ) {

        // Note the Selected parameter is added with a preceding @. This ensures that it is present in the 
        // return link. 
        link_template = get_drilldown_link_with_font(
               QUERY_PARAM_VIEW "=@VIEW@&p=&idlist=@IDLIST@&@CELLNO_PARAM@=@CELLNO@","@ATTR@","@TITLE@","@FONT_CLASS@");
    }

    result = replace_all_str(link_template,
            "@VIEW@",view->name,
            "@IDLIST@",idlist,
            "@ATTR@",attr,
            "@TITLE@",title,
            "@FONT_CLASS@",font_class,
            "@CELLNO_PARAM@",get_drilldown_name_static(QUERY_PARAM_SELECTED,1),
            "@CELLNO@",cell_no_txt,
            NULL);

    return result;
}
char *get_person_drilldown_link(ViewMode *view,char *dbfieldid,char *id,char *attr,char *name,char *font_class,char *cell_no_txt)
{
    char *result = NULL;
    static char *link_template = NULL;
    if (link_template == NULL ) {

        // Note the Selected parameter is added with a preceding @. This ensures that it is present in the 
        // return link. 
        link_template = get_drilldown_link_with_font(
               QUERY_PARAM_VIEW "=@VIEW@"
               "&"QUERY_PARAM_PERSON_ROLE"=@ROLE@"
               "&p=&"QUERY_PARAM_PERSON"=@ID@&@CELLNO_PARAM@=@CELLNO@","@ATTR@","@NAME@","@FONT_CLASS@");
    }

    result = replace_all_str(link_template,
            "@VIEW@",view->name,
            "@ROLE@",dbfieldid,
            "@ID@",id,
            "@ATTR@",attr,
            "@NAME@",name,
            "@FONT_CLASS@",font_class,
            "@CELLNO_PARAM@",get_drilldown_name_static(QUERY_PARAM_SELECTED,1),
            "@CELLNO@",cell_no_txt,
            NULL);

    return result;
}

static inline ViewMode *get_drilldown_view(DbItem *item) {

    if (item->drilldown_view == NULL) {
        DbItem *item2;
        ViewMode *m;

        switch (item->category) {
            case 'T':
                m = VIEW_TV;
                break;
            case 'M': case 'F':
                m = VIEW_MOVIE;
                break;
            default:
                m = VIEW_OTHER;
                break;
        }

        for( item2=item->linked ; item2 ; item2=item2->linked ) {

            if (item2->category != item->category ) {
                m = VIEW_MIXED;
                break;
            } else {
                switch (item2->category) {
                    case 'T':
                        if (item->season != item2->season) {
                            m = VIEW_TVBOXSET;
                        }
                        break;
                    case 'M': case 'F':
                        // As soon as there are two linked movies its a box set
                        m = VIEW_MOVIEBOXSET;
                        break;
                    default:
                        m = VIEW_MIXED;
                        break;
                }
            }
        }
        item->drilldown_view = m;
    }
    return item->drilldown_view;
}

/*
 * Find the next folder after the mount point.
 * This is not calculated from the real mount point but where we
 * expect things to be on the NMT.
 * So its just the folder name after the root part.
 */
char *next_folder(char *path,char *root)
{
    char *result = NULL;
    if (util_starts_with(path,root)) {
        char *p = strchr(path+strlen(root) + 1 , '/'); // mount folder
        if (p != NULL) {
            ovs_asprintf(&result,"%.*s",p-path,path);
        }
    }
    return result;
}

// Delist a file if it's grandparent folder is present and not empty.
// This is deleted from the db, which will be reflected in the next page draw.
int delisted(DbItem *rowid)
{

    int freepath;
    char *path = get_path(rowid,rowid->file,&freepath);
    int result = 0;
    static int auto_prune=-2;

    if (auto_prune == -2) {
        auto_prune = *oversight_val("ovs_auto_prune") == '1';
        HTML_LOG(0,"auto delist = %d",auto_prune);
    }

    // Dont check media if it is on another oversight db. Let each one manage itself.
    if (is_on_remote_oversight(rowid)) {
        return 0;
    }

    HTML_LOG(1,"auto delist precheck [%s]",path);
    if (auto_prune && !exists(path)) {
        
        HTML_LOG(0,"auto delist check [%s][%s]",rowid->db->source,path);

        char *ancestor_dir = NULL;
        if (util_starts_with(path,NETWORK_SHARE)) {
            ancestor_dir=next_folder(path,NETWORK_SHARE);
        } else {
            ancestor_dir=next_folder(path,"/");
        }

        if (ancestor_dir && exists(ancestor_dir) && !is_empty_dir(ancestor_dir) &&  auto_prune) {

            //media present - file gone!
            db_auto_remove_row(rowid);
            result = 1;

        }
        FREE(ancestor_dir);
    }

    //HTML_LOG(0,"delisted [%s] = %d",path,result);
    HTML_LOG(1-result,"delisted [%s] = %d",path,result);

    if (freepath) FREE(path);
    return result;
}

// Return 1 if this row and all of its linked items are delisted.
int all_linked_rows_delisted(DbItem *rowid)
{
    DbItem *r;

    for(r = rowid ; r != NULL ; r = r->linked) {
        if (!delisted(r)) {
            return 0;
        }
    }
    HTML_LOG(0,"delisted rows linked to [%s]",rowid->title);
    return 1;
}

int total_size(DbItem *rowid)
{
    int sz=0;
    DbItem *ri;
    for( ri = rowid ; ri ; ri=ri->linked ) {
        sz += ri->sizemb;
    }
    return sz;
}

char * write_titlechanger(int offset,int rows, int cols, int numids, DbItem **row_ids)
{
    int i,r,c;
    Array *script = array_new(free);


    
    static int first_time=1;
    if (first_time) {
        first_time = 0;
        array_add(script,STRDUP("function t_0() { ovs_menu_clear(); }\n"));
    }

    for ( r = 0 ; r < rows ; r++ ) {
        for ( c = 0 ; c < cols ; c++ ) {
            i = c * rows + r ;
            if ( i < numids ) {

                DbItem *item = row_ids[i];

                int watched,unwatched;
                get_watched_counts(item,&watched,&unwatched);

                char *js_fn_call=NULL;

                int season = -1;
                ViewMode *view_mode = get_drilldown_view(item);
                if (view_mode == VIEW_TV) {
                    season = item->season;
                }

                if (item->category == 'T' ) {
                    // Write the call to the show function and also tract the idlist;
                    if (view_mode == VIEW_TVBOXSET) {
                        js_fn_call = menu_js_fn(i+1+offset,
                                JS_ARG_STRING,"title",item->title,
                                JS_ARG_STRING,"orig_title",item->orig_title,
                                JS_ARG_STRING,"cert",item->certificate,
                                JS_ARG_STRING,"idlist",build_id_list(item),
                                JS_ARG_INT,"year",item->year,
                                JS_ARG_STRING,"view",view_mode->name,
                                JS_ARG_INT,"unwatched",unwatched,
                                JS_ARG_INT,"watched",watched,
                                JS_ARG_INT,"num_seasons",season_count(item),
                                JS_ARG_INT,"count",item->link_count+1,
                                JS_ARG_INT,"mb",total_size(item),
                                JS_ARG_END);
                    } else {
                        js_fn_call = menu_js_fn(i+1+offset,
                                JS_ARG_STRING,"title",item->title,
                                JS_ARG_STRING,"orig_title",item->orig_title,
                                JS_ARG_STRING,"cert",item->certificate,
                                JS_ARG_STRING,"idlist",build_id_list(item),
                                JS_ARG_INT,"year",item->year,
                                JS_ARG_STRING,"view",view_mode->name,
                                JS_ARG_INT,"unwatched",unwatched,
                                JS_ARG_INT,"watched",watched,
                                JS_ARG_INT,"season",season,
                                JS_ARG_INT,"count",item->link_count+1,
                                JS_ARG_STRING,"videosource",item->videosource,
                                JS_ARG_STRING,"video",item->video,
                                JS_ARG_STRING,"audio",item->audio,
                                JS_ARG_INT,"mb",total_size(item),
                                JS_ARG_END);
                    }

                } else {
                    int freeshare=0;
                    char *share = share_name(item,&freeshare);
                    // char *cert_country = NULL;
                    char *cert_rating = strchr(NVL(item->certificate),':');

                    if (cert_rating) {
                        // cert_country = COPY_STRING(cert_rating - item->certificate,item->certificate);
                        cert_rating++;
                    }

                    // Dont show watched/unwatched for movies
                    js_fn_call = menu_js_fn(i+1+offset,
                            JS_ARG_STRING,"title",item->title,
                            JS_ARG_STRING,"orig_title",item->orig_title,
                            JS_ARG_STRING,"cert",cert_rating,
                            JS_ARG_STRING,"idlist",build_id_list(item),
                            JS_ARG_INT,"runtime",item->runtime,
                            JS_ARG_INT,"year",item->year,
                            JS_ARG_STRING,"view",view_mode->name,
                            JS_ARG_INT,"unwatched",unwatched,
                            JS_ARG_INT,"watched",watched,
                            JS_ARG_STRING,"source",item->db->source,
                            JS_ARG_STRING,"share",share,
                            JS_ARG_INT,"count",item->link_count+1,
                            JS_ARG_STRING,"videosource",item->videosource,
                            JS_ARG_STRING,"video",item->video,
                            JS_ARG_STRING,"audio",item->audio,
                            JS_ARG_INT,"mb",total_size(item),
                            JS_ARG_END);
                    if (freeshare) FREE(share);
                    // if (cert_country) FREE(cert_country);
                }
                if (js_fn_call) {
                    array_add(script,js_fn_call);
                }
            }
        }
    }
    char *result = arraystr(script);
    array_free(script);
    HTML_LOG(0,"write_titlechanger end");
    return result;
}

// Generate the HTML for the grid. 
// Note that the row_ids have already been pruned to only contain the items
// for the current page.
char *render_grid(long page,GridSegment *gs, int numids, DbItem **row_ids,int page_before,int page_after)
{

    int rows = gs->dimensions.rows;

    int cols = gs->dimensions.cols;

    // Points past last item
    int end_item = rows * cols;
    if (end_item > numids ) {
        end_item = numids;
    }
    
    int centre_row = rows/2;
    int centre_col = cols/2;
    int r,c;

    int cell_margin=2;

    HTML_LOG(0,"render page %ld rows %d cols %d",page,rows,cols);

#if 0
    // Diagnostic code. Enable to see all rows dumped.
    HTML_LOG(0,"input size = %d",numids);
    for(r=0 ; r<numids ; r++) {
        HTML_LOG(0,"get_grid row %d %s %s %s",r,row_ids[r]->db->source,row_ids[r]->title,row_ids[r]->file);
        DbItem *l =row_ids[r]->linked;
        while (l) {
            HTML_LOG(0,"get_grid linked %d %s %s %s",r,l->db->source,l->title,l->file);
           l = l->linked;
        }
    }
#endif

    char *result=NULL;
    int i;
    char *width_attr;
    char *height_attr;
    char *tmp;
    int select_mode=!EMPTY_STR(query_select_val());

    char *grid_css;

    if (numids < rows * cols ) {
        //re-arrange layout to have as many columns as possible.
        rows = (numids + (cols-1)) / cols;
    }

    if (g_dimension->poster_mode) {
        ovs_asprintf(&width_attr," width=%dpx ", gs->dimensions.img_width+cell_margin);

        ovs_asprintf(&height_attr," height=%dpx ", gs->dimensions.img_height+cell_margin);
    } else {
        ovs_asprintf(&width_attr," width=%d%% ",(int)(100/cols));
        height_attr=STRDUP("");
    }

    ovs_asprintf(&grid_css,"<style type=\"text/css\">\n"
            "div.grid%d table tr td a font img {\n"
            "\twidth:%dpx;\n"
            "\theight:%dpx;\n}\n"
            "</style>\n",
            gs->offset,
            gs->dimensions.img_width,
            gs->dimensions.img_height);

    char *title_change_script = write_titlechanger(gs->offset,rows,cols,numids,row_ids);

TRACE;
    Array *rowArray = array_new(free);

    int selected_cell = -1;
    if (*get_selected_item()) {
        selected_cell = atol(get_selected_item());
    } else {
        selected_cell = centre_col * rows + centre_row;
    }
    HTML_LOG(0,"rows = %d",rows);
    

    // Now build the table and return the text.
    for ( r = 0 ; r < rows ; r++ ) {

        HTML_LOG(1,"grid row %d",r);

        ovs_asprintf(&tmp,"<tr class=\"grid_row%d\" >\n",(r&1));

        array_add(rowArray,tmp);

        for ( c = 0 ; c < cols ; c++ ) {

            switch(gs->grid_direction) {
                case GRID_ORDER_VERTICAL:
                    i = c * rows + r ; break;
                case GRID_ORDER_HORIZONTAL:
                case GRID_ORDER_DEFAULT:
                    i = r * cols + c ; break;

                default:
                    assert("bad grid order"==NULL);
                    i = r * cols + c ; break;
            }

            HTML_LOG(1,"grid col %d",c);

            int left_scroll = (page_before && c == 0);
            int right_scroll = (page_after && c == cols-1 );
            int is_selected = (i == selected_cell);

            char *item_text=NULL;
            if ( i < numids ) {
                item_text = get_item(gs->offset+i,row_ids[i],(c+r)&1,width_attr,height_attr,left_scroll,right_scroll,is_selected,select_mode);
            } else {
                // only draw empty cells if there are two or more rows
                if (rows > 1) {
                    item_text = get_empty(width_attr,(c+r)&1,height_attr,left_scroll,right_scroll,is_selected);
                } else {
                    item_text = NULL;
                }

            }

            if (item_text) array_add(rowArray,item_text);
            HTML_LOG(1,"grid end col %d",c);
        }

        array_add(rowArray,STRDUP("</tr>\n"));
        HTML_LOG(1,"grid end row %d",r);

    }

    char *w;
    if (!g_dimension->poster_mode) {
        w="width=100%";
    } else {
        w="";
    }

    result = arraystr(rowArray);
    array_free(rowArray);

    ovs_asprintf(&tmp,
            "<script type=\"text/javascript\"><!--\n%s\n--></script>\n%s\n"
            "<center><div class=grid%d><table class=overview_poster %s>\n%s\n</table></div></center>\n",
            title_change_script,
            grid_css,
            gs->offset,
            (g_dimension->poster_mode?"":" width=100%"),
            (result?result:"<tr><td>&nbsp;</td><tr>")
    );

    FREE(grid_css);

    FREE(title_change_script);

    FREE(result);
    result=tmp;

    FREE(width_attr);
    HTML_LOG(0,"render_grid length = %d",strlen(NVL(result)));

    return result;
}


char *get_grid(long page,GridSegment *gs,DbSortedRows *sorted_rows) 
{
    int numids = sorted_rows->num_rows;
    DbItem **row_ids = sorted_rows->rows;
    // first loop through the selected rowids that we expect to draw.
    // If there are any that need pruning - remove them from the database and get another one.
    // This will possibly cause a temporary inconsistency in page numbering but
    // as we have just updated the database it will be correct on the next page draw.
    //
    // Note the db load routing should already filter out items that cant be mounted,
    // otherwise this can cause timeouts.
    if (gs->parent->page_size == DEFAULT_PAGE_SIZE) {
        gs->parent->page_size = gs->dimensions.rows * gs->dimensions.cols;
    }

    int start = page * gs->parent->page_size; 

    int total=0;
    // Create space for pruned rows
    DbItem **prunedRows = filter_page_items(start,numids,row_ids,gs->parent->page_size,&total);

    
    DbItem **segmentRows = prunedRows + gs->offset;

    int segment_total = total - gs->offset;

    if (segment_total < 0 ) {

        segment_total = 0;

    } else if (segment_total > gs->dimensions.rows * gs->dimensions.cols ) {

        segment_total = gs->dimensions.rows * gs->dimensions.cols ;
    }


    int page_before = (gs->offset == 0) && (page > 0);
    int page_after = gs->offset + segment_total >= total;


    char  *ret =  render_grid(page,gs,segment_total,segmentRows,page_before,page_after);
    FREE(prunedRows);
    return ret;
}


/* Convert 234 to TVID/text message regex */
char *get_tvid( char *sequence ) {
    char *out = NULL;
    char *p,*q;
    if (sequence) {
        out = p = MALLOC(9*strlen(sequence)+1);
        *p = '\0';
        for(q = sequence ; *q ; q++ ) {
            switch(*q) {
                case '1' : strcpy(p,"1"); break;
                case '2' : strcpy(p,"[2abc]"); break;
                case '3' : strcpy(p,"[3def]"); break;
                case '4' : strcpy(p,"[4ghi]"); break;
                case '5' : strcpy(p,"[5jkl]"); break;
                case '6' : strcpy(p,"[6mno]"); break;
                case '7' : strcpy(p,"[7pqrs]"); break;
                case '8' : strcpy(p,"[8tuv]"); break;
                case '9' : strcpy(p,"[9wxyz]"); break;
            }
            p += strlen(p);
        }
    }
    HTML_LOG(2,"tvid %s = regex %s",sequence,out);
    return out;

}

char *default_button_attr() {
    static char *default_attr = NULL;
    if (default_attr == NULL) {
        ovs_asprintf(&default_attr,"width=%ld height=%ld",g_dimension->button_size,g_dimension->button_size);
        HTML_LOG(1,"default button attr = %s",default_attr);
    }
    return default_attr;
}

void href(char *url,char *attr,char *text) {
    printf("\n<a href=\"%s\" %s>%s</a>",url,attr,text);
}

char *get_theme_image_tag(char *image_name,char *attr) {

    char *isrc;
    char *result = NULL;
    assert(image_name);
    if (attr == NULL || ! *attr) {

        attr = default_button_attr();
    }
    isrc = icon_source(image_name);

    ovs_asprintf(&result,"<img alt=\"%s\" border=0 src=\"%s\" %s />",image_name,isrc,attr);

    FREE(isrc);
    return result;
}


DbSortedRows *get_sorted_rows_from_params()
{


    // Get filter options
    long crossview=0;

    config_check_long(g_oversight_config,"ovs_crossview",&crossview);
    HTML_LOG(1,"Crossview = %ld",crossview);

    //Tvid filter = this as the form 234
    //HTML_LOG(1,"begin hdump");
    html_hashtable_dump(0,"query",g_query);
    //HTML_LOG(1,"end hdump");

TRACE;

    ViewMode *view = get_view_mode(0);


    // Tv/Film filter
    // ==============

#if 0
    char *media_types=view->media_types;

    if(media_types == DB_MEDIA_TYPE_ANY) {
        media_types = query_val(QUERY_PARAM_TYPE_FILTER);
    }
#else
    // Not worth adding extra filtering for the view type
    char *media_types = query_val(QUERY_PARAM_TYPE_FILTER);
#endif

TRACE;
    Exp *query_exp = build_filter(media_types);
    
    HTML_LOG(0,"Scan..");

    // Get array of rowsets. One item for each database source. 
    DbItemSet **rowsets = db_crossview_scan_titles( crossview, query_exp);

    exp_free(query_exp,1);

TRACE;

    HTML_LOG(0,"Overview..");
    // Merge the rowsets into a single view.
    struct hashtable *overview = db_overview_hash_create(rowsets,view);
TRACE;

    DbItem **sorted_row_ids = NULL;
    
    char *sort = DB_FLDID_TITLE;

    config_check_str(g_query,QUERY_PARAM_SORT,&sort);
    int numrows = hashtable_count(overview);
TRACE;

    HTML_LOG(1,"Sort..");

    int (*sort_fn)(DbItem **,DbItem**) = view->default_sort;

    if (sort_fn == NULL) {
        if (sort && STRCMP(sort,DB_FLDID_TITLE) == 0) {

            sort_fn = db_overview_cmp_by_title;
        } else {
            sort_fn = db_overview_cmp_by_age_desc;
        }
    }

    if (sort_fn) {
        sorted_row_ids = sort_overview(overview,sort_fn);
    }

    //Free hash without freeing keys
    db_overview_hash_destroy(overview);

    DbSortedRows *sortedRows = MALLOC(sizeof(DbSortedRows));
    sortedRows->num_rows = numrows;
    sortedRows->rowsets = rowsets;
    sortedRows->rows = sorted_row_ids;

    HTML_LOG(0,"end get_sorted_rows_from_params : %d rows",numrows);

    return sortedRows;
}

void sorted_rows_free_all(DbSortedRows *sortedRows)
{
    if (sortedRows) {
        HTML_LOG(0,"sorted_rows_free_all");
        FREE(sortedRows->rows);
        db_free_rowsets_and_dbs(sortedRows->rowsets);
        FREE(sortedRows);
    }
}

typedef struct tvid_struct {
    char *sequence;
    char *range;
} Tvid ;

#define TVID_MAX_LEN 2 //
#define TVID_MAX 99  //must be 10 ^ TVID_MAX_LEN -1
char *get_tvid_links()
{

    // Pre compute format string for tvids.
#define TVID_MARKER "@X@Y@"
    char *params;
    ovs_asprintf(&params,"idlist=&p=0&"QUERY_PARAM_TITLE_FILTER"=%s",TVID_MARKER);

    char *format_string = get_self_link(params,"tvid=\""TVID_MARKER"\"","");
    FREE(params);

    int format_memsize = strlen(format_string)+20;

    // TODO there is a bug here if string contaings %s - will get corrupted.
    char *tmp = replace_all_str(format_string,
            "%","%%",
            TVID_MARKER,"%s",
            NULL);
    FREE(format_string);
    format_string=tmp;

//    HTML_LOG(0,"Format string [%s]",format_string);


    char *result = NULL;
    Tvid list[]={
        {"0","*"} , {"11","*"} , // reset
        {"1","1"} ,  // db scan will get all non-alpha if this parameter is < A
        {"2","A"} , {"22","B"} , {"222","C"} ,
        {"3","D"} , {"33","E"} , {"333","F"} ,
        {"4","G"} , {"44","H"} , {"444","I"} ,
        {"5","J"} , {"55","K"} , {"555","L"} ,
        {"6","M"} , {"66","N"} , {"666","O"} ,
        {"7","P"} , {"77","Q"} , {"777","R"} , {"7777","S"} ,
        {"8","T"} , {"88","U"} , {"888","V"} ,
        {"9","W"} , {"99","X"} , {"999","Y"} , {"9999","Z"} ,
        { NULL,NULL} } ;

    // Get size of array - probably a better C way.
    int i;
    for ( i = 0 ; list[i].sequence ; i++ ) {
//        HTML_LOG(0,"tvid = %s/%s",list[i].sequence,list[i].range);
        continue;
    }
    HTML_LOG(0,"size = %d",i);

    result = CALLOC(i,format_memsize);

    char *p = result;
    for ( i = 0 ; list[i].sequence ; i++ ) {
        p += sprintf(p,format_string,list[i].range,list[i].sequence);
        *(p++) = '\n';
        *p='\0';
    }
//    HTML_LOG(0,"result = %s",result);

    return result;
}

#define ARROW_CLASS " class=\"resize_arrow\""

char *dimension_cell_name_suffix() {
    return NVL(get_view_mode(0)->dimension_cell_suffix);
}

char *dimension_cell_name(char *set_name,char *name_suffix,char *set_val) {
    char *name;
    ovs_asprintf(&name,"%s%s%s",set_name,name_suffix,set_val);
    return name;
}

char *resize_link(char *name,char *increment,char *min,char *max,char *tvid,char *text)
{
    char *result;
    char *attr;
    char *params;

    char *name_suffix = dimension_cell_name_suffix();

    char *cell_name = dimension_cell_name(name,name_suffix,increment);

    ovs_asprintf(&params,"action=" QUERY_PARAM_ACTION_VALUE_SET "&"
                         QUERY_PARAM_SET_NAME "=%s%s&" 
                         QUERY_PARAM_SET_MIN "=%s&"
                         QUERY_PARAM_SET_MAX "=%s&"
                         QUERY_PARAM_SET_VAL "=%s&"
                         QUERY_START_CELL "=%s",
                         name,name_suffix,min,max,increment,cell_name);

    ovs_asprintf(&attr,"tvid=\"%s\" name=\"%s\" "ARROW_CLASS,tvid,cell_name);
    result = get_self_link(params,attr,text);

    FREE(cell_name);
    FREE(params);
    FREE(attr);
    return result;
}
// Links to resize the grid -on the fly
char *get_tvid_resize_links()
{
    char *result=NULL;
    char *view = query_view_val();

    if (*query_val(QUERY_PARAM_RESIZE)) {

#define GRID_INC "1"
#define ARROW_UP "↑"
#define ARROW_LEFT "←"
#define ARROW_RIGHT "→"
#define ARROW_DOWN "↓"
        char *rinc=resize_link("ovs_poster_mode_rows",GRID_INC,"1","99","558",ARROW_DOWN);
        char *rdec=resize_link("ovs_poster_mode_rows","-"GRID_INC,"1","99","552",ARROW_UP);
        char *cinc=resize_link("ovs_poster_mode_cols",GRID_INC,"1","99","556",ARROW_RIGHT);
        char *cdec=resize_link("ovs_poster_mode_cols","-"GRID_INC,"1","99","554",ARROW_LEFT);

#define POSTER_INC "10"

#define PARROW_UP "⇧"
#define PARROW_LEFT "⇦"
#define PARROW_RIGHT "⇨"
#define PARROW_DOWN "⇩"
        char *hinc=resize_link("ovs_poster_mode_height",POSTER_INC   ,"60","750","58",PARROW_DOWN);
        char *hdec=resize_link("ovs_poster_mode_height","-"POSTER_INC,"60","750","52",PARROW_UP);
        char *winc=resize_link("ovs_poster_mode_width" ,POSTER_INC,   "40","500","56",PARROW_RIGHT);
        char *wdec=resize_link("ovs_poster_mode_width" ,"-"POSTER_INC,"40","500","54",PARROW_LEFT);
        char *wzero=resize_link("ovs_poster_mode_width" ,QUERY_ASSIGN_PREFIX"0","0","500","","auto_width");
        char *hzero=resize_link("ovs_poster_mode_height" ,QUERY_ASSIGN_PREFIX"0","0","750","","auto_height");

        char *control=get_self_link(QUERY_PARAM_RESIZE"=&set_name=&set_val=&action=",ARROW_CLASS,"Off");

        char *grid_centre=NULL;
        char *poster_centre=NULL;
TRACE;

        if (*view) {

TRACE;
            grid_centre=get_self_link("action="QUERY_RESIZE_DIM_ACTION"&"QUERY_RESIZE_DIM_SET_NAME"="QUERY_RESIZE_DIM_SET_GRID,
                    ARROW_CLASS,"Grid");
TRACE;

            poster_centre=get_self_link("action="QUERY_RESIZE_DIM_ACTION"&"QUERY_RESIZE_DIM_SET_NAME"="QUERY_RESIZE_DIM_SET_IMAGE,
                    ARROW_CLASS,"Images");
TRACE;

        } else {
TRACE;
            grid_centre=STRDUP("<span "ARROW_CLASS">Grid</span>");
TRACE;
            poster_centre=STRDUP("<span "ARROW_CLASS">Images</span>");
TRACE;
        }

TRACE;

    ovs_asprintf(&result,"\
            <style type=\"text/css\">\
            .resize_arrow { font-size:8pt; }\
            </style>\
            <table class=\"resize_table\" >\n"
            "<tr>\n"
                "<td></td><td align=\"center\">%s</td>\n"
                "<td></td>"     "<td></td><td align=\"center\">%s</td><td></td>\n"
                "<td>%s</td>\n"
            "</tr>\n"
            "<tr>\n"
                "<td>%s</td><td>%s</td><td>%s</td>\n"
                "<td>%s</td><td>%s</td><td>%s</td>\n"
                "<td>%s</td>\n"
            "</tr>\n"
            "<tr>\n"
                "<td></td><td align=\"center\">%s</td><td></td>\n"
                "<td></td><td align=\"center\">%s</td><td></td>\n"
                "<td>%s</td>\n"
            "</tr>\n"
            "</table>",
                rdec,hdec,hzero,
                cdec,grid_centre,cinc,wdec,poster_centre,winc,wzero,
                rinc,hinc,control);

TRACE;
        FREE(rinc); FREE(rdec); FREE(cinc); FREE(cdec);
        FREE(hinc); FREE(hdec); FREE(winc); FREE(wdec);
        FREE(grid_centre);
        FREE(poster_centre);

        FREE(control);
TRACE;
//    } else {
//        char *control=get_self_link("resizeon=1","tvid=12","Resize");
//        ovs_asprintf(&result,"%s",control);
//        FREE(control);
    }
    return result;
}

int playlist_size(DbSortedRows *sorted_rows)
{
    int i;
    int count = 0;
    for(i = 0 ; i < sorted_rows->num_rows ; i++ ) {
        DbItem *rowid = sorted_rows->rows[i];
        if (rowid->playlist_names && rowid->playlist_paths) {
            count += rowid->playlist_names->size;
        }
    }

    return count;
}

void build_playlist(DbSortedRows *sorted_rows)
{
    int i;
    FILE *fp = NULL;
    for(i = 0 ; i < sorted_rows->num_rows ; i++ ) {
        DbItem *rowid = sorted_rows->rows[i];
        if (rowid->playlist_names && rowid->playlist_paths) {
            int j;
            HTML_LOG(1,"Adding files for [%s] to playlist",rowid->title);
            assert(rowid->playlist_names->size == rowid->playlist_paths->size);
            for(j = 0 ; j < rowid->playlist_names->size ; j++ ) {

                char *name = rowid->playlist_names->array[j]; 
                char *path = rowid->playlist_paths->array[j]; 
                HTML_LOG(1,"Adding [%s] to playlist",path);

                if (fp == NULL) fp = playlist_open();
                fprintf(fp,"%s|0|0|file://%s|",name,path);
                fflush(fp);
            }
        }
    }
    if (fp) {
        fclose(fp);
    }
}



// Add a javascript function to return a plot string.
// returns number of characters in javascript.
//
// Create a function call 
// <function_prefix>id() { <called_function>( {field:value pairs} ) };
//
// This will probably change to just a js array element soon, so the calling js, can
// walk through all items on page.
//
char *js_function(char *function_prefix,char *called_function,long fn_id,va_list ap) {
    
    Array *a = array_new(free);

    char sep = '{';
    char *s_member,*s_val;
    int i_val;

    while(1) {
        
        char *tmp,*tmp2;

        // Field type
        JavascriptArgType js_arg_type = va_arg(ap,JavascriptArgType);

        if (js_arg_type == JS_ARG_END ) break;

        // Field/Member name
        s_member = va_arg(ap,char *);
        ovs_asprintf(&tmp,"%c\n\t%s:",sep,s_member);
        array_add(a,tmp);
        sep=',';

        //value
        switch(js_arg_type) {
            case JS_ARG_STRING:
                s_val = va_arg(ap,char *);
                tmp2 = clean_js_string(s_val);
                ovs_asprintf(&tmp,"'%s'",tmp2);
                if (tmp2 != s_val ) FREE(tmp2);
                array_add(a,tmp);
                break;
            case JS_ARG_INT:
                i_val = va_arg(ap,int);
                ovs_asprintf(&tmp,"%d",i_val);
                array_add(a,tmp);
                break;
            case JS_ARG_END:
                assert(0);
        }
    }

    char *tmp = arraystr(a);
    array_free(a);

    char *result;

    ovs_asprintf(&result,
            "function %s" "%lx() { %s( %s\n} ); }\n",function_prefix,fn_id,called_function,tmp);
    FREE(tmp);

    return result;
}

char *ep_js_fn(long fn_id,...)
{

    char *result = NULL;

    va_list ap;
    va_start(ap,fn_id);
    result = js_function(JAVASCRIPT_EPINFO_FUNCTION_PREFIX ,"ovs_ep",fn_id,ap);
    va_end(ap);
    return result;
}

char *menu_js_fn(long fn_id,...)
{

    char *result = NULL;

    va_list ap;
    va_start(ap,fn_id);
    result = js_function(JAVASCRIPT_MENU_FUNCTION_PREFIX ,"ovs_menu",fn_id,ap);
    va_end(ap);
    return result;
}

void util_free_char_array(int size,char **a)
{
    int i;
    for(i = 0 ; i < size ; i++ ) {
        FREE(a[i]);
    }
    FREE(a);
}

char *best_eptitle(DbItem *item,int *free_title) {

    *free_title=0;
    char *title=item->eptitle;
    if (title == NULL || !*title) {
        title=item->eptitle_imdb;
    }
    if (title == NULL || !*title) {
        title=item->additional_nfo;
    }
    if (title == NULL || !*title) {
        title=util_basename(item->file);
        *free_title=1;
    }
    return title;
}

/**
 * Create a number of javascript functions that each return the 
 * plot for a given row.
 * The funtions are named using the address location of the data  structure.
 * eg plot12234() { return 'He came, he saw , he conquered'; }
 */
char *create_episode_js_fn(int num_rows,DbItem **sorted_rows) {

    char *result = NULL;

    int i;
    char *tmp;
    Array *outa = array_new(free);

TRACE;
    // get titles from plot.db
    get_plot_offsets_and_text(num_rows,sorted_rows,1);

TRACE;

    // Find the first plot and genre
    char *main_plot=NULL;
    char *main_genre=NULL;

    for(i = 0 ; i < num_rows ; i++ ) {
        DbItem *item = sorted_rows[i];
        if (EMPTY_STR(main_plot) && !EMPTY_STR(item->plottext[PLOT_MAIN])) {
            main_plot = item->plottext[PLOT_MAIN];
        }
        if (EMPTY_STR(main_genre) && !EMPTY_STR(item->genre)) {
            main_genre = item->genre;
        }
    }

    if (EMPTY_STR(main_plot)) main_plot = "(no plot info)";
    if (main_genre == NULL) {
        main_genre = STRDUP("no genre");
    } else {
        main_genre = expand_genre(main_genre);
    }

TRACE;

    tmp = ep_js_fn(0,
            JS_ARG_STRING,"plot",NVL(main_plot),
            JS_ARG_STRING,"genre",NVL(main_genre),
            JS_ARG_STRING,"title",NVL(sorted_rows[0]->title),
            JS_ARG_END);
    array_add(outa,tmp);

    FREE(main_genre);

HTML_LOG(0,"num rows = %d",num_rows);
    // Episode Plots
    for(i = 0 ; i < num_rows ; i++ ) {
        DbItem *item = sorted_rows[i];
        int free_title=0;
        char *title = best_eptitle(item,&free_title);

        int freeshare=0;
        char *share = share_name(item,&freeshare);

        char *plot = item->plottext[PLOT_EPISODE];
        if (plot && plot[2] == ':') plot += 3;

        tmp = ep_js_fn(i+1,
                JS_ARG_STRING,"idlist",build_id_list(item),
                JS_ARG_STRING,"episode",NVL(item->episode),
                JS_ARG_STRING,"plot",NVL(plot),
                JS_ARG_STRING,"info",item->file,
                JS_ARG_STRING,"title",title,
                JS_ARG_STRING,"date",get_date_static(item),
                JS_ARG_STRING,"share",share,
                JS_ARG_INT,"watched",item->watched,
                JS_ARG_INT,"locked",is_locked(item),
                JS_ARG_STRING,"source",item->db->source,
                JS_ARG_STRING,"videosource",item->videosource,
                JS_ARG_STRING,"video",item->video,
                JS_ARG_STRING,"audio",item->audio,
                JS_ARG_END);

        array_add(outa,tmp);

        if (free_title) FREE(title);
        if (freeshare) FREE(share);

    }

    result = arraystr(outa);

    array_free(outa);

    ovs_asprintf(&tmp,"<script type=\"text/javascript\"><!--\n%s\n--></script>\n",NVL(result));
    FREE(result);
    result = tmp;

TRACE;

    return result;
}

char *get_date_static(DbItem *item)
{
    static char *old_date_format=NULL;
    static char *recent_date_format=NULL;
    // Date format
    if (recent_date_format == NULL && !config_check_str(g_oversight_config,"ovs_date_format",&recent_date_format)) {
        recent_date_format="%d %b";
    }
    if (old_date_format == NULL && !config_check_str(g_oversight_config,"ovs_old_date_format",&old_date_format)) {
        old_date_format="%d %b %y";
    }

#define DATE_BUF_SIZ 40
    static char date_buf[DATE_BUF_SIZ];


    OVS_TIME date=item->airdate;
    if (date<=0) {
        date=item->airdate_imdb;
    }
    *date_buf='\0';
    if (date > 0) {

        char *date_format=NULL;
        if  (year(epoc2internal_time(time(NULL))) != year(date)) {  
            date_format = old_date_format;
        } else {
            date_format = recent_date_format;
        }

        strftime(date_buf,DATE_BUF_SIZ,date_format,internal_time2tm(date,NULL));
    }
    return date_buf;
}

// Return all rows after delisting
DbItem **filter_page_items(int start,int num_rows,DbItem **row_ids,int max_new,int *new_num)
{

    int i;
    int total = 0;

    DbItem **new_list = CALLOC(max_new+1,sizeof(DbItem *));

    for ( i = start ; total < max_new && i < num_rows ; i++ ) {
        DbItem *item = row_ids[i];
        if (item) {
            if (item->delist_checked) {
                new_list[total++] = item;
            } else {
                if (!all_linked_rows_delisted(item)) {
                    new_list[total++] = item;
                }
                item->delist_checked = 1;
            }
        }
    }
    *new_num = total;
    HTML_LOG(0,"filter_page_items = %d of %d items (max %d)",total,num_rows,max_new);
    return new_list;
}

char *pruned_tv_listing(int num_rows,DbItem **sorted_rows,int rows,int cols)
{
    int r,c;

    char *select=query_select_val();

    char *listing=NULL;

    int width_txt_and_date=100/cols; //text and date
    int width_epno=1; //episode width
    int width_icon=1; //episode width
    width_txt_and_date -= width_epno+width_icon;

TRACE;
    HTML_LOG(0,"pruned_tv_listing");


TRACE;
    char *script = create_episode_js_fn(num_rows,sorted_rows);
TRACE;

    int show_episode_titles = *query_val(QUERY_PARAM_EPISODE_TITLES) == '1';
    int show_episode_dates = *query_val(QUERY_PARAM_EPISODE_DATES) == '1';
    if  (!show_episode_dates && !show_episode_titles ) {
        width_txt_and_date = 1;
    }

    int show_repacks = *oversight_val("ovs_show_repack") != '0';
    

    printf("%s",script);

    HTML_LOG(0,"pruned_tv_listing num_rows=%d r%d x c%d",num_rows,rows,cols);
#if 0
    // Adjust rows to be squarish.
    if (num_rows/cols < rows ) {
        rows = (num_rows+cols-1) / cols;
    }

    HTML_LOG(0,"pruned_tv_listing num_rows=%d r%d x c%d",num_rows,rows,cols);
#endif

    int i=0;
    for(r=0 ; r < rows ; r++ ) {
        HTML_LOG(1,"tvlisting row %d",r);
        char *row_text = NULL;
        for(c = 0 ; c < cols ; c++ ) {
            HTML_LOG(1,"tvlisting col %d",c);

            //int i = c * rows + r;
            i = r * cols + c;
            if (i < num_rows) {

                int function_id = i+1;
                char *episode_col = NULL;

                DbItem *item = sorted_rows[i];

                if (*select) {
                    episode_col = select_checkbox(
                            item,
                            item->episode);
                } else {
                    char *ep = item->episode;
                    if (ep == NULL || !*ep ) {
                        ep = "play";
                    }
                    char *href_attr = href_focus_event_fn(JAVASCRIPT_EPINFO_FUNCTION_PREFIX,function_id);
                    episode_col = vod_link(
                            item,
                            ep,"",
                            item->db->source,
                            item->file,
                            ep,
                            NVL(href_attr),
                            watched_style(item));
                    FREE(href_attr);
                }

                int free_eptitle=0;
                char *episode_title = "";
                if (show_episode_titles) {
                    episode_title = best_eptitle(item,&free_eptitle);
                }

                char *title_txt=NULL;

                int is_proper = show_repacks && (util_strcasestr(item->file,"proper"));

                int is_repack = show_repacks && (util_strcasestr(item->file,"repack"));

                char *icon_text = icon_link(item->file);

                ovs_asprintf(&title_txt,"%s%s%s",
                        episode_title,
                        (is_proper?"&nbsp;<font class=proper>[pr]</font>":""),
                        (is_repack?"&nbsp;<font class=repack>[rpk]</font>":"")
                        );
                if (free_eptitle) {
                    FREE(episode_title);
                }


                //Date
                char *date_buf=get_date_static(item);


                //network icon
                char *network_icon = add_network_icon(item,"");

                //Put Episode/Title/Date together in new cell.
                char td_class[10];
                sprintf(td_class,"ep%d%d",item->watched,i%2);
                char *tmp;

                char *td_plot_attr = td_mouse_event_fn(JAVASCRIPT_EPINFO_FUNCTION_PREFIX,function_id);

                ovs_asprintf(&tmp,
                        "%s<td class=%s width=%d%% %s align=right>%s</td>" 
                        "<td width=%d%% %s>%s</td>"
                        "<td class=%s width=%d%% %s>"
                        "<font %s>%s%s</font>"
                        "<font class=epdate>%s</font></td>\n",
                        (row_text?row_text:""),
                        td_class,width_epno, td_plot_attr, episode_col,

                        width_icon,NVL(td_plot_attr),NVL(icon_text),

                        td_class, width_txt_and_date, td_plot_attr,

                        watched_style(item), (network_icon?network_icon:""),
                        title_txt,
                        (show_episode_dates && *date_buf?date_buf:"")
                        );
                FREE(icon_text);

                if (!EMPTY_STR(td_plot_attr)) FREE(td_plot_attr);

                FREE(title_txt);
                FREE(episode_col);
                FREE(row_text);
                row_text=tmp;

            } else {
                char *tmp=NULL;
                ovs_asprintf(&tmp,"%s<td width=%d%%></td><td width=%d%%></td><td width=%d%%></td>\n",
                    (row_text?row_text:""),
                    width_epno,
                    width_icon,
                    width_txt_and_date);
                FREE(row_text);
                row_text=tmp;
            }
        }
        // Add the row
        if (row_text) {
            char *tmp;
            ovs_asprintf(&tmp,"%s<tr align=top>%s</tr>\n",(listing?listing:""),row_text);
            FREE(row_text);
            FREE(listing);
            listing=tmp;
        }
        if (i >= num_rows) break;
    }


    char *result=NULL;
    ovs_asprintf(&result,"<table width=100%% class=listing onblur=\"tv_inf0();\" >%s</table>",listing);
    FREE(listing);
    return result;
}

char *tv_listing(DbSortedRows *sorted_rows,int rows,int cols)
{
    int pruned_num_rows;
    DbItem **pruned_rows;


    html_log(-1,"tv_listing");
    pruned_rows = filter_page_items(0,sorted_rows->num_rows,sorted_rows->rows,sorted_rows->num_rows,&pruned_num_rows);
    char *result = pruned_tv_listing(pruned_num_rows,pruned_rows,rows,cols);
    FREE(pruned_rows);

    return result;
}

// do not free
char *get_status_static()
{
    static char *result=NULL;
#define MSG_SIZE 50
    static char first=1;

    if (first) {
        first = 0;
        static char msg[MSG_SIZE+1];
        char *filename;
        ovs_asprintf(&filename,"%s/catalog.status",appDir());

        msg[0] = '\0';
        PRE_CHECK_FGETS(msg,MSG_SIZE);

        FILE *fp = fopen(filename,"r");
        if (fp) {
            fgets(msg,MSG_SIZE,fp);
            CHECK_FGETS(msg,MSG_SIZE);
            chomp(msg);

            result = msg;

            fclose(fp);
        } else {
            HTML_LOG(1,"Error %d opening [%s]",errno,filename);
        }
        FREE(filename);

        if (result == NULL) {

            if (exists_in_dir(tmpDir(),"cmd.pending")) {
                result = "Scanning...";
            }
        }

    }

    return result;
}

char *auto_option_list(char *name,char *firstItem,struct hashtable *vals)
{

    static char *attr = NULL;
    if (attr == NULL ) {
        if (g_dimension->local_browser) {
            ovs_asprintf(&attr,
            // Note the gaya path requires the full script name here. 
            // This becomes /HARD_DISK/Apps/oversight/oversight.cgi
            "onchange=\"location.assign('%s'+this.childNodes[this.selectedIndex].value)\"", getenv("SCRIPT_NAME"));
        } else {
            attr="onchange=\"location.assign(this.options[this.selectedIndex].value)\"";
        }
    }
    return option_list(name,attr,firstItem,vals);
}


#define PLACEHOLDER "@x@x@"
char *option_list(char *name,char *attr,char *firstItem,struct hashtable *vals)
{
    char *result=NULL;
    char *selected=query_val(name);
    char *params;

    // Do not take ownership of the keys - thay belong to the hashtable.
    Array *keys = util_hashtable_keys(vals,0);
TRACE1;
HTML_LOG(0,"option_list[%s]",name);

char *s = "fred";
HTML_LOG(0,"pos[%s]=%d",s,abet_pos(s,g_abet_title->abet));

    //array_sort(keys,array_strcasecmp);
    array_sort(keys,array_abetcmp);

    ovs_asprintf(&params,QUERY_PARAM_PAGE"=&"QUERY_PARAM_IDLIST"=&%s=" PLACEHOLDER,name);
    char *link=self_url(params);
    FREE(params);
    Array *link_parts = splitstr(strchr(link,'?'),PLACEHOLDER);
    FREE(link);

    char *first = NULL;
    Array *out = array_new(free);

    //GAYA does seem to like passing just the options to the link
    //eg just "?a=b"
    //we have to pass a more substantial path. eg. .?a=b

    if (keys && keys->size) {
        int i;


        for(i = 0 ; i < keys->size ; i++ ) {

            char *tmp;
            char *k=keys->array[i];
            char *v=hashtable_search(vals,k);
            char *link=join(link_parts,k);

            char *selected_text=(STRCMP(selected,k)==0?"selected":"");

            ovs_asprintf(&tmp, "<option value=\"%s\" %s >%s</option>\n", link, selected_text, v );

            if (firstItem != NULL && STRCMP(firstItem,k) == 0 ) {
                first = tmp;
            } else {
                array_add(out,tmp);
            }

            FREE(link);
        }
    }

    result = arraystr(out);
    array_free(out);

    if ( !EMPTY_STR(result) || !EMPTY_STR(first)) {
        char *tmp;
        ovs_asprintf(&tmp,"<select %s>\n%s\n%s</select>", attr,NVL(first),NVL(result));
        FREE(result);
        result = tmp;
    }
    FREE(first);
    array_free(link_parts);
    array_free(keys);
    return result;
}

void xx_dump_genre(char *file,int line,int num,DbItem **rows) {
    int i;
    HTML_LOG(0,"xx genre dump [%s:%d] num=%d",file,line,num);
    for(i = 0 ; i < num ; i++ ) {
        DbItem *item = rows[i];
        HTML_LOG(0,"%d[%s][%s]",item->id,item->title,item->genre);
    }
}

// Remove and store the last navigation cell. eg if user clicked on cell 12 this is passed in 
// the URL as @i=12. The url that returns to this page then has i=12. If we have returned to this
// page we must remove i=12 from the query so that it is not passed to the new urls created for this 
// page.
static char *selected_item = NULL;
void set_selected_item()
{
    if (selected_item == NULL) {
        selected_item = STRDUP(query_val(QUERY_PARAM_SELECTED));
        query_remove(QUERY_PARAM_SELECTED);
    }
}
char *get_selected_item()
{
    assert(selected_item);
    return selected_item;
}

// vi:sw=4:et:ts=4
