#include <Eina.h>
#include "Eolian.h"
#include "database.h"

static Eina_Hash *_classes = NULL;

typedef struct
{
   char *name;
   char *macro;
   char *description;
   Eina_List *inherits;
   Eina_List *properties; /* Hash prop_name -> _Function_Id */
   Eina_List *methods; /* Hash meth_name -> _Function_Id */
   Eina_List *constructors; /* Hash constructors_name -> _Function_Id */
} Class_desc;

typedef struct
{
   char *name;
   Eina_List *params; /* list of _Parameter_Desc */
   Function_Type type;
   Eina_Hash *data;
} _Function_Id;

typedef struct
{
   char *name;
   char *type;
   char *description;
   Parameter_Dir param_dir;
} _Parameter_Desc;


static void
_param_del(_Parameter_Desc *pdesc)
{
   free(pdesc->name);
   free(pdesc->type);
   free(pdesc->description);
   free(pdesc);
}

static void
_fid_del(_Function_Id *fid)
{
   _Parameter_Desc *param;
   free(fid->name);
   eina_hash_free(fid->data);
   EINA_LIST_FREE(fid->params, param)
      _param_del(param);
   free(fid);
}

static void
_class_del(Class_desc *class)
{
   char *inherit_name;
   Eina_List *inherits = class->inherits;
   EINA_LIST_FREE(inherits, inherit_name)
      free(inherit_name);

   _Function_Id *fid;
   EINA_LIST_FREE(class->methods, fid)
      _fid_del(fid);
   EINA_LIST_FREE(class->properties, fid)
      _fid_del(fid);

   free(class->name);
   free(class->description);
   free(class);
}

void _hash_free_cb(void *data)
{
   Class_desc *cl = data;
   _class_del(cl);
}

Eina_Bool
eolian_database_init()
{
   eina_init();
   if (!_classes)
      _classes = eina_hash_string_superfast_new(_hash_free_cb);
   return EINA_TRUE;
}

Eina_Bool
eolian_database_shutdown()
{
   eina_hash_free(_classes);
   eina_shutdown();
   return EINA_TRUE;
}

Eina_Bool
database_class_add(const char *classname)
{
   if (_classes)
     {
        Class_desc *desc = calloc(1, sizeof(*desc));
        desc->name = strdup(classname);
        eina_hash_set(_classes, classname, desc);
     }
   return EINA_TRUE;
}

Eina_Bool
database_class_del(char *class_name)
{
   Class_desc *cl = eina_hash_find(_classes, class_name);
   if (cl)
     {
        eina_hash_del(_classes, class_name, NULL);
        _class_del(cl);
           return EINA_TRUE;
     }

   return EINA_FALSE;
}

static Eina_Bool _class_name_get(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata)
{
   Class_desc *desc = data;
   Eina_List **list = fdata;
   if (desc && list)
     {
        *list = eina_list_append(*list, desc->name);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

const Eina_List *
database_class_names_list_get()
{
   Eina_List *list = NULL;
   eina_hash_foreach(_classes, _class_name_get, &list);
   return list;
}

Eina_Bool database_class_exists(char *class_name)
{
   return !!eina_hash_find(_classes, class_name);
}

Eina_Bool
database_class_inherit_add(const char *class_name, char *inherit_class_name)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   if (!desc) return EINA_FALSE;
   desc->inherits = eina_list_append(desc->inherits, inherit_class_name);
   return EINA_TRUE;
}

const char*
database_class_macro_get(const char *class_name)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   return (desc ? desc->macro : NULL);
}

void
database_class_macro_set(const char *class_name, const char *macro)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   if (desc) desc->macro = strdup(macro);
}

const char*
database_class_description_get(const char *class_name)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   return (desc ? desc->description : NULL);
}

void
database_class_description_set(const char *class_name, const char *description)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   if (desc) desc->description = strdup(description);
}

const Eina_List *
database_class_inherits_list_get(char *class_name)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   return (desc?desc->inherits:NULL);
}

Function_Id
database_function_new(const char *function_name, Function_Type foo_type)
{
   _Function_Id *fid = calloc(1, sizeof(*fid));
   fid->name = strdup(function_name);
   fid->type = foo_type;
   fid->data  = eina_hash_string_superfast_new(free);
   return (Function_Id) fid;
}

Eina_Bool database_class_function_add(const char *classname, Function_Id foo_id)
{
   Class_desc *desc = eina_hash_find(_classes, classname);
   if (!foo_id || !desc) return EINA_FALSE;
   _Function_Id *fid = (_Function_Id *) foo_id;
   switch (fid->type)
     {
      case PROPERTY_FUNC:
      case SET:
      case GET:
         desc->properties = eina_list_append(desc->properties, foo_id);
         break;
      case METHOD_FUNC:
         desc->methods = eina_list_append(desc->methods, foo_id);
         break;
      case CONSTRUCTOR:
         desc->constructors = eina_list_append(desc->constructors, foo_id);
         break;
      default:
         return EINA_FALSE;
     }
   return EINA_TRUE;
}

const Eina_List *
database_class_functions_list_get(char *class_name, Function_Type foo_type)
{
   Class_desc *desc = eina_hash_find(_classes, class_name);
   if (!desc) return NULL;
   switch (foo_type)
     {
      case PROPERTY_FUNC:
         return desc->properties;
      case METHOD_FUNC:
         return desc->methods;
      default: return NULL;
     }
}

