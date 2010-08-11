// $Id:$
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admin.h"
#include "display.h"
#include "gaya_cgi.h"
#include "template.h"


void display_admin(DbSortedRows *sorted_rows) {

    char *action = query_val(QUERY_PARAM_ACTION);

    HTML_LOG(1,"action = %s",action);

    if (!allow_admin()) {

        printf("admin disabled");

    } else if (EMPTY_STR(action) || STRCMP(action,"ask")==0 || STRCMP(action,"Cancel")==0) {

        display_main_template("default","admin",sorted_rows);

    } else if (util_starts_with(action,"settings")) {
        
        display_main_template("default","settings",sorted_rows);

#define CONFIRM_PREFIX "confirm_"
    } else if (util_starts_with(action,CONFIRM_PREFIX)) {
        
        display_main_template("default",action+strlen(CONFIRM_PREFIX),sorted_rows);

#define TEMPLATE_PREFIX "template_"
    } else if (util_starts_with(action,TEMPLATE_PREFIX)) {
        
        display_main_template("default",action+strlen(TEMPLATE_PREFIX),sorted_rows);

    } else {

        display_main_template("default","completed",sorted_rows);
    }

}
// vi:sw=4:et:ts=4
