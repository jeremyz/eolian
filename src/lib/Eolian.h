#ifndef EOLIAN_H
#define EOLIAN_H
#include <Eina.h>

Eina_Bool eolian_eo_file_parse(char *filename);

/* C style parsing */
# if 0
Eina_Bool eolian_eo_class_desc_parse(char *class_desc);
#endif
Eina_Bool eolian_eo_class_desc_parse_json(char *class_desc);

Eina_Bool eolian_database_init();
Eina_Bool eolian_database_shutdown();
Eina_Bool eolian_show();

#endif
