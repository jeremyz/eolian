#ifndef CH_PARSER_H
#define CH_PARSER_H
#include<Eina.h>
#include "database.h"

// Append eo declaration of a class to an existing h file text
Eina_Bool ch_parser_header_append(Eina_Strbuf *header, char *classname);

// Generate c source text for an eo class
char* ch_parser_eo_source_generate(char *classname);

// Generate c header text for eo class
char* ch_parser_eo_header_generate(char *classname);

// Generate legacy EAPI header text for eo class
char* ch_parser_legacy_header_generate(char *classname);

#endif
