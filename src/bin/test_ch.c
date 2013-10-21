#include<Eina.h>
#include "database.h"
#define ERRME(x) { printf(x); return 1; }

const char *ch_parser_block_find(const char *str, const char *blockname)
{
   const char *bstr = str;
   if (blockname) bstr = strstr(str, blockname);
   bstr = strchr(bstr, '{');
   return bstr;
}

Eina_Bool ch_parser_block_find_in(const char *block, const char *str)
{
   char *ret = NULL;
   char *end = strchr(block, '}');
   if (end) ret = strstr(block, str);
   return (ret < end) ? ret : EINA_FALSE; 
}

Eina_Bool ch_parser_block_add(Eina_Strbuf *block, size_t blockpos, const char *str, const char *laststr)
{
   const char *pstart = eina_strbuf_string_get(block) + blockpos;
   char *pend = strstr(pstart, laststr);
   if (!pend) return EINA_FALSE;
   
   char *spstart = pend;
   int spaces = 0;
   while (*(--spstart)!='\n') spaces++;
   
   size_t inspos = blockpos + (spstart - pstart) + 1;
   
   eina_strbuf_insert_printf(block, "%*s" "%s,\n", inspos, spaces, " ", str);
   return EINA_TRUE;
}

char *ch_parser_eo_class_c_find(Eina_Strbuf *str, char *classname);
char *ch_parser_eo_class_h_find(Eina_Strbuf *str, char *classname);

Eina_Bool ch_parser_eo_class_h_method_add(Eina_Strbuf *str, char *classname, Function_Id func);
Eina_Bool ch_parser_eo_class_c_method_add(Eina_Strbuf *str, char *classname, Function_Id func);

Eina_Bool ch_parser_eo_class_h_add(Eina_Strbuf *str, char *classname);
Eina_Bool ch_parser_eo_class_c_add(Eina_Strbuf *str, char *classname);

int main(int argc,char **argv)
{
    char *teststr = NULL;
    Eina_Strbuf *sbuff;

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
    
    sbuff = eina_strbuf_new();
    eina_strbuf_append(sbuff, teststr);
    
    printf ("\n\n\n%s\n\n\n", eina_strbuf_string_get(sbuff));
    
    const char *bl = ch_parser_block_find(eina_strbuf_string_get(sbuff), "enum");
    size_t bpos = bl - eina_strbuf_string_get(sbuff);
    ch_parser_block_add(sbuff, bpos, "Hehe", "ELM_OBJ_BUTTON_SUB_ID_LAST" );
    
    printf ("\n\n\n%s\n\n\n", eina_strbuf_string_get(sbuff));
    
    bl = ch_parser_block_find(eina_strbuf_string_get(sbuff), "Duh");
    printf ("Found %s - %s\n", "1", ch_parser_block_find_in(bl, "1") ? "Yes" : "No");
    printf ("Found %s - %s\n", "4", ch_parser_block_find_in(bl, "4") ? "Yes" : "No");
    printf ("Found %s - %s\n", "3", ch_parser_block_find_in(bl, "3") ? "Yes" : "No");
    
    return 0;
}