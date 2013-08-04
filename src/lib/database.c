#include <Eina.h>
#include "Eolian.h"
#include "database.h"

static Eina_Hash *_classes = NULL;

typedef struct
{
   char *name;
   Eina_List *inherits;
   Eina_Hash *properties;
} Class_desc;

typedef struct
{
   char *name;
   char *description;
   Eina_List *params;
} _Function_Id;

typedef struct
{
   Eina_Bool param_in;
   char *name;
   char *type;
   char *description;
} Parameter_Desc;

Eina_Bool eolian_database_init()
{
   if (!_classes)
      _classes = eina_hash_string_superfast_new(NULL);
   return EINA_TRUE;
}

Eina_Bool database_class_add(char *classname)
{
   if (_classes)
     {
        Class_desc *desc = calloc(1, sizeof(*desc));
        desc->name = strdup(classname);
        desc->properties = eina_hash_string_superfast_new(NULL);
        eina_hash_add(_classes, classname, desc);
     }
   return EINA_TRUE;
}

Eina_Bool database_class_inherits_list_add(char *classname, Eina_List *inherits_list)
{
   Class_desc *desc = eina_hash_find(_classes, classname);
   if (desc) desc->inherits = inherits_list;
   return !!desc;
}

static Eina_Bool _function_print(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata)
{
   int nb_spaces = (int)fdata;
   _Function_Id *fid = data;
   printf("%*s%s <%s>\n", nb_spaces, "", fid->name, (fid->description?fid->description:""));
   Eina_List *itr;
   Parameter_Desc *param;
   EINA_LIST_FOREACH(fid->params, itr, param)
     {
      if (param->param_in)
         printf("%*sIN %s: %s <%s>\n", nb_spaces+2, "", param->name, param->type, (param->description?param->description:""));
      else
         printf("%*sOUT %s: <%s>\n", nb_spaces+2, "", param->type, (param->description?param->description:""));
     }
   return EINA_TRUE;
}

static Eina_Bool _class_print(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata EINA_UNUSED)
{
   Class_desc *desc = data;
   printf("Class %s:\n", desc->name);

   // Inherits
   printf("  inherits: ");
   Eina_List *itr;
   char *word;
   EINA_LIST_FOREACH(desc->inherits, itr, word)
     {
        printf("%s ", word);
     }
   printf("\n");

   // Properties
   printf("  properties:\n");
   eina_hash_foreach(desc->properties, _function_print, (void *)4); // fdata = spaces to shift the prints
   printf("\n");
   return EINA_TRUE;
}

Function_Id
database_function_new(char *function_name)
{
   _Function_Id *fid = calloc(1, sizeof(*fid));
   fid->name = strdup(function_name);
   return (Function_Id) fid;
}

Eina_Bool database_class_property_add(char *classname, Function_Id foo_id)
{
   _Function_Id *fid = (_Function_Id *)foo_id;
   Class_desc *desc = eina_hash_find(_classes, classname);
   if (desc) eina_hash_add(desc->properties, fid->name, foo_id);
   return !!desc;
}

void
database_function_description_set(Function_Id foo_id, char *description)
{
   _Function_Id *fid = (_Function_Id *)foo_id;
   fid->description = strdup(description);
}

void
database_function_parameter_add(Function_Id foo_id, Eina_Bool param_in, char *type, char *name, char *description)
{
   _Function_Id *fid = (_Function_Id *)foo_id;
   if (fid)
     {
        Parameter_Desc *param = calloc(1, sizeof(*param));
        param->param_in = param_in;
        if (name) param->name = strdup(name);
        if (type) param->type = strdup(type);
        if (description) param->description = strdup(description);
        fid->params = eina_list_append(fid->params, param);
     }
}

Eina_Bool eolian_show()
{
   eina_hash_foreach(_classes, _class_print, NULL);
   return EINA_TRUE;
}
