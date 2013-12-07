#ifndef EOLIAN_H
#define EOLIAN_H
#include <Eina.h>

Eina_Bool eolian_eo_file_parse(const char *filename);

Eina_Bool eolian_init();
Eina_Bool eolian_shutdown();
Eina_Bool eolian_show();

#endif
