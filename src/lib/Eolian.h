#ifndef EOLIAN_H
#define EOLIAN_H
#include <Eina.h>

Eina_Bool eolian_eo_file_parse(char *filename);
Eina_Bool eolian_eo_class_desc_parse(char *class_desc);

Eina_Bool eolian_database_init();
Eina_Bool eolian_show();

#endif
