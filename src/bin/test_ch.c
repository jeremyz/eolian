#include <Eina.h>
#include <Ecore_File.h>
#include "Eolian.h"
#include "database.h"
#include <ctype.h>
#include <string.h>

#define EO_SUFFIX ".eo"

const char 
tmpl_eo_src[] = "\
EAPI Eo_Op @#CLASS_BASE_ID = EO_NOOP;\n\
\n\
static void\n\
_class_constructor(Eo_Class *klass)\n\
{\n\
   const Eo_Op_Func_Description func_desc[] = {@#list_func\n\
        EO_OP_FUNC_SENTINEL\n\
   };\n\
   eo_class_funcs_set(klass, func_desc);\n\
}\n\
\n\
static const Eo_Op_Description op_desc[] = {@#list_op\n\
     EO_OP_DESCRIPTION_SENTINEL\n\
};\n\
\n\
static const Eo_Class_Description class_desc = {\n\
     EO_VERSION,\n\
     \"@#class\",\n\
     EO_CLASS_TYPE_REGULAR,\n\
     EO_CLASS_DESCRIPTION_OPS(&@#CLASS_BASE_ID, op_desc, @#CLASS_SUB_ID_LAST),\n\
     NULL,\n\
     sizeof(@#class_Smart_Data),\n\
     _class_constructor,\n\
     NULL\n\
};\n\
\n\
EO_DEFINE_CLASS(@#class_class_get, &class_desc, @#list_inheritNULL);\
";

const char 
tmpl_eo_op_desc[] = "\n     EO_OP_DESCRIPTION(@#CLASS_SUB_ID_@#FUNC, \"@#desc\"),";

const char 
tmpl_eo_func_desc[] = "\n        EO_OP_FUNC(@#CLASS_ID(E@#CLASS_SUB_@#FUNC), _@#func),";

const char 
tmpl_eo_header[] = "\
#define @#CLASS_CLASS @#class_class_get()\n\
\n\
const Eo_Class *@class_class_get(void) EINA_CONST;\n\
\n\
extern EAPI Eo_Op @#CLASS_BASE_ID;\n\
\n\
enum\n\
{@#list_subid\n\
   @#CLASS_SUB_ID_LAST\n\
};\n\
\n\
#define @#CLASS_ID(sub_id) (@#CLASS_BASE_ID + sub_id)\n\
";

const char 
tmpl_eo_subid[] = "\n   @#CLASS_SUB_ID_@#FUNC,";

const char 
tmpl_eo_subid_apnd[] = "   @#CLASS_SUB_ID_@#FUNC,\n";

const char
tmpl_eo_funcdef[] = "\n\
/**\n\
 * @def @#class_@#func\n\
 *\n\
 * @#desc\n\
 *\n\
@#list_desc_param\
 *\n\
 */\n\
#define @#class_@#func(@#list_param) @#CLASS_ID(@#CLASS_SUB_ID_@#FUNC) @#list_typecheck\n\
";

const char
tmpl_eo_pardesc[] =" * @param[%s] %s\n";

static const char*
_template_fill(Eina_Strbuf *buf, const char* templ, const char* classname, const char *funcname, Eina_Bool reset)
{
   if (reset) eina_strbuf_reset(buf);
   if (templ) eina_strbuf_append(buf, templ);
   
   // TODO Cache ?
   char* capclass = strdup(classname);
   eina_str_toupper(&capclass);
   char* capfunc = strdup(funcname);
   eina_str_toupper(&capfunc);
   
   eina_strbuf_replace_all(buf, "@#class", classname);
   eina_strbuf_replace_all(buf, "@#func", funcname);
   
   eina_strbuf_replace_all(buf, "@#CLASS", capclass);
   eina_strbuf_replace_all(buf, "@#FUNC", capfunc);
   
   free(capfunc);
   free(capclass);
   
   return eina_strbuf_string_get(buf);
}

static void 
_strbuf_uppercase_append(Eina_Strbuf *sbuff, const char *uppstr)
{
   size_t i;
   for(i = 0; i < strlen(uppstr); i++) 
     eina_strbuf_append_char(sbuff, toupper(uppstr[i]));
}

static char*
_nextline(char *str, unsigned int lines)
{
   if (!str) return NULL;
   
   char *ret = str;
   while ((lines--) && *ret)
     {
        ret= strchr(ret, '\n');
        if (ret) ret++;
     }
   return ret;
}

static char*
_startline(char *str, char *pos)
{
   if (!str || !pos) return NULL;
   
   char *ret =  pos;
   while ((ret > str) && (*(ret-1)!='\n')) ret--;
  
   return ret;
}

//TODO change replacement
static char*
_class_h_find(Eina_Strbuf *str, const char *classname)
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

