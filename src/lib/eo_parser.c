#include <Ecore_File.h>
#include <stdio.h>
#include <string.h>

#include "Eolian.h"
#include "lexer.h"
#include "database.h"

static char *
_comment_parse(char *buffer, char **comment)
{
   char *new_buffer = LEX(buffer, KWORD("/*"), STRING("*/", comment));
   /* If comment is empty, NULL will be returned. */
   if (new_buffer && (*comment))
     {
        char *end = LEX_REVERSE(*comment, strlen(*comment), SKIP_SPACES_TOKEN);
        *(end + 1) = '\0';
     }
   return new_buffer;
}

static char*
_strip(char* str)
{
   char *beg, *end;
   beg = str;
   end = str + strlen(str) - 1;

   while (*beg == ' ')
     beg++;
   while (*end == ' ')
     end--;
   return strndup(beg, end - beg + 1);
}

static Function_Type
_get_func_type(char *type)
{
   if (!type) return METHOD_FUNC;
   if (!strcmp("rw", type)) return PROPERTY_FUNC;
   if (!strcmp("ro", type)) return GET;
   if (!strcmp("wo", type)) return SET;
   return METHOD_FUNC;
}

static Function_Id
_property_parse(char *buffer, char **new_buffer)
{
   Function_Id foo_id = NULL;
   char *function = NULL;
   Eina_List *types_list = NULL, *itr;
   /*
    * COMMENT = STRING
    * TYPE = STRING
    * TYPES = | TYPE | TYPE TYPES
    * PARAM = TYPE PARAM_NAME COMMENT | TYPE PARAM_NAME
    * PARAMS = | PARAM | PARAM PARAMS
    * PROTOTYPE = TYPES FUNC_NAME ( PARAMS )
    * FUNCTION = COMMENT PROTOTYPE | PROTOTYPE
    */

   // COMMENT
   char *comment = NULL;
   char *tmp_buffer = _comment_parse(buffer, &comment);
   if (tmp_buffer) buffer = tmp_buffer;

   // PROTOTYPE
   char *types_function = NULL; // types and function, no params
   char *prop_dir = NULL;
   tmp_buffer = LEX(buffer, STRING("(", &types_function));
   if (!types_function) goto end;
   prop_dir = strstr(types_function, " ");
   if (prop_dir)
     {
        prop_dir = strndup(types_function, prop_dir - types_function);
     }

   // FUNC_NAME
   LEX_REVERSE(types_function, strlen(types_function), UWORD(&function));
   foo_id = database_function_new(function, _get_func_type(prop_dir));
   if (comment)
      database_function_description_set(foo_id, comment);

   char *type_as_string;
   // Return TYPES
#if 0
   types_function[strlen(types_function) - strlen(function)] = '\0'; // needed to parse the types
   LEX(types_function, STRINGS_LIST(",", &types_list));
   EINA_LIST_FOREACH(types_list, itr, type_as_string)
      database_function_parameter_add(foo_id, EINA_FALSE, type_as_string, NULL, NULL);
#endif

   // PARAMS. tmp_buffer points to them
   char *params = NULL;
   Eina_List *params_list = NULL;
   *new_buffer = LEX(tmp_buffer, STRING(");", &params));
   LEX(params, STRINGS_LIST(",", &params_list));
   EINA_LIST_FOREACH(params_list, itr, type_as_string)
     {
        char *type_comment = strstr(type_as_string, "/*");
        char *type_comment2 = NULL;
        char *type = NULL;
        if (type_comment)
          {
             _comment_parse(type_comment, &type_comment2);
             type_as_string[strlen(type_as_string) - strlen(type_comment)] = '\0';
          }
        char *name = NULL;
        LEX_REVERSE(type_as_string, strlen(type_as_string), UWORD(&name)); // extract the param name
        char *tmp = strstr(type_as_string, name);
        *tmp = '\0';
        type = _strip(type_as_string);

        /* For properties:
         *   -if property is "set/get" or "set" we save description of IN parameter t.e. of type int;
         *   -if property is "get" we save description of OUT parameter t.e. of type int*. */
        Parameter_Dir dir = IN_PARAM;
        if (database_function_type_get(foo_id) == GET) dir = OUT_PARAM;

        database_function_parameter_add(foo_id, dir, type, name, type_comment2);
        if (type) free(type);
        if (name) free(name);
        if (type_comment2) free(type_comment2);
     }

end:
   if (params) free(params);
   if (comment)
     free(comment);
   if (types_function)
     free(types_function);
   if (prop_dir)
     free(prop_dir);
   if (function)
     free(function);
   EINA_LIST_FREE(types_list, type_as_string)
      free(type_as_string);
   EINA_LIST_FREE(params_list, type_as_string)
      free(type_as_string);

   return foo_id;
}

