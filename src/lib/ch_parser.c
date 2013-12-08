#include <Eina.h>
#include "Eolian.h"
#include "database.h"
#include <ctype.h>
#include <string.h>

static const char 
tmpl_eo_src[] = "\
EAPI Eo_Op @#CLASS_BASE_ID = EO_NOOP;\n\
\n\
@#list_events\n\
\n\
static const Evas_Smart_Cb_Description _smart_callbacks[] = {@#list_desc_events\n\
   {NULL, NULL}\n\
};\n\
\n\
@#list_body\
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

static const char 
tmpl_eo_op_desc[] = "\n     EO_OP_DESCRIPTION(@#CLASS_SUB_ID_@#FUNC, \"@#desc\"),";

static const char 
tmpl_eo_func_desc[] = "\n        EO_OP_FUNC(@#CLASS_ID(@#CLASS_SUB_@#FUNC), _eo_obj_@#func),";

static const char 
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

static const char 
tmpl_eo_subid[] = "\n   @#CLASS_SUB_ID_@#FUNC,";

static const char 
tmpl_eo_subid_apnd[] = "   @#CLASS_SUB_ID_@#FUNC,\n";

static const char
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

static const char
tmpl_eapi_funcdef[] = "\n\
/**\n\
 *\n\
 * @#desc\n\
 *\n\
@#list_desc_param\
 */\n\
EAPI @#type_return @#class_@#func(@#params);\n\
";

static const char
tmpl_eo_pardesc[] =" * @param[%s] %s\n";

static const char
tmpl_eapi_body[] ="\
\n\
EAPI @#ret_type\n\
@#eapi_prefix_@#func(Eo *obj@#full_params)\n\
{\n\
   @#CLASS_CHECK(obj);\n\
   @#ret_init_val\n\
   eo_do((Eo *) obj, @#class_@#func(@#eo_params));\n\
   return @#ret_val;\n\
}\n\
";

static const char
tmpl_eobind_body[] ="\
\n\
static void\n\
_eo_obj_@#func(Eo *obj, void *_pd EINA_UNUSED, va_list *list)\n\
{\n\
@#list_vars\
   _@#class_@#func(obj@#list_params);\n\
}\n\
";

