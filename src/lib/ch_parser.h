#ifndef CH_PARSER_H
#define CH_PARSER_H
#include<Eina.h>
#include "database.h"

char *ch_parser_block_find(Eina_Strbuf *str, char *blockname);
Eina_Bool ch_parser_block_find_in(Eina_Strbuf *block, char *str);
Eina_Bool ch_parser_block_add(Eina_Strbuf *block, char *str, char *laststr);

char *ch_parser_eo_class_c_find(Eina_Strbuf *str, char *classname);
char *ch_parser_eo_class_h_find(Eina_Strbuf *str, char *classname);

Eina_Bool ch_parser_eo_class_h_method_add(Eina_Strbuf *str, char *classname, Function_Id func);
Eina_Bool ch_parser_eo_class_c_method_add(Eina_Strbuf *str, char *classname, Function_Id func);

Eina_Bool ch_parser_eo_class_h_add(Eina_Strbuf *str, char *classname);
Eina_Bool ch_parser_eo_class_c_add(Eina_Strbuf *str, char *classname);

#endif
