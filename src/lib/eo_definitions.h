#ifndef __EO_DEFINITIONS_H__
#define __EO_DEFINITIONS_H__

#include <Eina.h>

/* RET */

typedef struct _eo_ret_def
{
   const char *type;
   const char *comment;
} Eo_Ret_Def;

/* PARAM */

typedef enum _param_way
{
   PARAM_IN,
   PARAM_OUT,
   PARAM_INOUT,
   PARAM_WAY_LAST
} Param_Way;

typedef struct _eo_param_def
{
   Param_Way way;
   const char *type;
   const char *name;
   const char *comment;
} Eo_Param_Def;

/* ACCESSOR */

typedef enum _eo_accessor_type
{
   SETTER,
   GETTER,
   ACCESSOR_TYPE_LAST
} Eo_Accessor_Type;

typedef struct _eo_accessor_def
{
   Eo_Accessor_Type type;
   Eo_Ret_Def ret;
   const char *comment;
   const char* legacy;
} Eo_Accessor_Def;

/* PROPERTY */

typedef struct _eo_property_def
{
   const char *name;
   Eina_List *params;
   Eina_List *accessors;
} Eo_Property_Def;

/* METHOD */

typedef struct _eo_method_def
{
   Eo_Ret_Def ret;
   const char *name;
   const char *comment;
   Eina_List *params;
   const char* legacy;
} Eo_Method_Def;

/* CLASS */

typedef struct _eo_class_def
{
   const char *name;
   const char *comment;
   Eina_List *inherits;
   Eina_List *signals;
   Eina_List *constructors;
   Eina_List *properties;
   Eina_List *methods;
} Eo_Class_Def;

void eo_definitions_ret_free(Eo_Ret_Def *ret);

void eo_definitions_param_free(Eo_Param_Def *param);

void eo_definitions_accessor_free(Eo_Accessor_Def *accessor);

void eo_definitions_property_def_free(Eo_Property_Def *prop);

void eo_definitions_method_def_free(Eo_Method_Def *prop);

void eo_definitions_class_def_free(Eo_Class_Def *kls);

#endif /* __EO_DEFINITIONS_H__ */