static const char*
_template_fill(Eina_Strbuf *buf, const char* templ, const char* classname, const char *funcname, Eina_Bool reset)
{
   char capclass[0xFF];
   char capfunc[0xFF];
   char *p;
   
   if (reset) eina_strbuf_reset(buf);
   if (templ) eina_strbuf_append(buf, templ);
   
   strcpy(capclass, classname);
   strcpy(capfunc, funcname);
   p = capclass; eina_str_toupper(&p);
   p = capfunc; eina_str_toupper(&p);
   
   eina_strbuf_replace_all(buf, "@#class", classname);
   eina_strbuf_replace_all(buf, "@#func", funcname);
   
   eina_strbuf_replace_all(buf, "@#CLASS", capclass);
   eina_strbuf_replace_all(buf, "@#FUNC", capfunc);
   
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

static void
_eapi_decl_func_generate(Eina_Strbuf *text, char *classname, Function_Id funcid, Function_Type ftype)
{
   //TODO return value
   const char *suffix = "";
   const char *umpr = NULL;
   const char *rettype = NULL;
   char *gettype = NULL;
   const char *func_lpref = NULL;
   
   if (ftype == GET)
     {
        suffix = "_get";
        umpr = "*";
        func_lpref = database_function_data_get(funcid, LEGACY_GET);
        const Eina_List *l = database_parameters_list_get(funcid);
        if (eina_list_count(l) == 1)
          {
             void* data = eina_list_data_get(l);
             database_parameter_information_get((Parameter_Desc)data, NULL, &gettype, NULL, NULL);
          }
     }
   
   if (ftype == SET)
     {
        suffix = "_set";
        umpr = "";
        func_lpref = database_function_data_get(funcid, LEGACY_SET);
     }
     
   func_lpref = (func_lpref) ? func_lpref : database_function_data_get(funcid, LEGACY);
   func_lpref = (func_lpref) ? func_lpref : database_class_legacy_prefix_get(classname);
   
   if (!func_lpref)
     {
        printf ("Error: Generating header legacy function in no-legacy class");
        return;
     }
   
   Eina_Strbuf *fbody = eina_strbuf_new();
   Eina_Strbuf *fparam = eina_strbuf_new();
   Eina_Strbuf *descparam = eina_strbuf_new();
   
   char tmpstr[0xFF];
   sprintf (tmpstr, "%s%s", database_function_name_get(funcid), suffix);
   _template_fill(fbody, tmpl_eapi_funcdef, func_lpref, tmpstr, EINA_FALSE);
   sprintf (tmpstr, "comment%s", suffix);
   const char *desc = database_function_description_get(funcid, tmpstr);
   eina_strbuf_replace_all(fbody, "@#desc", desc);
   
   const Eina_List *l;
   void *data;
   
   rettype = (gettype) ? gettype : database_function_return_type_get(funcid);
   if (rettype && !strcmp(rettype, "void")) rettype = NULL;
    
   if (!gettype)
     {
       EINA_LIST_FOREACH(database_parameters_list_get(funcid), l, data)
         {
            char *pname;
            char *pdesc;
            char *ptype;
            Parameter_Dir pdir;
            database_parameter_information_get((Parameter_Desc)data, &pdir, &ptype, &pname, &pdesc);
            umpr = (umpr) ? umpr : ( (pdir == IN_PARAM) ? "" : "*" );
            
            if (eina_strbuf_length_get(fparam)) eina_strbuf_append(fparam, ", ");
            eina_strbuf_append_printf(fparam, "%s%s %s", ptype, umpr, pname);
            eina_strbuf_append_printf(descparam, " * @param %s\n", pname);
         }
     }
   eina_strbuf_replace_all(fbody, "@#params", eina_strbuf_string_get(fparam));
   eina_strbuf_replace_all(fbody, "@#list_desc_param", eina_strbuf_string_get(descparam));
   eina_strbuf_replace_all(fbody, "@#type_return", (rettype) ? rettype : "void");
   eina_strbuf_append(text, eina_strbuf_string_get(fbody));
   
   eina_strbuf_free(fbody);
   eina_strbuf_free(fparam);
   eina_strbuf_free(descparam);
}

char*
ch_parser_legacy_header_generate(char *classname)
{
   const Function_Type ftype_order[] = {PROPERTY_FUNC, METHOD_FUNC};
   const Eina_List *l;
   void *data;
   
   if (!database_class_exists(classname)) 
     {
        printf ("Class \"%s\" not found in database\n", classname); 
        return NULL;
     }
   
   Eina_Strbuf * str_hdr = eina_strbuf_new();
   
   int i;
   for (i = 0; i < 2; i++)
      EINA_LIST_FOREACH(database_class_functions_list_get(classname, ftype_order[i]), l, data)
        {
           const Function_Type ftype = database_function_type_get((Function_Id)data);
           Eina_Bool prop_read = (ftype == PROPERTY_FUNC || ftype == GET ) ? EINA_TRUE : EINA_FALSE ;
           Eina_Bool prop_write = (ftype == PROPERTY_FUNC || ftype == SET ) ? EINA_TRUE : EINA_FALSE ;
           
           if (!prop_read && !prop_write)
             {
                _eapi_decl_func_generate(str_hdr, classname, (Function_Id)data, UNRESOLVED);
             }
           if (prop_read)
             {
                _eapi_decl_func_generate(str_hdr, classname, (Function_Id)data, GET);
             }
           if (prop_write)
             {
                _eapi_decl_func_generate(str_hdr ,classname, (Function_Id)data, SET);
             }
        }
   return eina_strbuf_string_steal(str_hdr);
}

static const char* 
_eo_fundef_generate(Eina_Strbuf *functext, Function_Id func, char *classname, Function_Type ftype)
{
   const char *str_dir[] = {"in", "out", "inout"};
   const Eina_List *l;
   void *data;
   char funcname[0xFF];
   char descname[0xFF];
   
   char *fsuffix = "";
   if (ftype == GET) fsuffix = "_get";
   if (ftype == SET) fsuffix = "_set";
   
   sprintf (funcname, "%s%s", database_function_name_get(func), fsuffix);
   sprintf (descname, "comment%s", fsuffix);
   const char *funcdesc = database_function_description_get(func, descname);
   
   Eina_Strbuf *str_func = eina_strbuf_new();
   _template_fill(str_func, tmpl_eo_funcdef, classname, funcname, EINA_TRUE);
   eina_strbuf_replace_all(str_func, "@#desc", funcdesc);
   
   Eina_Strbuf *str_par = eina_strbuf_new();
   Eina_Strbuf *str_pardesc = eina_strbuf_new();
   Eina_Strbuf *str_typecheck = eina_strbuf_new();             
   
   const char* rettype = database_function_return_type_get(func);
   if (rettype && strcmp(rettype, "void"))
     {
        eina_strbuf_append_printf(str_pardesc, tmpl_eo_pardesc, "out", "ret");
        eina_strbuf_append(str_par, "ret");
        eina_strbuf_append_printf(str_typecheck, ", EO_TYPECHECK(%s*, ret)", rettype);
     }
   
   EINA_LIST_FOREACH(database_parameters_list_get(func), l, data)
     {
        char *pname;
        char *ptype;
        Parameter_Dir pdir;
        database_parameter_information_get((Parameter_Desc)data, &pdir, &ptype, &pname, NULL);
        if (ftype == GET) pdir = OUT_PARAM;
        if (ftype == SET) pdir = IN_PARAM;
        char *umpr = (pdir == IN_PARAM) ? "" : "*";
        
        const char *dir_str = str_dir[(int)pdir];
       
        eina_strbuf_append_printf(str_pardesc, tmpl_eo_pardesc, dir_str, pname);
        
        if (eina_strbuf_length_get(str_par)) eina_strbuf_append(str_par, ", ");
        eina_strbuf_append(str_par, pname);
        
        eina_strbuf_append_printf(str_typecheck, ", EO_TYPECHECK(%s%s, %s)", ptype, umpr, pname);
     }
   
   eina_strbuf_replace_all(str_func, "@#list_param", eina_strbuf_string_get(str_par));
   eina_strbuf_replace_all(str_func, "@#list_desc_param", eina_strbuf_string_get(str_pardesc));
   eina_strbuf_replace_all(str_func, "@#list_typecheck", eina_strbuf_string_get(str_typecheck));
   
   eina_strbuf_free(str_par);
   eina_strbuf_free(str_pardesc);
   eina_strbuf_free(str_typecheck);
   
   eina_strbuf_append(functext, eina_strbuf_string_steal(str_func));
   return eina_strbuf_string_get(functext);
}

char*
ch_parser_eo_header_generate(char *classname)
{
   const Function_Type ftype_order[] = {PROPERTY_FUNC, METHOD_FUNC};
   const Eina_List *l;
   void *data;
   char tmpstr[0x1FF];
   
   if (!database_class_exists(classname)) 
     {
        printf ("Class \"%s\" not found in database\n", classname); 
        return NULL;
     }
   
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
                _eo_fundef_generate(str_hdr, (Function_Id)data, classname, UNRESOLVED);
             }
           if (prop_read)
             {
                sprintf(tmpstr, "%s_get", funcname);
                _template_fill(str_subid, tmpl_eo_subid, classname, tmpstr, EINA_FALSE);
                _eo_fundef_generate(str_hdr, (Function_Id)data, classname, GET);
             }
           if (prop_write)
             {
                sprintf(tmpstr, "%s_set", funcname);
                _template_fill(str_subid, tmpl_eo_subid, classname, tmpstr, EINA_FALSE);
                _eo_fundef_generate(str_hdr, (Function_Id)data, classname, SET);
             }
        }
   
   eina_strbuf_replace_all(str_hdr, "@#list_subid", eina_strbuf_string_get(str_subid));
   eina_strbuf_free(str_subid);
   
   return eina_strbuf_string_steal(str_hdr);
}

