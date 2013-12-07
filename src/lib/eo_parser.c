#if 0
#include <Ecore_File.h>
#include <stdio.h>
#include <string.h>

#include "Eolian.h"
#include "lexer.h"
#include "database.h"
#include "eina_json.h"

#define NAME "name"
#define INHERITS "inherits"
#define PROPERTIES "properties"
#define METHODS "methods"
#define CONSTRUCTORS "constructors"
#define IMPLEMENTS "implements"
#define SIGNALS "signals"

static Function_Type
_func_type_resolve(const char *type)
{
   if (!type) return METHOD_FUNC;
   if (!strcmp("rw", type)) return PROPERTY_FUNC;
   if ((!strcmp("ro", type)) || (!strcmp("get_only", type))) return GET;
   if ((!strcmp("wo", type)) || (!strcmp("set_only", type))) return SET;
   return METHOD_FUNC;
}

static Parameter_Dir
_get_param_dir(const char *dir)
{
   if (!strcmp("in", dir)) return IN_PARAM;
   if (!strcmp("out", dir)) return OUT_PARAM;
   return INOUT_PARAM;
}

#define JSON_ARR_NTH_STRING_GET(arr, idx) eina_json_string_get(eina_json_array_nth_get((arr), (idx)))
static Eina_Bool
_func_from_json(const char *class_name, Eina_Json_Value *jv, Function_Type _f_type)
{
   Eina_Iterator *it = eina_json_object_iterator_new(jv);
   Eina_Json_Value *itv, *v;
   Function_Id foo_id;
   Function_Type f_type;
   Eina_Bool ret;
   /* Iterate over properties. */
   EINA_ITERATOR_FOREACH(it, itv)
     {
        const char *func_name, *comment = NULL;
        Eina_Json_Value *func_body;

        func_name = eina_json_pair_name_get(itv);
        func_body = eina_json_pair_value_get(itv);

        /* UNRESOLVED passed when iterating over properties.
         * If field "type" is absent in JSON, t.e. type is PROPERTY. */
        if (_f_type == UNRESOLVED)
          {
             Eina_Json_Value *sp, *gp;
             sp = EINA_JSON_OBJECT_VALUE_GET(func_body, "set");
             gp = EINA_JSON_OBJECT_VALUE_GET(func_body, "get");
             if (sp && gp)
               {
                  f_type = PROPERTY_FUNC;
               }
             else if (sp)
               {
                  f_type = SET;
               }
             else
               {
                  f_type = GET;
               }
          }
        else
          f_type = _f_type;

        ret = database_class_function_exists(class_name, func_name, f_type);
        if (ret)
          {
             printf("ERR: Function: \"%s\" already exists in class \"%s\"\n", func_name, class_name);
             return EINA_FALSE;
          }

        foo_id = database_function_new(func_name, f_type);
        if (!foo_id) return EINA_FALSE;

        /* Read "return_type" parameter*/
        v = EINA_JSON_OBJECT_VALUE_GET(func_body, "return_type");
        if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
          {
             database_function_return_type_set(foo_id, eina_json_string_get(v));
          }

        /* Read "comment" parameter*/
        v = EINA_JSON_OBJECT_VALUE_GET(func_body, COMMENT);
        if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
          comment = eina_json_string_get(v);

        /* For properties need to determine which comment to save. */
        if ((f_type == SET) || (f_type == GET) || (f_type == PROPERTY_FUNC))
          {
             const char *comment_set = NULL, *comment_get = NULL;
             const char *tmp = NULL;

             Eina_Json_Value *sp, *gp;
             sp = EINA_JSON_OBJECT_VALUE_GET(func_body, "set");
             gp = EINA_JSON_OBJECT_VALUE_GET(func_body, "get");

             if (gp)
               {
                  v = EINA_JSON_OBJECT_VALUE_GET(gp, COMMENT);
                  if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
                    comment_get = eina_json_string_get(v);
                  v = EINA_JSON_OBJECT_VALUE_GET(gp, LEGACY);
                  if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
                    database_function_data_set(foo_id, LEGACY_GET, eina_json_string_get(v));
               }
             if (sp)
               {
                  v = EINA_JSON_OBJECT_VALUE_GET(sp, COMMENT);
                  if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
                    comment_set = eina_json_string_get(v);
                  v = EINA_JSON_OBJECT_VALUE_GET(sp, LEGACY);
                  if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
                    database_function_data_set(foo_id, LEGACY_SET, eina_json_string_get(v));
               }

             if (f_type == PROPERTY_FUNC)
               {
                  tmp = comment ? comment : comment_set;
                  database_function_description_set(foo_id, COMMENT, tmp);
                  database_function_description_set(foo_id, COMMENT_SET, tmp);
                  database_function_description_set(foo_id, COMMENT_GET, comment_get);
               }
             else if (f_type == GET)
               {
                  tmp = comment ? comment : comment_get;
                  database_function_description_set(foo_id, COMMENT, tmp);
                  database_function_description_set(foo_id, COMMENT_GET, tmp);
               }
             else if (f_type == SET)
               {
                  tmp = comment ? comment : comment_set;
                  database_function_description_set(foo_id, COMMENT, tmp);
                  database_function_description_set(foo_id, COMMENT_SET, tmp);
               }
          }
        else
          {
             v = EINA_JSON_OBJECT_VALUE_GET(func_body, LEGACY);
             if ((v) && (eina_json_type_get(v) == EINA_JSON_TYPE_STRING))
               database_function_data_set(foo_id, LEGACY, eina_json_string_get(v));
             database_function_description_set(foo_id, COMMENT, comment);
          }

        /* Read parameters. */
        Eina_Json_Value *params_section;
        params_section = EINA_JSON_OBJECT_VALUE_GET(func_body, "parameters");
        if ((f_type == METHOD_FUNC) || (f_type == CONSTRUCTOR))
          {
             if ((params_section) &&
                 (eina_json_type_get(params_section) == EINA_JSON_TYPE_OBJECT))
               {
                  Eina_Json_Value *params;
                  Eina_Json_Value *itr;

                  Eina_List *lst = NULL;
                  char *data;
                  lst = eina_list_append(lst, "in");
                  lst = eina_list_append(lst, "in,out");
                  lst = eina_list_append(lst, "out");

                  EINA_LIST_FREE(lst, data)
                    {
                       /* consequentially get in, in-out and out parameters */
                       params = EINA_JSON_OBJECT_VALUE_GET(params_section, data);
                       if ((params) && (eina_json_type_get(params) == EINA_JSON_TYPE_ARRAY))
                         {
                            Eina_Iterator *par_it = eina_json_array_iterator_new(params);
                            EINA_ITERATOR_FOREACH(par_it, itr)
                              {
                                 Eina_Json_Value *param = eina_json_object_nth_get(itr, 0);
                                 const char *par_name, *par_comment = NULL, *par_type = NULL;
                                 Eina_Json_Value *par_body;
                                 Parameter_Dir par_dir;

                                 par_name = eina_json_pair_name_get(param);
                                 par_body = eina_json_pair_value_get(param);
                                 par_type = JSON_ARR_NTH_STRING_GET(par_body, 0);
                                 par_comment = JSON_ARR_NTH_STRING_GET(par_body, 1);
                                 par_dir = _get_param_dir(data);
                                 database_function_parameter_add(foo_id, par_dir, par_type, par_name, par_comment);
                              }
                            eina_iterator_free(par_it);
                         }
                    }
               }
          }
        else
          {
             if ((params_section) &&
                 (eina_json_type_get(params_section) == EINA_JSON_TYPE_ARRAY))
               {
                  Eina_Iterator *par_it = eina_json_array_iterator_new(params_section);
                  Eina_Json_Value *itr;
                  EINA_ITERATOR_FOREACH(par_it, itr)
                    {
                       Eina_Json_Value *param = eina_json_object_nth_get(itr, 0);
                       const char *par_name, *par_comment = NULL, *par_type = NULL;
                       Eina_Json_Value *par_body;
                       Parameter_Dir par_dir;

                       par_name = eina_json_pair_name_get(param);
                       par_body = eina_json_pair_value_get(param);
                       par_type = JSON_ARR_NTH_STRING_GET(par_body, 0);
                       par_comment = JSON_ARR_NTH_STRING_GET(par_body, 1);
                       par_dir = (f_type == GET) ? OUT_PARAM : IN_PARAM;
                       database_function_parameter_add(foo_id, par_dir, par_type, par_name, par_comment);
                    }
               }
          }
        database_class_function_add(class_name, foo_id);
     }
   eina_iterator_free(it);
   return EINA_TRUE;
}