Function_Type
database_function_type_get(Function_Id function_id)
{
   _Function_Id *fid = (_Function_Id *)function_id;
   return fid->type;
}

const char *
database_function_name_get(Function_Id function_id)
{
   _Function_Id *fid = (_Function_Id *)function_id;
   return fid->name;
}

void
database_function_description_set(Function_Id function_id, const char *key, const char *description)
{
   _Function_Id *fid = (_Function_Id *)function_id;
   EINA_SAFETY_ON_NULL_RETURN(key);
   if (description)
     {
        if (!eina_hash_find(fid->data, key))
          eina_hash_set(fid->data, key, strdup(description));
     }
   else
     {
        eina_hash_del(fid->data, key, NULL);
     }
}

const char *
database_function_description_get(Function_Id function_id, const char *key)
{
   _Function_Id *fid = (_Function_Id *)function_id;
   return (fid ? eina_hash_find(fid->data, key) : NULL);
}

Parameter_Desc
database_function_parameter_add(Function_Id foo_id, Parameter_Dir param_dir, const char *type, const char *name, const char *description)
{
   _Parameter_Desc *param = NULL;
   _Function_Id *fid = (_Function_Id *)foo_id;
   if (fid)
     {
        param = calloc(1, sizeof(*param));
        param->param_dir = param_dir;
        if (name) param->name = strdup(name);
        if (type) param->type = strdup(type);
        if (description) param->description = strdup(description);
        fid->params = eina_list_append(fid->params, param);
     }
   return (Parameter_Desc) param;
}

const Eina_List *
database_parameters_list_get(Function_Id foo_id)
{
   _Function_Id *fid = (_Function_Id *)foo_id;
   return (fid?fid->params:NULL);
}

/* Get parameter information */
void
database_parameter_information_get(Parameter_Desc param_desc, Parameter_Dir *param_dir, char **type, char **name, char **description)
{
   _Parameter_Desc *param = (_Parameter_Desc *)param_desc;
   if (!param) return;
   if (param_dir) *param_dir = param->param_dir;
   if (type) *type = param->type;
   if (name) *name = param->name;
   if (description) *description = param->description;
}

static Eina_Bool _function_print(const _Function_Id *fid, int nb_spaces)
{
   printf("%*s%s ", nb_spaces, "", fid->name);
   switch (fid->type)
     {
      case PROPERTY_FUNC:
           {
              const char *str = database_function_description_get((Function_Id)fid, "comment_set");
              printf("\n%*s <%s>\n", nb_spaces + 5, "", (str ? str : ""));
              str = database_function_description_get((Function_Id)fid, "comment_get");
              printf("%*s <%s>\n", nb_spaces + 5, "", (str ? str : ""));
              break;
           }
      case GET:
           {
              const char *str = database_function_description_get((Function_Id)fid, "comment_get");
              printf("<%s>\n", (str ? str : ""));
              break;
           }
      case SET:
           {
              const char *str = database_function_description_get((Function_Id)fid, "comment_set");
              printf("<%s>\n", (str ? str : ""));
              break;
           }

      case METHOD_FUNC:
      case CONSTRUCTOR:
           {
              //char *str = eina_hash_find(fid->data, "comment");
              const char *str = database_function_description_get((Function_Id)fid, "comment");
              printf("<%s>\n", (str ? str : ""));
              break;
           }
     }
   Eina_List *itr;
   _Parameter_Desc *param;
   EINA_LIST_FOREACH(fid->params, itr, param)
     {
        char *param_dir = NULL;
        switch (param->param_dir)
          {
           case IN_PARAM:
              param_dir = "IN";
              break;
           case OUT_PARAM:
              param_dir = "OUT";
              break;
           case INOUT_PARAM:
              param_dir = "INOUT";
              break;
          }
         printf("%*s%s <%s> <%s> <%s>\n", nb_spaces + 5, "", param_dir, param->name, param->type, (param->description?param->description:""));
     }
   return EINA_TRUE;
}

static Eina_Bool _class_print(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata EINA_UNUSED)
{
   Eina_List *itr;
   _Function_Id *function;
   Class_desc *desc = data;
   printf("Class %s:\n", desc->name);
   printf("  macro: %s\n", desc->macro);
   printf("  description: %s\n", desc->description);

   // Inherits
   printf("  inherits: ");
   char *word;
   EINA_LIST_FOREACH(desc->inherits, itr, word)
     {
        printf("%s ", word);
     }
   printf("\n");

   // Constructors:
   printf("  constructors:\n");
   EINA_LIST_FOREACH(desc->constructors, itr, function)
     {
        _function_print(function, 4);
     }
   printf("\n");

   // Properties
   printf("  properties:\n");
   EINA_LIST_FOREACH(desc->properties, itr, function)
     {
        _function_print(function, 4);
     }
   printf("\n");

   // Methods
   printf("  methods:\n");
   EINA_LIST_FOREACH(desc->methods, itr, function)
     {
        _function_print(function, 4);
     }
   printf("\n");
   return EINA_TRUE;
}

Eina_Bool eolian_show()
{
   eina_hash_foreach(_classes, _class_print, NULL);
   return EINA_TRUE;
}