static void
_eobind_func_generate(Eina_Strbuf *text, char *classname, Function_Id funcid, Function_Type ftype)
{
   const char *suffix = "";
   const char *umpr = NULL;
   
   Eina_Strbuf *fbody = eina_strbuf_new();
   Eina_Strbuf *vars = eina_strbuf_new();
   Eina_Strbuf *params = eina_strbuf_new();
   
   if (ftype == GET)
     {
        suffix = "_get";
        umpr = "*";
     }
   if (ftype == SET)
     {
        suffix = "_set";
        umpr = "";
     }
   
   char tmpstr[0xFF];
   sprintf (tmpstr, "%s%s", database_function_name_get(funcid), suffix);
   _template_fill(fbody, tmpl_eobind_body, classname, tmpstr, EINA_FALSE);
   
   const Eina_List *l;
   void *data;
   
   const char* rettype = database_function_return_type_get(funcid);
   if (rettype && strcmp(rettype, "void"))
     {
        eina_strbuf_append_printf(vars, "   %s* ret = va_arg(*list, %s*);\n", rettype, rettype);
        eina_strbuf_append_printf(params, ", ret");
     }
   
   EINA_LIST_FOREACH(database_parameters_list_get(funcid), l, data)
     {
        char *pname;
        char *ptype;
        Parameter_Dir pdir;
        database_parameter_information_get((Parameter_Desc)data, &pdir, &ptype, &pname, NULL);
        umpr = (umpr) ? umpr : ( (pdir == IN_PARAM) ? "" : "*" );
        eina_strbuf_append_printf(vars, "   %s%s %s = va_arg(*list, %s%s);\n", ptype, umpr, pname, ptype, umpr);
        eina_strbuf_append_printf(params, ", %s", pname);
     }
   
   eina_strbuf_replace_all(fbody, "@#list_vars", eina_strbuf_string_get(vars));
   eina_strbuf_replace_all(fbody, "@#list_params", eina_strbuf_string_get(params));
   eina_strbuf_append(text, eina_strbuf_string_get(fbody));
   
   eina_strbuf_free(vars);
   eina_strbuf_free(params);
   eina_strbuf_free(fbody);
}

