## Skin Macros ##

Go to [SkinMacros](SkinMacros.md) for more information on Skin Macros. If the documentation is lacking see section "Examining the source code"

## Variables ##

Variables can be used to hold text to oputput directly to the HTML page, or they can simple numeric expressions, used in IF/THEN/ELSE tests.

Macros are similar to functions. They can take arguments and produce different output depending on the arguments. Variables are just little bits of text.

### Direct output ###
The syntax for outputting a variable is

```
[:$variable_name:]
```

eg

```
width=[:$ovs_image_width:]
```

### Passing Variables to macros ###

Variables can be passed as arguments to macros eg.

```
[:SOME_MACRO($some_variable):]
```

### Types of Variables ###

All of the config files in /share/Apps/oversight/conf are loaded as predefined variables. (constants really)

oversight.cfg settings become $ovs**_variables. eg $ovs\_version
catalog.cfg settings become $catalog_** variables
unpak.cfg settings become $unpak**_variables._

The default settings are store in the corresponding defaults file. Eg .oversight.cfg.defaults.
As the user changes settings they are stored in the main file.**


#### User configurable skin variables and skin.cfg ####

Also the skin.cfg for the current skin

```
/share/Apps/oversight/templates/<skin_name>/conf
```

is loaded as $skin**_variables._

If developing skins you should only put things you WANT the user to change in the**

```
/share/Apps/oversight/templates/<skin_name>/conf/.skin.cfg.defaults
```

As the user changes settings they are stored in skin.cfg.
The name must start with "skin_"_

To allow the user to edit the settings add a corresponding entry to
```
/share/Apps/oversight/templates/<skin_name>/help/skin.cfg.help
```

eg
```
skin_height:The height of the blah - free text
skin_width:The width from 110 to 140 - drop down list|110|120|130|140
```

### Private skin variables ###

If you want variables that you DO NOT want the user to change, then user the SET macro,
and the variable names begin with underscore

eg.

```
[:SET(_height,440):]
```

and reference it in the usual way:

```
<img src=... height=[:$_height:] >
```

If you want to set these variables for your entire skin then put the SET macros in a template (e.g. vars.template ) , and include that template in your other templates.

## Examining the source code ##

Until the documentation is completed the best place for information are the existing templates and the source code.

### Macros ###

For macros eg [:XXX:] look in http://code.google.com/p/oversight/source/browse/trunk/src/macro.c

Eg for information on the "POSTER" macro, search the file for "POSTER" you will find a line like
```
hashtable_insert(macros,"POSTER",macro_fn_poster);
```

Then search for the corresponding function (in this case function macro\_fn\_poster ) and review the function. Especially call\_info->args
```
/**
* =begin wiki
* ==POSTER==
*
* Display poster for current item.
*
* [:POSTER:]
* [:POSTER(attributes):]
*
* =end wiki
*/
char *macro_fn_poster(MacroCallInfo *call_info)
{
```

### Variables ###

For predefined variables eg @sd of [:IF($@sd):] look in http://code.google.com/p/oversight/source/browse/trunk/src/variables.c