static char* 
_eo_fundef_generate(Function_Id func, char *classname, Function_Type ftype)
{
   const char *str_dir[] = {"in", "out", "inout"}; 
   const Eina_List *l;
   void *data;
   char funcname[0xFF];
   char descname[0xFF];
   char *fsuffix = "";
   if (ftype == GET) fsuffix = "_get";
   if (ftype == SET) fsuffix = "_set";
   
   sprintf (funcname,"%s%s", database_function_name_get(func), fsuffix);
   sprintf (descname,"comment%s", fsuffix);
   const char *funcdesc = database_function_description_get(func, descname);
   
   Eina_Strbuf *str_func = eina_strbuf_new();
   _template_fill(str_func, tmpl_eo_funcdef, classname, funcname, EINA_TRUE);
   eina_strbuf_replace_all(str_func, "@#desc", funcdesc);
   
   Eina_Strbuf *str_par = eina_strbuf_new();
   Eina_Strbuf *str_pardesc = eina_strbuf_new();
   Eina_Strbuf *str_typecheck = eina_strbuf_new();             
   
   EINA_LIST_FOREACH(database_parameters_list_get(func), l, data)
     {
        char *pname;
        char *ptype;
        Parameter_Dir pdir;
        database_parameter_information_get((Parameter_Desc)data, &pdir, &ptype, &pname, NULL);
        
        const char *dir_str = str_dir[(int)pdir];
        if (ftype == GET) dir_str = "out";
        if (ftype == SET) dir_str = "in";
        eina_strbuf_append_printf(str_pardesc, tmpl_eo_pardesc, dir_str, pname);
        
        if (eina_strbuf_length_get(str_par)) eina_strbuf_append(str_par, ", ");
        eina_strbuf_append(str_par, pname);
        
        eina_strbuf_append_printf(str_typecheck, ", EO_TYPECHECK(%s, %s)", ptype, pname);
     }
   
   eina_strbuf_replace_all(str_func, "@#list_param", eina_strbuf_string_get(str_par));
   eina_strbuf_replace_all(str_func, "@#list_desc_param", eina_strbuf_string_get(str_pardesc));
   eina_strbuf_replace_all(str_func, "@#list_typecheck", eina_strbuf_string_get(str_typecheck));
   
   eina_strbuf_free(str_par);
   eina_strbuf_free(str_pardesc);
   eina_strbuf_free(str_typecheck);
   
   return eina_strbuf_string_steal(str_func);
}

char*
ch_parser_eo_header_generate(char *classname)
{
   const Function_Type ftype_order[] = {PROPERTY_FUNC, METHOD_FUNC};
   const Eina_List *l;
   void *data;
   char tmpstr[0x1FF];
   
   if (!database_class_exists(classname)) return NULL;
   
   Eina_Strbuf * str_hdr = eina_strbuf_new();
   _template_fill(str_hdr, tmpl_eo_header, classname, "", EINA_TRUE);
   
   Eina_Strbuf *str_subid = eina_strbuf_new();
   
   int i;
   for (i = 0; i < 2; i++)
      EINA_LIST_FOREACH(database_class_functions_list_get(classname, ftype_order[i]), l, data)
        {
           const Function_Type ftype = database_function_type_get((Function_Id)data);
           const char *funcname = database_function_name_get((Function_Id)data);
           Eina_Bool prop_read = (ftype == PROPERTY_FUNC || ftype == GET ) ? EINA_TRUE : EINA_FALSE ;
           Eina_Bool prop_write = (ftype == PROPERTY_FUNC || ftype == SET ) ? EINA_TRUE : EINA_FALSE ;
           
           if (!prop_read && !prop_write)
             {
                _template_fill(str_subid, tmpl_eo_subid, classname, funcname, EINA_FALSE);
                char *funcdef = _eo_fundef_generate((Function_Id)data, classname, UNRESOLVED);
                eina_strbuf_append(str_hdr, funcdef);
                free(funcdef);
             }
           if (prop_read)
             {
                sprintf(tmpstr, "%s_get", funcname);
                _template_fill(str_subid, tmpl_eo_subid, classname, tmpstr, EINA_FALSE);
                char *funcdef = _eo_fundef_generate((Function_Id)data, classname, GET);
                eina_strbuf_append(str_hdr, funcdef);
                free(funcdef);
             }
           if (prop_write)
             {
                sprintf(tmpstr, "%s_set", funcname);
                _template_fill(str_subid, tmpl_eo_subid, classname, tmpstr, EINA_FALSE);
                char *funcdef = _eo_fundef_generate((Function_Id)data, classname, SET);
                eina_strbuf_append(str_hdr, funcdef);
                free(funcdef);
             }
        }
   
   eina_strbuf_replace_all(str_hdr, "@#list_subid", eina_strbuf_string_get(str_subid));
   eina_strbuf_free(str_subid);
   
   return eina_strbuf_string_steal(str_hdr);
}

