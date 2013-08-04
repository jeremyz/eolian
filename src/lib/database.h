#ifndef DATABASE_H
#define DATABASE_H

typedef struct _Function_Id* Function_Id;

Eina_Bool database_class_add(char *class_name);
Eina_Bool database_class_inherits_list_add(char *class_name, Eina_List *inherits_list);
Eina_Bool database_class_property_add(char *classname, Function_Id foo_id);

Function_Id database_function_new(char *function_name);
void database_function_description_set(Function_Id function_id, char *description);
void database_function_parameter_add(Function_Id foo_id, Eina_Bool param_in, char *type, char *name, char *description);

#endif