static void
_eapi_func_generate(Eina_Strbuf *text, char *classname, Function_Id funcid, Function_Type ftype)
{
   //TODO return value
   const char *suffix = "";
   const char *umpr = NULL;
   const char *rettype = NULL;
   const char *func_lpref = NULL;
   char *gettype = NULL;
   
   if (ftype == GET)
     {
        suffix = "_get";
        umpr = "*";
        func_lpref = database_function_data_get(funcid, LEGACY_GET);
        const Eina_List *l = database_parameters_list_get(funcid);
        if (eina_list_count(l) == 1)
          {
             void* data = eina_list_data_get(l);
             database_parameter_information_get((Parameter_Desc)data, NULL, &gettype, NULL, NULL);
          }
     }
   
   if (ftype == SET)
     {
        suffix = "_set";
        umpr = "";
        func_lpref = database_function_data_get(funcid, LEGACY_SET);
     }
     
   func_lpref = (func_lpref) ? func_lpref : database_function_data_get(funcid, LEGACY);
   func_lpref = (func_lpref) ? func_lpref : database_class_legacy_prefix_get(classname);
   
   if (!func_lpref)
     {
        printf ("Error: Generating source legacy function in no-legacy class");
        return;
     }
   
   Eina_Strbuf *fbody = eina_strbuf_new();
   Eina_Strbuf *fparam = eina_strbuf_new();
   Eina_Strbuf *eoparam = eina_strbuf_new();
   
   char tmpstr[0xFF];
   sprintf (tmpstr, "%s%s", database_function_name_get(funcid), suffix);
   _template_fill(fbody, tmpl_eapi_body, classname, tmpstr, EINA_FALSE);
   eina_strbuf_replace_all(fbody, "@#eapi_prefix", func_lpref);
   
   const Eina_List *l;
   void *data;
   
   tmpstr[0] = '\0';
   
   const char* retstr = (gettype) ? gettype : database_function_return_type_get(funcid);
   if (retstr && strcmp(retstr, "void"))
     {
        rettype = retstr;
        sprintf (tmpstr, "%s ret;", rettype);
        eina_strbuf_append_printf(eoparam, "&ret");
     }
   
   if (!gettype)
   {
       EINA_LIST_FOREACH(database_parameters_list_get(funcid), l, data)
         {
            char *pname;
            char *ptype;
            Parameter_Dir pdir;
            database_parameter_information_get((Parameter_Desc)data, &pdir, &ptype, &pname, NULL);
            umpr = (umpr) ? umpr : ( (pdir == IN_PARAM) ? "" : "*" );
            eina_strbuf_append_printf(fparam, ", %s%s %s", ptype, umpr, pname);
            if (eina_strbuf_length_get(eoparam)) eina_strbuf_append(eoparam, ", ");
            eina_strbuf_append_printf(eoparam, "%s", pname);
         }
   }
   eina_strbuf_replace_all(fbody, "@#full_params", eina_strbuf_string_get(fparam));
   eina_strbuf_replace_all(fbody, "@#eo_params", eina_strbuf_string_get(eoparam));
   eina_strbuf_replace_all(fbody, "@#ret_type", (rettype) ? rettype : "void");
   eina_strbuf_replace_all(fbody, "@#ret_init_val", tmpstr); 
   eina_strbuf_replace_all(fbody, "@#ret_val", (rettype) ? "ret" : "");
   eina_strbuf_append(text, eina_strbuf_string_get(fbody));
   
   eina_strbuf_free(fbody);
   eina_strbuf_free(fparam);
   eina_strbuf_free(eoparam);
}

