#include<Eina.h>
#include "database.h"
#define ERRME(x) { printf(x); return 1; }

char *ch_parser_block_find(Eina_Strbuf *str, char *blockname);
Eina_Bool ch_parser_block_find_in(Eina_Strbuf *block, char *str);
Eina_Bool ch_parser_block_add(Eina_Strbuf *block, char *str, char *laststr);

char *ch_parser_eo_class_c_find(Eina_Strbuf *str, char *classname);
char *ch_parser_eo_class_h_find(Eina_Strbuf *str, char *classname);

Eina_Bool ch_parser_eo_class_h_method_add(Eina_Strbuf *str, char *classname, Function_Id func);
Eina_Bool ch_parser_eo_class_c_method_add(Eina_Strbuf *str, char *classname, Function_Id func);

Eina_Bool ch_parser_eo_class_h_add(Eina_Strbuf *str, char *classname);
Eina_Bool ch_parser_eo_class_c_add(Eina_Strbuf *str, char *classname);

int main(int argc,void **argv)
{
    char *teststr = NULL;

    if (argc<2) ERRME("No parameters \n");

    FILE* fp = fopen(argv[1], "r");
    if (!fp) ERRME("Cant open file\n");

    fseek(fp, 0, SEEK_END);
    int f_size = ftell(fp);
    if (f_size <= 0) ERRME("Cant read file\n");

    fseek(fp, 0, SEEK_SET);
    teststr = malloc(f_size);
    if (!teststr) ERRME("Cant allocate memory for file\n");

    fread(teststr,1,f_size,fp);
    teststr[f_size] = '\0';

    printf ("File was loaded succesefully\n");

    return 0;
}