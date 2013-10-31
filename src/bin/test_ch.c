#include <Eina.h>
#include <Ecore_File.h>
#include "Eolian.h"
#include "database.h"
#include <ctype.h>
#include <string.h>

#define EO_SUFFIX ".eo"

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
Eina_Bool ch_parser_eo_class_h_method_add(Eina_Strbuf *text, const char *classname, EINA_UNUSED Function_Id func)
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
Eina_Bool ch_parser_eo_class_c_method_add(Eina_Strbuf *text, const char *classname, EINA_UNUSED Function_Id func)
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

static char* _function_define_string_get(Function_Id func, char *classname)
{
   const char *str_dir[] = {"in", "out", "inout"}; 
   const Eina_List *l;
   void *data;
   
   Eina_Strbuf *ret = eina_strbuf_new();
   const char *fnname = database_function_name_get(func);

   eina_strbuf_append_printf(ret, "/**\n * @def %s\n *\n * %s\n *\n", fnname,
                      database_function_description_get(func, "comment"));
   
   Eina_Strbuf *param = eina_strbuf_new();
   Eina_Strbuf *typecheck = eina_strbuf_new();                  
   EINA_LIST_FOREACH(database_parameters_list_get(func), l, data)
     {
        char *pname;
        char *ptype;
        Parameter_Dir pdir;
        database_parameter_information_get((Parameter_Desc)data, &pdir, &ptype, &pname, NULL);
        eina_strbuf_append_printf(ret, " * @param[%s] %s\n", str_dir[(int)pdir], pname);
        
        if (eina_strbuf_length_get(param)) eina_strbuf_append(param, ", ");
        eina_strbuf_append(param, pname);
        eina_strbuf_append_printf(typecheck, ", EO_TYPECHECK(%s%s, %s)", 
                                  ptype,
                                  (pdir==IN_PARAM) ? "" : "*",
                                  pname);
     }
   eina_strbuf_append(ret, " *\n */\n");
   eina_strbuf_append_printf(ret, "#define %s_%s(%s) ", classname, fnname, eina_strbuf_string_steal(param));
   _strbuf_uppercase_append(ret, classname);
   eina_strbuf_append(ret, "_ID(");
   _strbuf_uppercase_append(ret, classname);
   eina_strbuf_append(ret, "_SUB_ID_");
   _strbuf_uppercase_append(ret, fnname);
   eina_strbuf_append(ret, ")");
   eina_strbuf_append(ret, eina_strbuf_string_steal(typecheck));
   
   return eina_strbuf_string_steal(ret);
}
//#define elm_obj_button_admits_autorepeat_get(ret) ELM_OBJ_BUTTON_ID(ELM_OBJ_BUTTON_SUB_ID_ADMITS_AUTOREPEAT_GET), EO_TYPECHECK(Eina_Bool *, ret)
char* ch_parser_eo_header_generate(char *classname)
{
   const Eina_List *l;
   void *data;
   
   if (!database_class_exists(classname)) return NULL;
   
   char *cap_class = strdup(classname);
   eina_str_toupper(&cap_class);
   
   Eina_Strbuf * str_decl = eina_strbuf_new();
   eina_strbuf_append_printf(str_decl, "#define %s_CLASS %s_class_get()\n\n", 
                             cap_class, classname);
   eina_strbuf_append_printf(str_decl, "const Eo_Class *%s_class_get(void) EINA_CONST;\n\n", 
                             classname);
   eina_strbuf_append_printf(str_decl, "extern EAPI Eo_Op %s_BASE_ID;\n\n", cap_class);
   eina_strbuf_append_printf(str_decl, "enum\n{\n\n");
   
   Eina_Strbuf *cap_buff = eina_strbuf_new();
   Eina_Strbuf *func_defs = eina_strbuf_new();
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, METHOD_FUNC), l, data)
     {
        eina_strbuf_reset(cap_buff);
        _strbuf_uppercase_append(cap_buff, database_function_name_get((Function_Id)data));
        eina_strbuf_append_printf(str_decl, "   %s_SUB_ID_%s,\n", cap_class,
                                          eina_strbuf_string_get(cap_buff));
        eina_strbuf_append_printf(func_defs, "\n%s\n", _function_define_string_get((Function_Id)data, classname));
     }
   eina_strbuf_append_printf(str_decl, "   %s_SUB_ID_LAST\n};", cap_class);
   eina_strbuf_append_printf(str_decl, "\n%s\n", eina_strbuf_string_get(func_defs));
   
   eina_strbuf_free(cap_buff);
   free(cap_class);
   
   return eina_strbuf_string_steal(str_decl);
}

