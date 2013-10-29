#ifndef DATABASE_H
#define DATABASE_H

typedef struct _Function_Id* Function_Id;
typedef struct _Parameter_Desc* Parameter_Desc;

typedef enum
{
   UNRESOLVED,
   PROPERTY_FUNC,
   SET,
   GET,
   METHOD_FUNC,
   CONSTRUCTOR,
   DESTRUCTOR
} Function_Type;

typedef enum
{
   IN_PARAM,
   OUT_PARAM,
   INOUT_PARAM
} Parameter_Dir;

/* Add a class in the database */
Eina_Bool database_class_add(const char *class_name);

/* Add a class from the database */
Eina_Bool
database_class_del(char *class_name);

/* Returns the list of class names of the database */
const Eina_List *database_class_names_list_get();

/* Indicates if a class exists in the database */
Eina_Bool database_class_exists(const char *class_name);

/* Add an inherit class name to a class */
Eina_Bool database_class_inherit_add(const char *class_name, char *inherit_class_name);

/* Returns c-macro for current class like EVAS_OBJ_IMAGE_CLASS */
const char* database_class_macro_get(const  char *class_name);

/* Set c-macro of class. */
void
database_class_macro_set(const char *class_name, const char *macro);

/* Get description of class. */
const char*
database_class_description_get(const char *class_name);

/* Set description of class. */
void
database_class_description_set(const char *class_name, const char *description);

/* Returns the list of inherit class names of a certain class */
const Eina_List *database_class_inherits_list_get(char *class_name);

/* Returns a list of the function ids of a type in a class */
const Eina_List *database_class_functions_list_get(char *class_name, Function_Type foo_type);

/* Create a function */
Function_Id database_function_new(const char *function_name, Function_Type foo_type);

/* Get a type of a function */
Function_Type database_function_type_get(Function_Id function_id);

/* Add a function to a class */
Eina_Bool database_class_function_add(const char *classname, Function_Id foo_id);

/* Get a name of a function */
const char *database_function_name_get(Function_Id function_id);

Eina_Bool database_class_function_exists(const char *class_name, const char *func_name);

/* Add a description to a function */
void database_function_description_set(Function_Id function_id, const char *key, const char *description);

/* Get a description of a function */
const char *database_function_description_get(Function_Id function_id, const char *key);

/* Add a parameter to a function */
Parameter_Desc database_function_parameter_add(Function_Id foo_id, Parameter_Dir param_dir, const char *type, const char *name, const char *description);

/* Returns the list of Parameter_Desc of a function */
const Eina_List *database_parameters_list_get(Function_Id foo_id);

/* Get parameter information */
void
database_parameter_information_get(Parameter_Desc param_desc, Parameter_Dir *param_dir, char **type, char **name, char **description);
/* Need to add API for callbacks and implements */

#endif
