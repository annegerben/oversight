#ifndef __OVS_MACRO_H__
#define __OVS_MACRO_H__

#include "db.h"

void macro_init();
char *macro_call(char *call,int num_rows,DbRowId **sorted_rows);

#endif