/*
#define ELM_OBJ_BUTTON_CLASS elm_obj_button_class_get()

const Eo_Class *elm_obj_button_class_get(void) EINA_CONST;

extern EAPI Eo_Op ELM_OBJ_BUTTON_BASE_ID;

enum
{
   ELM_OBJ_BUTTON_SUB_ID_ADMITS_AUTOREPEAT_GET,
   ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_SET,
   ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_GET,
   ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_INITIAL_TIMEOUT_SET,
   ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_INITIAL_TIMEOUT_GET,
   ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_GAP_TIMEOUT_SET,
   ELM_OBJ_BUTTON_SUB_ID_AUTOREPEAT_GAP_TIMEOUT_GET,
   ELM_OBJ_BUTTON_SUB_ID_LAST
};

#define ELM_OBJ_BUTTON_ID(sub_id) (ELM_OBJ_BUTTON_BASE_ID + sub_id)


/**
 * @def elm_obj_button_admits_autorepeat_get
 * @since 1.8
 *
 * No description supplied by the EAPI.
 *
 * @param[out] ret
 *
 * @see elm_button_admits_autorepeat_get
 *
 * @ingroup Button
 */
//#define elm_obj_button_admits_autorepeat_get(ret) ELM_OBJ_BUTTON_ID(ELM_OBJ_BUTTON_SUB_ID_ADMITS_AUTOREPEAT_GET), EO_TYPECHECK(Eina_Bool *, ret)


char* ch_parser_eo_source_generate(char *classname)
{
   const Eina_List *l;
   void *data;
   
   if (!database_class_exists(classname)) return NULL;
   
   char *cap_class = strdup(classname);
   eina_str_toupper(&cap_class);
   
   Eina_Strbuf * str_decl = eina_strbuf_new();
   eina_strbuf_append_printf(str_decl, "EO_DEFINE_CLASS(%s_class_get, &class_desc, ", classname);
   EINA_LIST_FOREACH(database_class_inherits_list_get(classname), l, data)
     {
        _strbuf_uppercase_append(str_decl, (char*)data);
        eina_strbuf_append(str_decl, "_CLASS, "); 
     }
   eina_strbuf_append(str_decl, "NULL);");
   
   Eina_Strbuf * str_desc = eina_strbuf_new();
   eina_strbuf_append_printf(str_desc, "static const Eo_Class_Description class_desc = {\n"
                                       "     EO_VERSION,\n" 
                                       "     \"%s\",\n"
                                       "     EO_CLASS_TYPE_REGULAR,\n"
                                       "     EO_CLASS_DESCRIPTION_OPS(&%s_BASE_ID, op_desc, %s_SUB_ID_LAST),\n"
                                       "     NULL,\n"
                                       "     _class_constructor,\n"
                                       "     NULL\n};",
                                       classname, cap_class, cap_class);
   
   Eina_Strbuf * str_op = eina_strbuf_new();
   eina_strbuf_append(str_op, "static const Eo_Op_Description op_desc[] = {\n");
   
   Eina_Strbuf * str_constr = eina_strbuf_new();
   eina_strbuf_append(str_constr, "static void\n_class_constructor(Eo_Class *klass)\n{\n"
                                  "   const Eo_Op_Func_Description func_desc[] = {\n");
   
   Eina_Strbuf *cap_buff = eina_strbuf_new();
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, METHOD_FUNC), l, data)
     {
        eina_strbuf_reset(cap_buff);
        _strbuf_uppercase_append(cap_buff, database_function_name_get((Function_Id)data));
        eina_strbuf_append_printf(str_op, 
                  "     EO_OP_DESCRIPTION(%s_SUB_ID_%s, \"%s\"),\n", 
                  cap_class,
                  eina_strbuf_string_get(cap_buff), 
                  database_function_description_get((Function_Id)data,"comment"));
        
        eina_strbuf_append_printf(str_constr, 
                  "        EO_OP_FUNC(%s_ID(%s_SUB_ID_%s), _%s),\n", 
                  cap_class,
                  cap_class,
                  eina_strbuf_string_get(cap_buff), 
                  database_function_name_get((Function_Id)data));          
     }
   eina_strbuf_append(str_op, "     EO_OP_DESCRIPTION_SENTINEL\n};");
   eina_strbuf_append(str_constr, "        EO_OP_FUNC_SENTINEL\n   };\n"
                                  "   eo_class_funcs_set(klass, func_desc);\n}");

   Eina_Strbuf *str_ret = eina_strbuf_new();
   eina_strbuf_append_printf(str_ret, "EAPI Eo_Op %s_BASE_ID = EO_NOOP;\n", cap_class);
   eina_strbuf_append_printf(str_ret, "\n%s\n", eina_strbuf_string_steal(str_constr));
   eina_strbuf_append_printf(str_ret, "\n%s\n", eina_strbuf_string_steal(str_op));
   eina_strbuf_append_printf(str_ret, "\n%s\n", eina_strbuf_string_steal(str_desc));
   eina_strbuf_append_printf(str_ret, "\n%s\n", eina_strbuf_string_steal(str_decl));
   
   eina_strbuf_free(cap_buff);
   free(cap_class);
   
   return eina_strbuf_string_steal(str_ret);
}

