#include<Eina.h>
#include "database.h"
#include <ctype.h>
#include <string.h>
#define ERRME(x) { printf(x); return 1; }

const char *ch_parser_block_find(const char *str, const char *blockname)
{
   //TODO Need?
   if (!str) return NULL;
   
   const char *bstr = str;
   if (blockname) bstr = strstr(str, blockname);
   if (bstr) bstr = strchr(bstr, '{');
   return bstr;
}

const char *ch_parser_block_find_in(const char *block, const char *str)
{
   //TODO Need ?
   if (!(block && str)) return NULL;
   
   char *ret = NULL;
   char *end = strchr(block, '}');
   if (end) ret = strstr(block, str);
   return (ret < end) ? ret : NULL; 
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

void _strbuf_uppercase_append(Eina_Strbuf *sbuff, const char *uppstr)
{
   size_t i;
   for(i = 0; i < strlen(uppstr); i++) 
     eina_strbuf_append_char(sbuff, toupper(uppstr[i]));
}

const char *_skipchars(const char *str, const char *chars)
{
   const char *ret = str;
   while (ret && strchr(chars, *ret)) ret++;
   return ret;
}

const char *_nextline(const char *str, unsigned int lines)
{
   const char *ret = str;
   while ((lines--) && ret)
     {
        ret= strchr(ret, '\n');
        if (ret) ret++;
     }
   return ret;
}

Eina_Strbuf *_get_str_until(const char *str, const char *stopper)
{
   const char *ch = str;
   Eina_Strbuf *ret = eina_strbuf_new();
   while (ch && !strchr(stopper, *ch))
     {
        eina_strbuf_append_char(ret, *ch);
        ch++;
     }
   return ret;
}

char *ch_parser_eo_class_c_find(Eina_Strbuf *str, const char *classname)
{
   char *ret = NULL;
   Eina_Strbuf *classreal;
   
   classreal = eina_strbuf_new();
   eina_strbuf_append_printf(classreal, "EO_DEFINE_CLASS(%s_class_get", classname);
   ret = strstr(eina_strbuf_string_get(str), eina_strbuf_string_get(classreal));
   eina_strbuf_free(classreal);
   
   return ret;
}
// Str ?
char *ch_parser_eo_class_h_find(Eina_Strbuf *str, const char *classname)
{
   char *ret = NULL;
   Eina_Strbuf *classreal;
   
   classreal = eina_strbuf_new();
   _strbuf_uppercase_append(classreal, classname);
   eina_strbuf_append(classreal, "_CLASS");
   ret = strstr(eina_strbuf_string_get(str), eina_strbuf_string_get(classreal));
   eina_strbuf_free(classreal);
   
   return ret;
}

#define TEMPERR(text, ret) { printf(text); return ret; }
#define TEMPLOG(text)  { printf(text); }

// API ?
Eina_Bool ch_parser_eo_class_h_method_add(Eina_Strbuf *text, const char *classname, Function_Id func)
{
   const char func_name[] = "my_func";
   const char func_desc[] = "La Big Function";
   
   const char *txstr = eina_strbuf_string_get(text);
   const char *pstr;
   
   Eina_Strbuf *cap_class_name = eina_strbuf_new();
   _strbuf_uppercase_append(cap_class_name,classname);
   Eina_Strbuf *cap_enum_name = eina_strbuf_new();
   eina_strbuf_append_printf(cap_enum_name,"%s_SUB_ID_", eina_strbuf_string_get(cap_class_name));
   _strbuf_uppercase_append(cap_enum_name, func_name);
   Eina_Strbuf *cap_last_name = eina_strbuf_new();
   eina_strbuf_append_printf(cap_last_name,"%s_SUB_ID_LAST", eina_strbuf_string_get(cap_class_name));
   
   pstr = ch_parser_block_find(txstr, "enum");
   ch_parser_block_add(text, pstr - txstr, eina_strbuf_string_get(cap_enum_name), 
                                           eina_strbuf_string_get(cap_last_name));
   
   //separeate fucn ?
  //txstr = eina_strbuf_string_get(text);
   //pstr = strchr(txstr, '\0');
   //if (pstr > txstr) pstr--;
   
   
   Eina_Strbuf *func_entry = eina_strbuf_new();
   //Docs
   eina_strbuf_append_printf(func_entry, "\n/**\n*@def %s_%s\n*\n%s\n*\n*/\n", classname, func_name, func_desc);
   //Eo body def
   eina_strbuf_append_printf(func_entry, "#define %s_%s(par) %s_ID(%s) Paramstruf(param)", classname, func_name, 
                             eina_strbuf_string_get(cap_class_name),
                             eina_strbuf_string_get(cap_enum_name));

   eina_strbuf_append(text, eina_strbuf_string_get(func_entry));                         
   return EINA_TRUE;
   //Free strbuf
}

// API ?
Eina_Bool ch_parser_eo_class_c_method_add(Eina_Strbuf *text, const char *classname, Function_Id func)
{
   const char *txstr = eina_strbuf_string_get(text);
   const char *pstr;
   Eina_Strbuf *str_desc, *str_opt, *str_constructor;
   
   //Def
   pstr = ch_parser_eo_class_c_find(text, classname);
   if (!pstr) TEMPERR("Class not found in file\n", EINA_FALSE);
   pstr = strchr(pstr, ',');
   pstr = _skipchars(pstr, ",& ");
   if (!pstr) TEMPERR("Couldn't parse desc name from class\n", EINA_FALSE);
   str_desc = _get_str_until(pstr, ",");
   printf("Class desk is %s \n", eina_strbuf_string_get(str_desc));
   
   //Desc
   pstr = ch_parser_block_find(txstr, eina_strbuf_string_get(str_desc));
   pstr = ch_parser_block_find_in(pstr, "EO_CLASS_DESCRIPTION_OPS");
   pstr = strchr(pstr, ',');
   pstr = _skipchars(pstr, ", ");
   str_opt = _get_str_until(pstr, ",");
   printf("Class options is %s \n", eina_strbuf_string_get(str_opt));
   
   // constr
   pstr = _nextline(pstr, 3);
   pstr = _skipchars(pstr, " ");
   str_constructor = _get_str_until(pstr, ",");
   printf("Class constructor is %s \n", eina_strbuf_string_get(str_constructor));
   
   const char func_name[] = "my_func";
   const char func_desc[] = "La Big Function";
   
   //free; Build function entry
   Eina_Strbuf *cap_func_name = eina_strbuf_new();
   eina_strbuf_append(cap_func_name,"EO_OP_FUNC(");
   _strbuf_uppercase_append(cap_func_name, classname);
   eina_strbuf_append(cap_func_name,"_ID(");
   _strbuf_uppercase_append(cap_func_name, classname);
   eina_strbuf_append(cap_func_name,"_SUB_ID_");
   _strbuf_uppercase_append(cap_func_name, func_name);
   eina_strbuf_append_printf(cap_func_name,"), _%s)", func_name);
   
   Eina_Strbuf *cap_class_name = eina_strbuf_new();
   eina_strbuf_append(cap_class_name,"EO_OP_DESCRIPTION(");
   _strbuf_uppercase_append(cap_class_name, classname);
   eina_strbuf_append(cap_class_name,"_SUB_ID_");
   _strbuf_uppercase_append(cap_class_name, func_name);
   eina_strbuf_append_printf(cap_class_name,", \"%s\")", func_desc);
   
 // EO_OP_DESCRIPTION(ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_GAP_TIMEOUT_GET, "Get the interval between each generated autorepeat event."), 
   txstr = eina_strbuf_string_get(text);
   pstr = ch_parser_block_find(txstr, eina_strbuf_string_get(str_constructor));
   pstr = ch_parser_block_find(txstr, "func_desc");
   ch_parser_block_add(text, pstr - txstr, eina_strbuf_string_get(cap_func_name), "EO_OP_FUNC_SENTINEL");
   
   txstr = eina_strbuf_string_get(text);
   pstr = ch_parser_block_find(txstr, eina_strbuf_string_get(str_opt));
   ch_parser_block_add(text, pstr - txstr,  eina_strbuf_string_get(cap_class_name), "EO_OP_DESCRIPTION_SENTINEL");
   
   // free strings;
   return EINA_TRUE;
}  

Eina_Bool ch_parser_eo_class_h_add(Eina_Strbuf *str, const char *classname);
Eina_Bool ch_parser_eo_class_c_add(Eina_Strbuf *str, const char *classname);

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
    
    printf ("Before\n------------\n");
    printf ("\n\n\n%s\n\n\n", eina_strbuf_string_get(sbuff));
    printf ("After\n------------\n");
    
    ch_parser_eo_class_h_method_add(sbuff, "elm_obj_button", NULL);
    
    printf ("\n\n\n%s\n\n\n", eina_strbuf_string_get(sbuff));
    /*
    const char *bl = ch_parser_block_find(eina_strbuf_string_get(sbuff), "enum");
    if (bl)
    {
       size_t bpos = bl - eina_strbuf_string_get(sbuff);
       ch_parser_block_add(sbuff, bpos, "Hehe", "ELM_OBJ_BUTTON_SUB_ID_LAST" );
    }
    
    printf ("\n\n\n%s\n\n\n", eina_strbuf_string_get(sbuff));
    
    bl = ch_parser_block_find(eina_strbuf_string_get(sbuff), "Duh");
    printf ("Found %s - %s\n", "1", ch_parser_block_find_in(bl, "1") ? "Yes" : "No");
    printf ("Found %s - %s\n", "4", ch_parser_block_find_in(bl, "4") ? "Yes" : "No");
    printf ("Found %s - %s\n", "3", ch_parser_block_find_in(bl, "3") ? "Yes" : "No");
    
    printf ("Found class h - %s\n",ch_parser_eo_class_h_find(sbuff, "elm_obj_button") ? "Yes" : "No");
    printf ("Found class c - %s\n",ch_parser_eo_class_c_find(sbuff, "elm_obj_button") ? "Yes" : "No");
    */
    return 0;
}