Parameter_Dir
_get_param_dir(char *dir)
{
   if (!strcmp("in", dir)) return IN_PARAM;
   if (!strcmp("out", dir)) return OUT_PARAM;
   return INOUT_PARAM;
}

static Function_Id
_method_parse(char *buffer, char **new_buffer)
{
   Function_Id foo_id = NULL;
   Eina_List *types_list = NULL, *itr;
   char *function = NULL;
   /*
    * COMMENT = STRING
    * TYPE = STRING
    * TYPES = | TYPE | TYPE TYPES
    * PARAM = TYPE PARAM_NAME COMMENT | TYPE PARAM_NAME
    * PARAMS = | PARAM | PARAM PARAMS
    * PROTOTYPE = TYPES FUNC_NAME ( PARAMS )
    * FUNCTION = COMMENT PROTOTYPE | PROTOTYPE
    */

   // COMMENT
   char *comment = NULL;
   char *tmp_buffer = _comment_parse(buffer, &comment);
   if (tmp_buffer) buffer = tmp_buffer;

   // PROTOTYPE
   char *types_function = NULL; // types and function, no params
   tmp_buffer = LEX(buffer, STRING("(", &types_function));
   if (!types_function) goto end;

   // FUNC_NAME
   LEX_REVERSE(types_function, strlen(types_function), UWORD(&function));
   foo_id = database_function_new(function, METHOD_FUNC);
   if (comment)
      database_function_description_set(foo_id, comment);

   char *type_as_string;
   // Return TYPES
#if 0
   types_function[strlen(types_function) - strlen(function)] = '\0'; // needed to parse the types
   LEX(types_function, STRINGS_LIST(",", &types_list));
   EINA_LIST_FOREACH(types_list, itr, type_as_string)
      database_function_parameter_add(foo_id, EINA_FALSE, type_as_string, NULL, NULL);
#endif

   // PARAMS. tmp_buffer points to them
   char *params = NULL;
   Eina_List *params_list = NULL;
   *new_buffer = LEX(tmp_buffer, STRING(");", &params));
   /* If there are no params, NULL will be written into params*/
   if (!params) goto end;

   LEX(params, STRINGS_LIST(",", &params_list));
   if (params) free(params);
   params = NULL;
   EINA_LIST_FOREACH(params_list, itr, type_as_string)
     {
        char *type_comment = strstr(type_as_string, "/*");
        char *type_comment2 = NULL;
        if (type_comment)
          {
             _comment_parse(type_comment, &type_comment2);
             type_as_string[strlen(type_as_string) - strlen(type_comment)] = '\0';
          }
        char *name = NULL;
        char *dir = NULL;
        char *type = NULL;
        LEX(type_as_string, UWORD(&dir)); // extract the param name
        LEX_REVERSE(type_as_string, strlen(type_as_string), UWORD(&name)); // extract the param name
        char *tmp = strstr(type_as_string, name);
        *tmp = '\0';
        type = _strip(type_as_string + strlen(dir));
        database_function_parameter_add(foo_id, _get_param_dir(dir), type, name, type_comment2);
        if (name) free(name);
        if (dir) free(dir);
        if (type) free(type);
        if (type_comment2) free(type_comment2);
     }

end:
   if (comment)
      free(comment);
   if (types_function)
      free(types_function);
   if (function)
      free(function);
   EINA_LIST_FREE(types_list, type_as_string)
      free(type_as_string);
   EINA_LIST_FREE(params_list, type_as_string)
      free(type_as_string);

   return foo_id;
}

