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
   if (new_buffer)
     {
        char *end = LEX_REVERSE(*comment, strlen(*comment), SKIP_SPACES_TOKEN);
        *(end+1) = '\0';
     }
   return new_buffer;
}

static Function_Id
_function_parse(char *buffer, char **new_buffer)
{
   Function_Id foo_id = NULL;
   char *function = NULL;
   Eina_List *types_list = NULL, *itr;
   *new_buffer = NULL;
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
   foo_id = database_function_new(function);
   if (comment)
      database_function_description_set(foo_id, comment);

   // Return TYPES
   types_function[strlen(types_function) - strlen(function)] = '\0'; // needed to parse the types
   char *type_as_string;
   LEX(types_function, STRINGS_LIST(",", &types_list));
   EINA_LIST_FOREACH(types_list, itr, type_as_string)
      database_function_parameter_add(foo_id, EINA_FALSE, type_as_string, NULL, NULL);

   // PARAMS. tmp_buffer points to them
   char *params = NULL;
   Eina_List *params_list = NULL;
   *new_buffer = LEX(tmp_buffer, STRING(");", &params));
   LEX(params, STRINGS_LIST(",", &params_list));
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
        LEX_REVERSE(type_as_string, strlen(type_as_string), UWORD(&name)); // extract the param name
        char *tmp = strstr(type_as_string, name);
        *tmp = '\0';
        database_function_parameter_add(foo_id, EINA_TRUE, type_as_string, name, type_comment2);
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

   return foo_id;
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

static Function_Id
_function_parse2(char *buffer, char **new_buffer)
{
   Function_Id foo_id = NULL;
   Eina_List *types_list = NULL, *itr;
   char *function = NULL;
   *new_buffer = NULL;
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
   foo_id = database_function_new(function);
   if (comment)
      database_function_description_set(foo_id, comment);

   // Return TYPES
   types_function[strlen(types_function) - strlen(function)] = '\0'; // needed to parse the types
   char *type_as_string;
   LEX(types_function, STRINGS_LIST(",", &types_list));
   EINA_LIST_FOREACH(types_list, itr, type_as_string)
      database_function_parameter_add(foo_id, EINA_FALSE, type_as_string, NULL, NULL);

   // PARAMS. tmp_buffer points to them
   char *params = NULL;
   Eina_List *params_list = NULL;
   *new_buffer = LEX(tmp_buffer, STRING(");", &params));
   LEX(params, STRINGS_LIST(",", &params_list));
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
        database_function_parameter_add(foo_id, !strcmp(dir, "in"), type, name, type_comment2);
        if (name) free(name);
        if (dir) free(dir);
        if (type) free(type);
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

   return foo_id;
}

static char *
_class_parse(char *buffer)
{
   char *class_name = NULL;
   char *new_buffer = LEX(buffer, UWORD(&class_name), KCHAR('='), KCHAR('{'));
   if (new_buffer)
     {
        database_class_add(class_name);
        while(new_buffer)
          {
             buffer = new_buffer;
             new_buffer = NULL;
             char *token = NULL;
             LEX(buffer, UWORD(&token));
             if (!token) return NULL;
             if (!strcmp(token, "inherit"))
               {
                  Eina_List *inherits_list = NULL;
                  new_buffer = LEX(buffer, KWORD("inherit"), KCHAR('{'),
                        UWORDS_LIST(",", &inherits_list), KCHAR(';'), KCHAR('}'));
                  if (!new_buffer) return NULL;
                  database_class_inherits_list_add(class_name, inherits_list);
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
                       Function_Id foo_id = _function_parse(buffer, &new_buffer);
                       if (foo_id)
                          database_class_property_add(class_name, foo_id);
                       if (LEX(new_buffer, KCHAR('}')))
                         {
                            new_buffer = LEX(new_buffer, KCHAR('}'));
                            loop = EINA_FALSE;
                         }
                    }
               }
             if(!strcmp(token, "properties_set"))
               {
                  new_buffer = LEX(buffer, KWORD("properties_set"), KCHAR('{'));
                  Eina_Bool loop = EINA_TRUE;
                  while (new_buffer && loop)
                    {
                       buffer = new_buffer;
                       Function_Id foo_id = _function_parse(buffer, &new_buffer);
                       if (foo_id)
                          database_class_property_add(class_name, foo_id);
                       if (LEX(new_buffer, KCHAR('}')))
                         {
                            new_buffer = LEX(new_buffer, KCHAR('}'));
                            loop = EINA_FALSE;
                         }
                    }
               }
             if(!strcmp(token, "properties_get"))
               {
                  new_buffer = LEX(buffer, KWORD("properties_get"), KCHAR('{'));
                  Eina_Bool loop = EINA_TRUE;
                  while (new_buffer && loop)
                    {
                       buffer = new_buffer;
                       Function_Id foo_id = _function_parse(buffer, &new_buffer);
                       if (foo_id)
                          database_class_property_add(class_name, foo_id);
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
                       Function_Id foo_id = _function_parse2(buffer, &new_buffer);
                       if (foo_id)
                          database_class_method_add(class_name, foo_id);
                       if (LEX(new_buffer, KCHAR('}')))
                         {
                            new_buffer = LEX(new_buffer, KCHAR('}'));
                            loop = EINA_FALSE;
                         }
                    }
               }
          }
     }
   return new_buffer;
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

   char *tmp = buffer;
   while (tmp)
     {
        _class_parse(tmp);
        tmp = NULL;
     }
   return EINA_TRUE;
}

