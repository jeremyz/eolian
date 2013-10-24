#include <Ecore_File.h>
#include <stdio.h>
#include <string.h>

#include "Eolian.h"
#include "lexer.h"
#include "database.h"
#include "eina_json.h"

#define NAME "name"
#define MACRO "macro"
#define INHERITS "inherits"
#define PROPERTIES "properties"
#define METHODS "methods"

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
_strip(char *str)
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
_get_func_type(const char *type)
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
      database_function_description_set(foo_id, "comment", comment);

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
        char *tmp;
        // extract param name; tmp will be a "space" before par name;
        tmp = LEX_REVERSE(type_as_string, strlen(type_as_string), UWORD(&name));
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
_get_param_dir(const char *dir)
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
      database_function_description_set(foo_id, "comment", comment);

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
        char *tmp;
        tmp = LEX_REVERSE(type_as_string, strlen(type_as_string), UWORD(&name)); // extract the param name
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

   if (database_class_exists(class_name))
     {
        printf("Class: \"%s\" already exists in database,\n", class_name);
        goto end;
     }
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
end:
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

#define JSON_ARR_NTH_STRING_GET(arr, idx) eina_json_string_get(eina_json_array_nth_get((arr), (idx)))
static Function_Id
_func_from_json(const char *class_name, Eina_Json_Value *jv, Function_Type _f_type)
{
   Eina_Iterator *it = eina_json_object_iterator_new(jv);
   Eina_Json_Value *itv, *v;
   Function_Id foo_id;
   Function_Type f_type;
   /* Iterate over properties. */
   EINA_ITERATOR_FOREACH(it, itv)
     {
        const char *func_name, *comment = NULL;
        Eina_Json_Value *func_body;

        func_name = eina_json_pair_name_get(itv);
        func_body = eina_json_pair_value_get(itv);

        if (_f_type == UNRESOLVED)
          {
             v = EINA_JSON_OBJECT_VALUE_GET(func_body, "type");
             const char *prop_type = eina_json_string_get(v);
             f_type = _get_func_type(prop_type);
          }
        else
          f_type = _f_type;

        foo_id = database_function_new(func_name, f_type);
        if (!foo_id) return NULL;

        /* Read "comment" parameter*/
        v = EINA_JSON_OBJECT_VALUE_GET(func_body, "comment");
        if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
          comment = eina_json_string_get(v);

        /* For properties need to determine which comment to save. */
        if ((f_type == SET) || (f_type == GET) || (f_type == PROPERTY_FUNC))
          {
             const char *comment_set = NULL, *comment_get = NULL;
             const char *tmp = NULL;

             v = EINA_JSON_OBJECT_VALUE_GET(func_body, "comment_get");
             if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
               comment_get = eina_json_string_get(v);
             v = EINA_JSON_OBJECT_VALUE_GET(func_body, "comment_set");
             if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
               comment_set = eina_json_string_get(v);

             if (f_type == PROPERTY_FUNC)
               {
                  tmp = comment ? comment : comment_set;
                  database_function_description_set(foo_id, "comment", tmp);
                  database_function_description_set(foo_id, "comment_set", tmp);
                  database_function_description_set(foo_id, "comment_get", comment_get);
               }
             else if (f_type == GET)
               {
                  tmp = comment ? comment : comment_get;
                  database_function_description_set(foo_id, "comment", tmp);
                  database_function_description_set(foo_id, "comment_get", tmp);
               }
             else if (f_type == SET)
               {
                  tmp = comment ? comment : comment_set;
                  database_function_description_set(foo_id, "comment", tmp);
                  database_function_description_set(foo_id, "comment_set", tmp);
               }
          }
        else
          {
             database_function_description_set(foo_id, "comment", comment);
          }

        /* Read parameters. */
        Eina_Json_Value *param_arr, *param;
        param_arr = EINA_JSON_OBJECT_VALUE_GET(func_body, "parameters");
        Eina_Iterator *param_it = eina_json_array_iterator_new(param_arr);
        EINA_ITERATOR_FOREACH(param_it, param)
          {
             const char *param_dir, *par_type, *par_name, *par_comment;
             param_dir = JSON_ARR_NTH_STRING_GET(param, 0);
             par_type = JSON_ARR_NTH_STRING_GET(param, 2);
             par_name = JSON_ARR_NTH_STRING_GET(param, 3);
             par_comment = JSON_ARR_NTH_STRING_GET(param, 4);

             database_function_parameter_add(foo_id, _get_param_dir(param_dir), par_type, par_name, par_comment);
          }
        eina_iterator_free(param_it);
        database_class_function_add(class_name, foo_id);
     }
   eina_iterator_free(it);
   return foo_id;
}

static void
_class_parse_json(char *buffer)
{
   Eina_Json_Context *ctx = NULL;
   Eina_Bool err;
   Eina_Json_Type type;
   Eina_Json_Value *tree = NULL, *jv = NULL;

   ctx = eina_json_context_dom_new();
   err = eina_json_context_parse(ctx, buffer);
   if (!err)
     {
        Eina_Json_Error e = -1;
        e = eina_json_context_error_get(ctx);
        printf("json parsing error: %d\n", e);
        goto end;
     }
   tree = eina_json_context_dom_tree_take(ctx);
   type = eina_json_type_get(tree);
   if (type != EINA_JSON_TYPE_OBJECT)
     {
        printf("Json Value is not oblect\n");
        goto end;
     }

   const char *class_name = NULL;
   /* Get "name" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, NAME);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_STRING))
     {
        class_name = eina_json_string_get(jv);
        database_class_add(class_name);
     }
   /* Get "macro" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, MACRO);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_STRING))
     {
        const char *macro = eina_json_string_get(jv);
        database_class_macro_set(class_name, macro);
     }
   /* Get "comment" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, "comment");
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_STRING))
     {
        const char *comment = eina_json_string_get(jv);
        database_class_description_set(class_name, comment);
     }

   /* Get "inhrits" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, INHERITS);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_ARRAY))
     {
        Eina_Json_Value *arr, *v;
        Eina_Iterator *it = NULL;
        arr = jv;
        it = eina_json_array_iterator_new(arr);
        EINA_ITERATOR_FOREACH(it, v)
          {
             const char *val = eina_json_string_get(v);
             if (strlen(val))
               database_class_inherit_add(class_name, strdup(val));
          }
        eina_iterator_free(it);
     }
   /* Get "properties" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, PROPERTIES);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_OBJECT))
     {
        _func_from_json(class_name, jv, UNRESOLVED);
     }
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, METHODS);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_OBJECT))
     {
        _func_from_json(class_name, jv, METHOD_FUNC);
     }
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, "constructors");
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_OBJECT))
     {
        _func_from_json(class_name, jv, CONSTRUCTOR);
     }

end:
   if (tree) eina_json_value_free(tree);
   if (ctx) eina_json_context_free(ctx);
}
#undef JSON_ARR_NTH_STRING_GET

Eina_Bool eolian_eo_class_desc_parse_json(char *class_desc)
{
   char *tmp = class_desc;
   while (tmp)
     {
        _class_parse_json(tmp);
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

   eolian_eo_class_desc_parse_json(buffer);
   free(buffer);
   return EINA_TRUE;
}