static Eina_Bool
_class_parse_json(char *buffer)
{
   Eina_Json_Context *ctx = NULL;
   Eina_Bool err;
   Eina_Json_Type type;
   Eina_Json_Value *tree = NULL, *jv = NULL;
   Eina_Bool ret = EINA_TRUE;

   ctx = eina_json_context_dom_new();
   err = eina_json_context_parse(ctx, buffer);
   if (!err)
     {
        Eina_Json_Error e = -1;
        e = eina_json_context_error_get(ctx);
        printf("ERR: json parsing error: %d\n", e);
        ret = EINA_FALSE;
        goto end;
     }
   tree = eina_json_context_dom_tree_take(ctx);
   type = eina_json_type_get(tree);
   if (type != EINA_JSON_TYPE_OBJECT)
     {
        printf("ERR: Json Value is not oblect\n");
        ret = EINA_FALSE;
        goto end;
     }

   const char *class_name = NULL;
   /* Get "name" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, NAME);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_STRING))
     {
        class_name = eina_json_string_get(jv);
        if (database_class_exists(class_name))
          {
             printf("ERR: Class: \"%s\" already exists in database,\n", class_name);
             ret = EINA_FALSE;
             goto end;
          }
        database_class_add(class_name);
     }
   /* Get "comment" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, COMMENT);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_STRING))
     {
        const char *comment = eina_json_string_get(jv);
        database_class_description_set(class_name, comment);
     }

   /* Get "legacy" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, LEGACY);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_STRING))
     {
        const char *legacy = eina_json_string_get(jv);
        database_class_legacy_prefix_set(class_name, legacy);
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
   /* Get "methods" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, METHODS);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_OBJECT))
     {
        _func_from_json(class_name, jv, METHOD_FUNC);
     }
   /* Get "constructors" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, CONSTRUCTORS);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_OBJECT))
     {
        _func_from_json(class_name, jv, CONSTRUCTOR);
     }
   /* Get "implements" section. */
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, IMPLEMENTS);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_ARRAY))
     {
        Eina_Iterator *it = eina_json_array_iterator_new(jv);
        Eina_Json_Value *param;
        /* Implement's array: ["Class Name", "Func Name", "Func Type"]"*/
        EINA_ITERATOR_FOREACH(it, param)
          {
             const char *cl, *f, *t;
             Function_Type ft = UNRESOLVED;
             cl = JSON_ARR_NTH_STRING_GET(param, 0);
             f = JSON_ARR_NTH_STRING_GET(param, 1);
             jv =  eina_json_array_nth_get(param, 2);
             if (jv)
               {
                  t = JSON_ARR_NTH_STRING_GET(param, 2);
                  ft = _func_type_resolve(t);
               }
             database_class_implements_add(class_name, database_class_implements_new(cl, f, ft));
          }
        eina_iterator_free(it);
     }
   jv = EINA_JSON_OBJECT_VALUE_GET(tree, SIGNALS);
   if ((jv) && (eina_json_type_get(jv) == EINA_JSON_TYPE_ARRAY))
     {
        Eina_Iterator *it = eina_json_array_iterator_new(jv);
        Eina_Json_Value *param;
        /* Signal's array: ["clicked", "This event will shoot when button is clecked"]"*/
        EINA_ITERATOR_FOREACH(it, param)
          {
             const char *s, *d;
             s = JSON_ARR_NTH_STRING_GET(param, 0);
             d = JSON_ARR_NTH_STRING_GET(param, 1);
             database_class_event_add(class_name, database_class_event_new(s, d));
          }
        eina_iterator_free(it);
     }

end:
   if (tree) eina_json_value_free(tree);
   if (ctx) eina_json_context_free(ctx);
   return ret;
}
#undef JSON_ARR_NTH_STRING_GET

Eina_Bool eolian_eo_class_desc_parse_json(char *class_desc)
{
   char *tmp = class_desc;
   Eina_Bool ret;
   while (tmp)
     {
        ret = _class_parse_json(tmp);
        tmp = NULL;
     }
   return ret;
}

Eina_Bool eolian_eo_file_parse(char *filename)
{
   Eina_Bool ret;
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

   ret = eolian_eo_class_desc_parse_json(buffer);
   free(buffer);
   return ret;
}
#endif