char* 
ch_parser_eo_source_generate(char *classname)
{
   const Eina_List *l;
   void *data;
   
   if (!database_class_exists(classname)) 
     {
        printf ("Class \"%s\" not found in database\n", classname); 
        return NULL;
     }
   
   Eina_Strbuf *tmpbuf = eina_strbuf_new();
   Eina_Strbuf *str_src = eina_strbuf_new();
   Eina_Strbuf *str_op = eina_strbuf_new();
   Eina_Strbuf *str_func = eina_strbuf_new();
   Eina_Strbuf *str_bodyf = eina_strbuf_new();
   
   _template_fill(str_src, tmpl_eo_src, classname, "", EINA_TRUE);
   
   Eina_Strbuf *str_ev_decl = eina_strbuf_new();
   Eina_Strbuf *str_ev_op = eina_strbuf_new();
   
   EINA_LIST_FOREACH(database_class_events_list_get(classname), l, data)
     {
        char *evname;
        char *evdesc;
        database_class_event_information_get((Event_Desc)data, &evname, &evdesc);
        _template_fill(str_ev_decl, "static const char SIG_@#CLASS[] = \"@#class\";\n", evname, "", EINA_FALSE);
        _template_fill(str_ev_op, "\n   {SIG_@#CLASS, \"@#func\"},", evname, evdesc, EINA_FALSE);
     }
 
   eina_strbuf_replace_all(str_src, "@#list_events", eina_strbuf_string_steal(str_ev_decl));
   eina_strbuf_replace_all(str_src, "@#list_desc_events", eina_strbuf_string_steal(str_ev_op));
   
   EINA_LIST_FOREACH(database_class_inherits_list_get(classname), l, data)
     {
        _template_fill(tmpbuf, "@#CLASS_CLASS, ", classname, "", EINA_FALSE);
     }
   eina_strbuf_replace_all(str_src, "@#list_inherit", eina_strbuf_string_get(tmpbuf));
   
   eina_strbuf_replace_all(str_src, "@#list_inherit", eina_strbuf_string_get(tmpbuf));
   
   //Implements - TODO one generate func def for all
   EINA_LIST_FOREACH(database_class_implements_list_get(classname), l, data)
     {
        char *funcname;
        char *impl_class;
        Function_Type ftype;
        database_class_implement_information_get((Implements_Desc)data, &impl_class, &funcname, &ftype);
        
        Function_Id in_meth = NULL; 
        Function_Id in_prop = NULL;
        const Eina_List *ll;
        Function_Id fnid;
        EINA_LIST_FOREACH(database_class_functions_list_get(impl_class, CONSTRUCTOR), ll, fnid)
          if (fnid && !strcmp(database_function_name_get(fnid), funcname)) in_meth = fnid;
        EINA_LIST_FOREACH(database_class_functions_list_get(impl_class, METHOD_FUNC), ll, fnid)
          if (fnid && !strcmp(database_function_name_get(fnid), funcname)) in_meth = fnid;
        EINA_LIST_FOREACH(database_class_functions_list_get(impl_class, PROPERTY_FUNC), ll, fnid)
          if (fnid && !strcmp(database_function_name_get(fnid), funcname)) in_prop = fnid;
        
        if (!in_meth && !in_prop)
          {
             printf ("Failed to generate imlementation of %s:%s - missing form super class\n", impl_class, funcname);
             return NULL;
          }
        if (in_meth && in_prop)
          {
             switch(ftype)
              {
               case PROPERTY_FUNC:
               case SET:
               case GET:
                 in_meth = NULL;
                 break;
               case METHOD_FUNC:
                 in_prop = NULL;
                 break;
               default:
                 printf ("Failed to generate imlementation of %s:%s - ambigious\n", impl_class, funcname);
                 return NULL;
              }
          } 
        if (in_meth)
          {
             _template_fill(str_func, tmpl_eo_func_desc, impl_class, funcname, EINA_FALSE);
             _eobind_func_generate(str_bodyf, classname, in_meth, UNRESOLVED);
          }
        if (in_prop)
          {
             char tmpstr[0xFF];

             if (ftype == PROPERTY_FUNC) ftype = database_function_type_get(in_prop);
            
             Eina_Bool prop_read = ( ftype == SET ) ? EINA_FALSE : EINA_TRUE;
             Eina_Bool prop_write = ( ftype == GET ) ? EINA_FALSE : EINA_TRUE;
               
             if (prop_read)
               {
                  sprintf(tmpstr, "%s_get", funcname);
                  _template_fill(str_func, tmpl_eo_func_desc, impl_class, tmpstr, EINA_FALSE);
                  _eobind_func_generate(str_bodyf, classname, in_prop, GET);
               }
                
             if (prop_write)
               {
                  sprintf(tmpstr, "%s_set", funcname);
                  _template_fill(str_func, tmpl_eo_func_desc, impl_class, tmpstr, EINA_FALSE);
                  _eobind_func_generate(str_bodyf, classname, in_prop, SET);
               }
          }
     }
   
   //Constructors
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, CONSTRUCTOR), l, data)
     {
        const char *funcname = database_function_name_get((Function_Id)data);
        _template_fill(str_func, tmpl_eo_func_desc, classname, funcname, EINA_FALSE);
        _eobind_func_generate(str_bodyf, classname, (Function_Id)data, UNRESOLVED);
     }
   
   //Properties
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, PROPERTY_FUNC), l, data)
     {
        const char *funcname = database_function_name_get((Function_Id)data);
        const Function_Type ftype = database_function_type_get((Function_Id)data);
        char tmpstr[0xFF];
        
        Eina_Bool prop_read = ( ftype == SET ) ? EINA_FALSE : EINA_TRUE;
        Eina_Bool prop_write = ( ftype == GET ) ? EINA_FALSE : EINA_TRUE;
        
        if (prop_read)
          {
             const char *desc = database_function_description_get((Function_Id)data, "comment_get");
             sprintf(tmpstr, "%s_get", funcname);
             _template_fill(str_func, tmpl_eo_func_desc, classname, tmpstr, EINA_FALSE);
             _template_fill(tmpbuf, tmpl_eo_op_desc, classname, tmpstr, EINA_TRUE);
             eina_strbuf_replace_all(tmpbuf, "@#desc", desc);
             eina_strbuf_append(str_op, eina_strbuf_string_get(tmpbuf));
             _eobind_func_generate(str_bodyf, classname, (Function_Id)data, GET);
             _eapi_func_generate(str_bodyf, classname, (Function_Id)data, GET);
          }
        if (prop_write)
          {
             const char *desc = database_function_description_get((Function_Id)data, "comment_set");
             sprintf(tmpstr, "%s_set", funcname);
             _template_fill(str_func, tmpl_eo_func_desc, classname, tmpstr, EINA_FALSE);
             _template_fill(tmpbuf, tmpl_eo_op_desc, classname, tmpstr, EINA_TRUE);
             eina_strbuf_replace_all(tmpbuf, "@#desc", desc);
             eina_strbuf_append(str_op, eina_strbuf_string_get(tmpbuf));
             _eobind_func_generate(str_bodyf, classname, (Function_Id)data, SET);
             _eapi_func_generate(str_bodyf, classname, (Function_Id)data, SET);
          }
     }
     
   //Methods
   EINA_LIST_FOREACH(database_class_functions_list_get(classname, METHOD_FUNC), l, data)
     {
        const char *funcname = database_function_name_get((Function_Id)data);
        const char *desc = database_function_description_get((Function_Id)data, "comment");

        _template_fill(str_func, tmpl_eo_func_desc, classname, funcname, EINA_FALSE);
        _template_fill(tmpbuf, tmpl_eo_op_desc, classname, funcname, EINA_TRUE);
        eina_strbuf_replace_all(tmpbuf, "@#desc", desc);
        eina_strbuf_append(str_op, eina_strbuf_string_get(tmpbuf));
        _eobind_func_generate(str_bodyf, classname, (Function_Id)data, UNRESOLVED);
        _eapi_func_generate(str_bodyf, classname, (Function_Id)data, UNRESOLVED);
     }
     
   eina_strbuf_replace_all(str_src, "@#list_func", eina_strbuf_string_get(str_func));
   eina_strbuf_replace_all(str_src, "@#list_op", eina_strbuf_string_get(str_op));
   eina_strbuf_replace_all(str_src, "@#list_body", eina_strbuf_string_get(str_bodyf));
   
   eina_strbuf_free(tmpbuf);
   eina_strbuf_free(str_op);
   eina_strbuf_free(str_func);
   eina_strbuf_free(str_bodyf);
   
   return eina_strbuf_string_steal(str_src);
}

Eina_Bool
ch_parser_header_append(Eina_Strbuf *header, char *classname)
{
   if (!database_class_exists(classname)) 
     {
        printf ("Class \"%s\" not found in database\n", classname); 
        return EINA_FALSE;
     }
   
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
            
            _eo_fundef_generate(str_funcdef, (Function_Id)data, classname, UNRESOLVED);
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
   