int main(int argc, char **argv)
{
   eina_init();
   int i;
   Eina_Bool help = EINA_FALSE, show = EINA_FALSE;
   Eina_List *files = NULL, *itr;
   for(i = 1; i < argc; i++)
     {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
          {
             help = EINA_TRUE;
             continue;
          }
        if (!strcmp(argv[i], "--show"))
          {
             show = EINA_TRUE;
             continue;
          }
        /* Directory parameter found. */
        if (!strcmp(argv[i], "-d") && (i < (argc-1)))
          {
             i++;
             char *dir = ecore_file_realpath(argv[i]);
             if (strlen(dir) != 0)
               {
                  if (ecore_file_is_dir(dir))
                    {
                       Eina_List *dir_files;
                       char *file;
                       /* Get all files from directory. Not recursively!!!*/
                       dir_files = ecore_file_ls(dir);
                       EINA_LIST_FREE(dir_files, file)
                         {
                            char *filepath = malloc(strlen(dir) + 1 + strlen(file) + 1);
                            sprintf(filepath, "%s/%s", dir, file);
                            if ((!ecore_file_is_dir(filepath)) && eina_str_has_suffix(filepath, EO_SUFFIX))
                              {
                                 /* Allocated string will be freed during deletion of "files" list. */
                                 files = eina_list_append(files, strdup(filepath));
                              }
                            free(filepath);
                            free(file);
                         }
                    }
               }
             free(dir);
             continue;
          }
        if (!strcmp(argv[i], "-f") && (i < (argc-1)))
          {
             i++;
             char *realpath = ecore_file_realpath(argv[i]);
             if (strlen(realpath) != 0)
               {
                  if (!ecore_file_is_dir(realpath))
                    {
                       if (eina_str_has_suffix(realpath, EO_SUFFIX))
                         files = eina_list_append(files, strdup(realpath));
                    }
               }
             free(realpath);
             continue;
          }
     }

   if (!files || help)
     {
        printf("Usage: %s [-h/--help] [--show] input files\n", argv[0]);
        return 0;
     }

   eolian_database_init();
   char *filename;
   EINA_LIST_FOREACH(files, itr, filename)
     {
        if (!eolian_eo_file_parse(filename))
          {
             printf("Error during parsing file %s\n", filename);
          }
     }

   if (show)
     {
        eolian_show();
     }
     
   printf("Hello!\n");
   printf ("%s\n",ch_parser_eo_header_generate("Evas_Object_Image"));
   
   EINA_LIST_FREE(files, filename)
      free(filename);
   eolian_database_shutdown();
   eina_shutdown();
   return 0;
}

/*
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
   
    //const char *bl = ch_parser_block_find(eina_strbuf_string_get(sbuff), "enum");
    //if (bl)
    //{
    //   size_t bpos = bl - eina_strbuf_string_get(sbuff);
    //   ch_parser_block_add(sbuff, bpos, "Hehe", "ELM_OBJ_BUTTON_SUB_ID_LAST" );
    //}
    
    //printf ("\n\n\n%s\n\n\n", eina_strbuf_string_get(sbuff));
    
    //bl = ch_parser_block_find(eina_strbuf_string_get(sbuff), "Duh");
    //printf ("Found %s - %s\n", "1", ch_parser_block_find_in(bl, "1") ? "Yes" : "No");
    //printf ("Found %s - %s\n", "4", ch_parser_block_find_in(bl, "4") ? "Yes" : "No");
    //printf ("Found %s - %s\n", "3", ch_parser_block_find_in(bl, "3") ? "Yes" : "No");
    //
    //printf ("Found class h - %s\n",ch_parser_eo_class_h_find(sbuff, "elm_obj_button") ? "Yes" : "No");
    //printf ("Found class c - %s\n",ch_parser_eo_class_c_find(sbuff, "elm_obj_button") ? "Yes" : "No");
    
    return 0;
}
*/