char* 
ch_parser_eo_source_generate(char *classname)
{
   const Eina_List *l;
   void *data;
   
   if (!database_class_exists(classname)) return NULL;
   
   Eina_Strbuf *tmpbuf = eina_strbuf_new();
   Eina_Strbuf *str_src = eina_strbuf_new();
   Eina_Strbuf *str_op = eina_strbuf_new();
   Eina_Strbuf *str_func = eina_strbuf_new();
   
   _template_fill(str_src, tmpl_eo_src, classname, "", EINA_TRUE);
   
   EINA_LIST_FOREACH(database_class_inherits_list_get(classname), l, data)
     {
        _template_fill(tmpbuf, "@#CLASS_CLASS, ", classname, "", EINA_FALSE);
     }
   eina_strbuf_replace_all(str_src, "@#list_inherit", eina_strbuf_string_get(tmpbuf));
   
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, METHOD_FUNC), l, data)
     {
        const char *funcname = database_function_name_get((Function_Id)data);
        const char *desc = database_function_description_get((Function_Id)data, "comment");
        
        _template_fill(str_func, tmpl_eo_func_desc, classname, funcname, EINA_FALSE);
        _template_fill(tmpbuf, tmpl_eo_op_desc, classname, funcname, EINA_TRUE);
        eina_strbuf_replace_all(tmpbuf, "@#desc", desc);
        eina_strbuf_append(str_op, eina_strbuf_string_get(tmpbuf));
     }
   eina_strbuf_replace_all(str_src, "@#list_func", eina_strbuf_string_get(str_func));
   eina_strbuf_replace_all(str_src, "@#list_op", eina_strbuf_string_get(str_op));
   
   eina_strbuf_free(tmpbuf);
   eina_strbuf_free(str_op);
   eina_strbuf_free(str_func);
   
   return eina_strbuf_string_steal(str_src);
}

Eina_Bool
ch_parser_header_append(Eina_Strbuf *header, char *classname)
{
   char *clsptr = _class_h_find(header, classname);
   
   if (!clsptr)
     {
        printf ("Class %s not found - append all\n", classname);
        char *hptr = ch_parser_eo_header_generate(classname);
        eina_strbuf_append_char(header, '\n');
        eina_strbuf_append(header, hptr);
        free(hptr);
        return EINA_TRUE;
     }
   
   printf ("Class %s found - searching for functions...\n", classname);
   
   char *funcdef_pos = _nextline(strstr(clsptr, "+ sub_id)"), 1);
   char *subid_pos = _startline(clsptr, strstr(clsptr, "SUB_ID_LAST"));
   
   if (!(funcdef_pos && subid_pos) || (subid_pos > funcdef_pos))
     {
       printf ("Bad insertion queues - update aborted\n"); 
       return EINA_FALSE;
     }
   
   Eina_Strbuf *str_subid = eina_strbuf_new();
   Eina_Strbuf *str_funcdef = eina_strbuf_new();
   
   const Eina_List *l;
   void *data;
   
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, METHOD_FUNC), l, data)
     {
        const char *funcname = database_function_name_get((Function_Id)data);
        char *found = strstr(clsptr, funcname);
        
        if (!found)
          {
            printf ("Method %s found - appending\n", funcname);
            
            _template_fill(str_subid, tmpl_eo_subid_apnd, classname, funcname, EINA_FALSE);
            
            char *funcdef = _eo_fundef_generate((Function_Id)data, classname, UNRESOLVED);
            eina_strbuf_append(str_funcdef, funcdef);
            free(funcdef);
          }
     }

   const char *hdstr = eina_strbuf_string_get(header);
   unsigned enum_offs = subid_pos - hdstr;
   unsigned defs_offs = funcdef_pos - hdstr + eina_strbuf_length_get(str_subid);
   eina_strbuf_insert(header, eina_strbuf_string_get(str_subid), enum_offs);
   eina_strbuf_insert(header, eina_strbuf_string_get(str_funcdef), defs_offs);
   
   eina_strbuf_free(str_subid);
   eina_strbuf_free(str_funcdef);
   
   return EINA_TRUE;
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
   
   /*
   Eina_File *fn = eina_file_open("../EvasObjectImage1.h", EINA_FALSE);
   Eina_Strbuf *hfile = eina_strbuf_new();
   eina_strbuf_append(hfile, (char*)eina_file_map_all(fn, EINA_FILE_SEQUENTIAL));
   ch_parser_header_append(hfile, "Evas_Object_Image");
   printf ("upd\n%s\n",eina_strbuf_string_get(hfile));
   eina_file_close(fn);
   */
   
   printf ("%s\n",ch_parser_eo_header_generate("elm_win"));
   
   EINA_LIST_FREE(files, filename)
      free(filename);
   eolian_database_shutdown();
   eina_shutdown();
   return 0;
}