static char *
_class_parse(char *buffer)
{
   char *class_name = NULL;
   char *new_buffer = LEX(buffer, STRING("=", &class_name), KCHAR('{'));
   char *class_name_tmp = _strip(class_name);
   if (class_name) free(class_name);
   class_name = class_name_tmp;
   if (new_buffer)
     {
        database_class_add(class_name);
        while(new_buffer)
          {
             buffer = new_buffer;
             new_buffer = NULL;
             char *token = NULL;
             LEX(buffer, UWORD(&token));
             if (!token) break;
             if (!strcmp(token, "inherit"))
               {
                  Eina_List *inherits_list = NULL;
                  char *inherits_str = NULL;
                  char *inherit;
                  new_buffer = LEX(buffer, KWORD("inherit"), KCHAR('{'));

                  new_buffer = LEX(new_buffer, STRING(";", &inherits_str), KCHAR('}'));

                  if (inherits_str)
                    {
                       LEX(inherits_str, STRINGS_LIST(",", &inherits_list));
                       free(inherits_str);
                       inherits_str = NULL;
                    }
                  if (!new_buffer) return NULL;
                  /* */
                  EINA_LIST_FREE(inherits_list, inherit)
                    {
                       char *tmp = _strip(inherit);
                       free(inherit);
                       database_class_inherit_add(class_name, tmp);
                    }
               }
             if(!strcmp(token, "constructor"))
               {
                  new_buffer = LEX(buffer, KWORD("constructor"),
                        KCHAR('('), KCHAR(')'), KCHAR(';'));
               }
             if(!strcmp(token, "destructor"))
               {
                  new_buffer = LEX(buffer, KWORD("destructor"),
                        KCHAR('('), KCHAR(')'), KCHAR(';'));
               }
             if(!strcmp(token, "properties"))
               {
                  new_buffer = LEX(buffer, KWORD("properties"), KCHAR('{'));
                  Eina_Bool loop = EINA_TRUE;
                  while (new_buffer && loop)
                    {
                       buffer = new_buffer;
                       Function_Id foo_id = _property_parse(buffer, &new_buffer);
                       if (foo_id)
                          database_class_function_add(class_name, foo_id);
                       if (LEX(new_buffer, KCHAR('}')))
                         {
                            new_buffer = LEX(new_buffer, KCHAR('}'));
                            loop = EINA_FALSE;
                         }
                    }
               }
             if(!strcmp(token, "methods"))
               {
                  new_buffer = LEX(buffer, KWORD("methods"), KCHAR('{'));
                  Eina_Bool loop = EINA_TRUE;
                  while (new_buffer && loop)
                    {
                       buffer = new_buffer;
                       Function_Id foo_id = _method_parse(buffer, &new_buffer);
                       if (foo_id)
                          database_class_function_add(class_name, foo_id);
                       if (LEX(new_buffer, KCHAR('}')))
                         {
                            new_buffer = LEX(new_buffer, KCHAR('}'));
                            loop = EINA_FALSE;
                         }
                    }
               }
             free(token);
             token = NULL;
          }
     }
   if (class_name) free(class_name);
   return new_buffer;
}

Eina_Bool eolian_eo_class_desc_parse(char *class_desc)
{
   char *tmp = class_desc;
   while (tmp)
     {
        _class_parse(tmp);
        tmp = NULL;
     }
   return EINA_TRUE;
}

Eina_Bool eolian_eo_file_parse(char *filename)
{
   if (!ecore_file_exists(filename)) return EINA_FALSE;

   unsigned int sz = ecore_file_size(filename);
   char *buffer = malloc(sz * sizeof(char) + 1); // \0 at the end

   FILE* fd = fopen(filename, "r");
   size_t read_sz = fread(buffer, 1, sz, fd);
   buffer[read_sz] = '\0';

   if (read_sz != sz)
     {
        printf("%s: Read size %d different from file size %d. Continue.\n", __FUNCTION__, read_sz, sz);
     }

   eolian_eo_class_desc_parse(buffer);
   free(buffer);
   return EINA_TRUE;
}

