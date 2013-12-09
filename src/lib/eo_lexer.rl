#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Eina.h>

#include "eo_lexer.h"
#include "database.h"

static int _eo_tokenizer_log_dom = -1;
#ifdef CRITICAL
#undef CRITICAL
#endif
#define CRITICAL(...) EINA_LOG_DOM_CRIT(_eo_tokenizer_log_dom, __VA_ARGS__)

#ifdef ERR
#undef ERR
#endif
#define ERR(...) EINA_LOG_DOM_ERR(_eo_tokenizer_log_dom, __VA_ARGS__)

#ifdef WRN
#undef WRN
#endif
#define WRN(...) EINA_LOG_DOM_WARN(_eo_tokenizer_log_dom, __VA_ARGS__)

#ifdef INF
#undef INF
#endif
#define INF(...) EINA_LOG_DOM_INFO(_eo_tokenizer_log_dom, __VA_ARGS__)

#ifdef DBG
#undef DBG
#endif
#define DBG(...) EINA_LOG_DOM_DBG(_eo_tokenizer_log_dom, __VA_ARGS__)

static int _init_counter = 0;

int
eo_tokenizer_init()
{
   if (!_init_counter)
     {
        eina_init();
        eina_log_color_disable_set(EINA_FALSE);
        _eo_tokenizer_log_dom = eina_log_domain_register("eo_toknz", EINA_COLOR_CYAN);
     }
   return _init_counter++;
}

int
eo_tokenizer_shutdown()
{
   if (_init_counter <= 0) return 0;
   _init_counter--;
   if (!_init_counter)
     {
        eina_log_domain_unregister(_eo_tokenizer_log_dom);
        _eo_tokenizer_log_dom = -1;
        eina_shutdown();
     }
   return _init_counter;
}

static void
_eo_tokenizer_abort(Eo_Tokenizer *toknz,
                    const char *file, const char* fct, int line,
                    const char *fmt, ...)
{
   va_list ap;
   va_start (ap, fmt);
   eina_log_vprint(_eo_tokenizer_log_dom, EINA_LOG_LEVEL_ERR,
                   file, fct, line, fmt, ap);
   va_end(ap);
   fprintf(stderr, " toknz[%d] n:%d l:%d p:%d pe:%d ts:%s te:%s act:%d\n",
          toknz->cs, toknz->current_nesting, toknz->current_line,
          (int)(toknz->p - toknz->buf), (int)(toknz->pe - toknz->buf),
          toknz->ts, toknz->te, toknz->act);
   exit(EXIT_FAILURE);
}
#define ABORT(toknz, ...) \
   _eo_tokenizer_abort(toknz, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);

static void _eo_tokenizer_normalize_buf(char *buf)
{
   int c;
   char *s, *d;
   Eina_Bool in_space = EINA_TRUE;
   Eina_Bool in_newline = EINA_FALSE;

   /* ' '+ -> ' '
    * '\n' ' '* '*' ' '* -> '\n'
    */
   for (s = buf, d = buf; *s != '\0'; s++)
     {
        c = *s;
        *d = c;

        if (!in_space || (c != ' '))
          d++;

        if (c == ' ')
          in_space = EINA_TRUE;
        else
          in_space = EINA_FALSE;

        if (c == '\n')
          {
             in_newline = EINA_TRUE;
             in_space = EINA_TRUE;
          }
        else if (in_newline && c == '*' )
          {
             in_space = EINA_TRUE;
             in_newline = EINA_FALSE;
             d--;
          }
     }
   /* ' '+$ -> $ */
   d--;
   while (*d == ' ') d--;
   d++;
   if (d < buf) return;
   *d = '\0';
}

static const char*
_eo_tokenizer_token_get(Eo_Tokenizer *toknz, char *p)
{
   if (toknz->saved.tok == NULL) ABORT(toknz, "toknz->saved.tok is NULL");
   char d[BUFSIZE];
   int l = (p - toknz->saved.tok);
   memcpy(d, toknz->saved.tok, l);
   d[l] = '\0';
   _eo_tokenizer_normalize_buf(d);
   toknz->saved.tok = NULL;
   DBG("token : >%s<", d);
   return eina_stringshare_add(d);
}

static Eo_Class_Def*
_eo_tokenizer_class_get(Eo_Tokenizer *toknz, char *p)
{
   Eo_Class_Def *kls = calloc(1, sizeof(Eo_Class_Def));
   if (kls == NULL) ABORT(toknz, "calloc Eo_Class_Def failure");

   kls->name = _eo_tokenizer_token_get(toknz, p);

   return kls;
}

static Eo_Property_Def*
_eo_tokenizer_property_get(Eo_Tokenizer *toknz, char *p)
{
   Eo_Property_Def *prop = calloc(1, sizeof(Eo_Property_Def));
   if (prop == NULL) ABORT(toknz, "calloc Eo_Property_Def failure");

   prop->name = _eo_tokenizer_token_get(toknz, p);

   return prop;
}

static Eo_Method_Def*
_eo_tokenizer_method_get(Eo_Tokenizer *toknz, char *p)
{
   Eo_Method_Def *meth = calloc(1, sizeof(Eo_Method_Def));
   if (meth == NULL) ABORT(toknz, "calloc Eo_Method_Def failure");

   meth->name = _eo_tokenizer_token_get(toknz, p);

   return meth;
}

static Eo_Param_Def*
_eo_tokenizer_param_get(Eo_Tokenizer *toknz, char *p)
{
   char *s;

   Eo_Param_Def *param = calloc(1, sizeof(Eo_Param_Def));
   if (param == NULL) ABORT(toknz, "calloc Eo_Param_Def failure");

   for (s = p; s >= toknz->saved.tok; s--)
     {
        if ((*s == ' ') || (*s == '*'))
          break;
     }

   if (s == toknz->saved.tok)
     ABORT(toknz, "wrong parameter: %s", _eo_tokenizer_token_get(toknz, p));
   s++;

   param->way = PARAM_IN;
   if (strncmp(toknz->saved.tok, "in ", 3) == 0)
     {
        toknz->saved.tok += 3;
        param->way = PARAM_IN;
     }
   else if (strncmp(toknz->saved.tok, "out ", 4) == 0)
     {
        toknz->saved.tok += 4;
        param->way = PARAM_OUT;
     }
   else if (strncmp(toknz->saved.tok, "inout ", 6) == 0)
     {
        toknz->saved.tok += 6;
        param->way = PARAM_INOUT;
     }

   param->type = _eo_tokenizer_token_get(toknz, s);

   toknz->saved.tok = s;
   param->name = _eo_tokenizer_token_get(toknz, p);

   return param;
}

static Eo_Accessor_Def *
_eo_tokenizer_accessor_get(Eo_Tokenizer *toknz, Eo_Accessor_Type type)
{
   Eo_Accessor_Def *accessor = calloc(1, sizeof(Eo_Accessor_Def));
   if (accessor == NULL) ABORT(toknz, "calloc Eo_Accessor_Def failure");

   accessor->type = type;

   return accessor;
}

%%{
   machine common;

   access toknz->;
   variable p toknz->p;
   variable pe toknz->pe;
   variable eof toknz->eof;

   action inc_line {
      toknz->current_line += 1;
      DBG("inc[%d] %d", toknz->cs, toknz->current_line);
   }

   action save_line {
      toknz->saved.line = toknz->current_line;
      DBG("save line[%d] %d", toknz->cs, toknz->current_line);
   }

   action save_fpc {
      toknz->saved.tok = fpc;
      DBG("save token[%d] %p %c", toknz->cs, fpc, *fpc);
   }

   action show_comment {
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }

   action show_ignore {
      DBG("ignore[%d] line:%d", toknz->cs, toknz->current_line);
   }

   action show_error {
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = fpc, d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *fpc, buf);
      toknz->cs = eo_tokenizer_error;
      fbreak;  /* necessary to stop scanners */
   }

   cr                = '\n';
   cr_neg            = [^\n];
   ws                = [ \t\r];
   newline           = cr @inc_line;
   ignore            = (0x00..0x20 - cr) | newline;

   alnum_u           = alnum | '_';
   alpha_u           = alpha | '_';
   ident             = alpha+ >save_fpc (alnum | '_' )+;
   signal            = alpha+ >save_fpc (alnum | '_' | ',' )+;

   eo_comment        = "/*@" ignore* alnum_u >save_fpc ( any | cr @inc_line )* :>> "*/";
   c_comment         = "/*" ( any | cr @inc_line )* :>> "*/";
   cpp_comment       = "//" (any - cr )* newline;
   comment           = ( c_comment | cpp_comment ) > save_line;

   end_statement     = ';';
   begin_def         = '{';
   end_def           = '}' end_statement;
   begin_list        = '(';
   end_list          = ')';
   list_separator    = ',';

}%%

%%{
   machine eo_tokenizer;
   include common;

   write data;

###### TOKENIZE ACCESSOR

   action end_accessor_comment {
      if (toknz->tmp.accessor->comment != NULL)
        ABORT(toknz, "accessor has already a comment");
      toknz->tmp.accessor->comment = _eo_tokenizer_token_get(toknz, fpc-1);
      INF("        %s", toknz->tmp.accessor->comment);
   }

   action end_accessor_rettype {
      if (toknz->tmp.accessor->ret.type != NULL)
        ABORT(toknz, "accessor has already a return type");
      toknz->tmp.accessor->ret.type = _eo_tokenizer_token_get(toknz, fpc);
      INF("        %s", toknz->tmp.accessor->ret.type);
   }

   action end_accessor_rettype_comment {
      if (toknz->tmp.accessor->ret.comment != NULL)
        ABORT(toknz, "accessor return type has already a comment");
      toknz->tmp.accessor->ret.comment = _eo_tokenizer_token_get(toknz, fpc-2);
      INF("        %s", toknz->tmp.accessor->ret.comment);
   }

   action end_accessor_legacy {
      toknz->tmp.accessor->legacy = _eo_tokenizer_token_get(toknz, fpc);
   }

   action end_accessor {
      INF("      }");
      toknz->tmp.prop->accessors = eina_list_append(toknz->tmp.prop->accessors, toknz->tmp.accessor);
      toknz->tmp.accessor = NULL;
      toknz->current_nesting--;
      fgoto tokenize_property;
   }

   rettype_comment = ws* eo_comment %end_accessor_rettype_comment;
   rettype = 'return' ws+ alpha+ >save_fpc (alnum_u | '*' | ws )+  %end_accessor_rettype end_statement rettype_comment?;

   legacy = 'legacy' ws+ ident %end_accessor_legacy end_statement;

   tokenize_accessor := |*
      ignore+;    #=> show_ignore;
      eo_comment  => end_accessor_comment;
      comment     => show_comment;
      rettype;
      legacy;
      end_def     => end_accessor;
      any         => show_error;
      *|;

###### TOKENIZE PARAMS

   action end_param_comment {
      const char *c = _eo_tokenizer_token_get(toknz, fpc-2);
      if (toknz->tmp.param == NULL)
        ABORT(toknz, "no parameter set to associate this comment to: %s", c);
      toknz->tmp.param->comment = c;
      if (toknz->tmp.prop)
        toknz->tmp.prop->params = eina_list_append(toknz->tmp.prop->params, toknz->tmp.param);
      else if (toknz->tmp.meth)
        toknz->tmp.meth->params = eina_list_append(toknz->tmp.meth->params, toknz->tmp.param);
      else
        ABORT(toknz, "got a param but there is no property nor method waiting for it");
      toknz->tmp.param = NULL;
   }

   action end_param {
      toknz->tmp.param = _eo_tokenizer_param_get(toknz, fpc);
      INF("        %s : %s", toknz->tmp.param->name, toknz->tmp.param->type);
      if (toknz->tmp.prop)
        toknz->tmp.prop->params = eina_list_append(toknz->tmp.prop->params, toknz->tmp.param);
      else if (toknz->tmp.meth)
        toknz->tmp.meth->params = eina_list_append(toknz->tmp.meth->params, toknz->tmp.param);
      else
        ABORT(toknz, "got a pending param but there is no property nor method waiting for it");
      toknz->tmp.param = NULL;
   }

   action end_params {
      INF("      }");
      toknz->current_nesting--;
      if (toknz->tmp.prop)
        fgoto tokenize_property;
      else if (toknz->tmp.meth)
        fgoto tokenize_method;
      else
        ABORT(toknz, "leaving tokenize_params but there is no property nor method pending");
   }

   param_comment = ws* eo_comment %end_param_comment;
   param = alpha+ >save_fpc (alnum_u | '*' | ws )+  %end_param end_statement param_comment?;

   tokenize_params := |*
      ignore+;    #=> show_ignore;
      comment     => show_comment;
      param;
      end_def     => end_params;
      any         => show_error;
      *|;

###### TOKENIZE PROPERTY

   action begin_property_get {
      INF("      get {");
      toknz->tmp.accessor = _eo_tokenizer_accessor_get(toknz, GETTER);
      toknz->current_nesting++;
      fgoto tokenize_accessor;
   }

   action begin_property_set {
      INF("      set {");
      toknz->tmp.accessor = _eo_tokenizer_accessor_get(toknz, SETTER);
      toknz->current_nesting++;
      fgoto tokenize_accessor;
   }

   action begin_property_params {
      INF("      params {");
      toknz->current_nesting++;
      fgoto tokenize_params;
   }

   action end_property {
      if (eina_list_count(toknz->tmp.prop->params) == 0)
        WRN("property '%s' has no parameters.", toknz->tmp.prop->name);
      if (eina_list_count(toknz->tmp.prop->accessors) == 0)
        WRN("property '%s' has no accessors.", toknz->tmp.prop->name);
      INF("    }");
      toknz->tmp.kls->properties = eina_list_append(toknz->tmp.kls->properties, toknz->tmp.prop);
      toknz->tmp.prop = NULL;
      toknz->current_nesting--;
      fgoto tokenize_properties;
   }

   prop_get = 'get' ws* begin_def;
   prop_set = 'set' ws* begin_def;
   prop_params = 'params' ws* begin_def;

   tokenize_property := |*
      ignore+;    #=> show_ignore;
      comment     => show_comment;
      prop_get    => begin_property_get;
      prop_set    => begin_property_set;
      prop_params => begin_property_params;
      end_def     => end_property;
      any         => show_error;
      *|;

###### TOKENIZE PROPERTIES

   action begin_property {
      INF("    %s {", toknz->tmp.prop->name);
      toknz->current_nesting++;
      fgoto tokenize_property;
   }

   action end_property_name {
      if (toknz->tmp.prop != NULL)
        ABORT(toknz, "there is a pending property definition %s", toknz->tmp.prop->name);
      toknz->tmp.prop = _eo_tokenizer_property_get(toknz, fpc);
   }

   action end_properties {
      INF("  }");
      toknz->current_nesting--;
      fgoto tokenize_class;
   }

   begin_property = ident %end_property_name ignore* begin_def;

   tokenize_properties := |*
      ignore+;       #=> show_ignore;
      comment        => show_comment;
      begin_property => begin_property;
      end_def        => end_properties;
      any            => show_error;
      *|;

###### TOKENIZE METHOD

   action end_method_comment {
      if (toknz->tmp.meth->comment != NULL)
        ABORT(toknz, "method has already a comment");
      toknz->tmp.meth->comment = _eo_tokenizer_token_get(toknz, fpc-1);
      INF("        %s", toknz->tmp.meth->comment);
   }

   action begin_method_params {
      INF("      params {");
      toknz->current_nesting++;
      fgoto tokenize_params;
   }

   action end_method_rettype {
      if (toknz->tmp.meth->ret.type != NULL)
        ABORT(toknz, "method '%s' has already a return type", toknz->tmp.meth->name);
      toknz->tmp.meth->ret.type = _eo_tokenizer_token_get(toknz, fpc);
      INF("        %s", toknz->tmp.meth->ret.type);
   }

   action end_method_rettype_comment {
      if (toknz->tmp.meth->ret.comment != NULL)
        ABORT(toknz, "method '%s' return type has already a comment", toknz->tmp.meth->name);
      toknz->tmp.meth->ret.comment = _eo_tokenizer_token_get(toknz, fpc-2);
      INF("        %s", toknz->tmp.meth->ret.comment);
   }

   action end_method_legacy {
      toknz->tmp.meth->legacy = _eo_tokenizer_token_get(toknz, fpc);
   }

   action end_method {
      if (eina_list_count(toknz->tmp.meth->params) == 0)
        WRN("method '%s' has no parameters.", toknz->tmp.meth->name);
      INF("    }");
      if (toknz->do_constructors)
        toknz->tmp.kls->constructors = eina_list_append(toknz->tmp.kls->methods, toknz->tmp.meth);
      else
        toknz->tmp.kls->methods = eina_list_append(toknz->tmp.kls->methods, toknz->tmp.meth);
      toknz->tmp.meth = NULL;
      toknz->current_nesting--;
      fgoto tokenize_methods;
   }

   meth_params = 'params' ws* begin_def;
   meth_legacy = 'legacy' ws+ ident %end_method_legacy end_statement;
   meth_rettype_comment = ws* eo_comment %end_method_rettype_comment;
   meth_rettype = 'return' ws+ alpha+ >save_fpc (alnum_u | '*' | ws )+  %end_method_rettype end_statement meth_rettype_comment?;

   tokenize_method := |*
      ignore+;    #=> show_ignore;
      eo_comment  => end_method_comment;
      comment     => show_comment;
      meth_params => begin_method_params;
      meth_rettype;
      meth_legacy;
      end_def     => end_method;
      any         => show_error;
      *|;

###### TOKENIZE METHODS

   action begin_method {
      INF("    %s {", toknz->tmp.meth->name);
      toknz->current_nesting++;
      fgoto tokenize_method;
   }

   action end_method_name {
      if (toknz->tmp.meth != NULL)
        ABORT(toknz, "there is a pending method definition %s", toknz->tmp.meth->name);
      toknz->tmp.meth = _eo_tokenizer_method_get(toknz, fpc);
   }

   action end_methods {
      INF("  }");
      toknz->current_nesting--;
      fgoto tokenize_class;
   }

   begin_method = ident %end_method_name ignore* begin_def;

   tokenize_methods := |*
      ignore+;       #=> show_ignore;
      comment        => show_comment;
      begin_method   => begin_method;
      end_def        => end_methods;
      any            => show_error;
      *|;

###### TOKENIZE CLASS

   action end_class_comment {
      if (toknz->tmp.kls->comment != NULL)
        ABORT(toknz, "class %s has already a comment", toknz->tmp.kls->name);
      toknz->tmp.kls->comment = _eo_tokenizer_token_get(toknz, fpc-1);
   }

   action end_inherit_name {
      const char *base = _eo_tokenizer_token_get(toknz, fpc);
      toknz->tmp.kls->inherits = eina_list_append(toknz->tmp.kls->inherits, base);
   }

   action end_signal_name {
      const char *base = _eo_tokenizer_token_get(toknz, fpc);
      toknz->tmp.kls->signals = eina_list_append(toknz->tmp.kls->signals, base);
   }

   action begin_constructors {
      INF("  constructors {");
      toknz->do_constructors = EINA_TRUE;
      toknz->current_nesting++;
      fgoto tokenize_methods;
   }

   action begin_properties {
      INF("  properties {");
      toknz->current_nesting++;
      fgoto tokenize_properties;
   }

   action begin_methods {
      INF("  begin methods");
      toknz->do_constructors = EINA_FALSE;
      toknz->current_nesting++;
      fgoto tokenize_methods;
   }

   action end_class {
      INF("end class: %s", toknz->tmp.kls->name);
      toknz->classes = eina_list_append(toknz->classes, toknz->tmp.kls);
      toknz->tmp.kls = NULL;
      toknz->current_nesting--;
      fgoto main;
   }

   inherit_item = ident %end_inherit_name ignore*;
   inherit_item_next = list_separator ignore* inherit_item;
   inherits = 'inherits' ignore* begin_def ignore* (inherit_item inherit_item_next*)? end_def;

   signal_item = signal %end_signal_name ignore* end_statement ignore*;
   signals = 'signals' ignore* begin_def ignore* signal_item* end_def;

   constructors = 'constructors' ignore* begin_def;
   properties = 'properties' ignore* begin_def;
   methods = 'methods' ignore* begin_def;

   tokenize_class := |*
      ignore+;       #=> show_ignore;
      eo_comment     => end_class_comment;
      comment        => show_comment;
      inherits;
      signals;
      constructors   => begin_constructors;
      properties     => begin_properties;
      methods        => begin_methods;
      end_def        => end_class;
      any            => show_error;
      *|;

###### TOP LEVEL

   action begin_class {
      INF("begin class: %s", toknz->tmp.kls->name);
      toknz->current_nesting++;
      fgoto tokenize_class;
   }

   action end_class_name {
      if (toknz->tmp.kls != NULL)
        ABORT(toknz, "there is a pending class definition %s", toknz->tmp.kls->name);
      toknz->tmp.kls = _eo_tokenizer_class_get(toknz, fpc);
   }

   begin_class = ident %end_class_name ignore* begin_def;

   main := |*
      ignore+;    #=> show_ignore;
      comment     => show_comment;
      begin_class => begin_class;
      any         => show_error;
   *|;

}%%

Eina_Bool
eo_tokenizer_walk(Eo_Tokenizer *toknz, const char *source)
{
   INF("tokenize %s...", source);
   toknz->source = eina_stringshare_add(source);

   FILE *stream;

   int done = 0;
   int have = 0;
   int offset = 0;

   stream = fopen(toknz->source, "rb");
   if (!stream)
     {
        ERR("unable to read in %s", toknz->source);
        return EINA_FALSE;
     }

   %% write init;

   while (!done)
     {
        int len;
        int space;

        toknz->p = toknz->buf + have;
        space = BUFSIZE - have;

        if (space == 0)
          {
             fclose(stream);
             ABORT(toknz, "out of buffer space");
          }

        len = fread(toknz->p, 1, space, stream);
        if (len == 0) break;
        toknz->pe = toknz->p + len;

        if (len < space)
          {
             toknz->eof = toknz->pe;
             done = 1;
          }

        %% write exec;

        if ( toknz->cs == %%{ write error; }%% )
          {
             ERR("wrong termination");
             break;
          }

        if ( toknz->ts == 0 )
          have = 0;
        else
          {
             DBG("move data and pointers before buffer feed");
             have = toknz->pe - toknz->ts;
             offset = toknz->ts - toknz->buf;
             memmove(toknz->buf, toknz->ts, have);
             toknz->te -= offset;
             toknz->ts = toknz->buf;
          }

        if (toknz->saved.tok != NULL)
          {
             if ((have == 0) || ((toknz->saved.tok - offset) < toknz->buf))
               {
                  WRN("reset lost saved token %p", toknz->saved.tok);
                  toknz->saved.tok = NULL;
               }
             else
               toknz->saved.tok -= offset;
          }
     }

   fclose(stream);

   return EINA_TRUE;
}

Eo_Tokenizer*
eo_tokenizer_get(void)
{
   Eo_Tokenizer *toknz = calloc(1, sizeof(Eo_Tokenizer));
   if (!toknz) return NULL;

   toknz->ts = NULL;
   toknz->te = NULL;
   /* toknz->top = 0; */
   toknz->source = NULL;
   toknz->max_nesting = 10;
   toknz->current_line = 1;
   toknz->current_nesting = 0;
   toknz->saved.tok = NULL;
   toknz->saved.line = 0;
   toknz->classes = NULL;

   return toknz;
}

static char *_accessor_type_str[ACCESSOR_TYPE_LAST] = { "setter", "getter" };
static char *_param_way_str[PARAM_WAY_LAST] = { "IN", "OUT", "INOUT" };

void
eo_tokenizer_dump(Eo_Tokenizer *toknz)
{
   const char *s;
   Eina_List *k, *l, *m;

   Eo_Class_Def *kls;
   Eo_Property_Def *prop;
   Eo_Method_Def *meth;
   Eo_Param_Def *param;
   Eo_Accessor_Def *accessor;
   /* Eo_Ret_Def *ret; */

   EINA_LIST_FOREACH(toknz->classes, k, kls)
     {
        printf("Class: %s (%s)\n",
               kls->name, (kls->comment ? kls->comment : "-"));
        printf("  inherits from :");
        EINA_LIST_FOREACH(kls->inherits, l, s)
           printf(" %s", s);
        printf("\n");
        printf("  signals:\n");
        EINA_LIST_FOREACH(kls->signals, l, s)
           printf("    %s\n", s);

        EINA_LIST_FOREACH(kls->constructors, l, meth)
          {
             printf("  constructors: %s\n", meth->name);
             printf("    return: %s (%s)\n", meth->ret.type, meth->ret.comment);
             printf("    legacy : %s\n", meth->legacy);
             EINA_LIST_FOREACH(meth->params, m, param)
               {
                  printf("    param: %s %s : %s (%s)\n",
                         _param_way_str[param->way], param->name,
                         param->type, param->comment);
               }
          }

        EINA_LIST_FOREACH(kls->properties, l, prop)
          {
             printf("  property: %s\n", prop->name);
             EINA_LIST_FOREACH(prop->params, m, param)
               {
                  printf("    param: %s : %s (%s)\n",
                         param->name, param->type, param->comment);
               }
             EINA_LIST_FOREACH(prop->accessors, m, accessor)
               {
                  printf("    accessor: %s : %s (%s)\n",
                         accessor->ret.type, _accessor_type_str[accessor->type],
                         accessor->comment);
                  printf("      legacy : %s\n", accessor->legacy);
               }
          }

        EINA_LIST_FOREACH(kls->methods, l, meth)
          {
             printf("  method: %s\n", meth->name);
             printf("    return: %s (%s)\n", meth->ret.type, meth->ret.comment);
             printf("    legacy : %s\n", meth->legacy);
             EINA_LIST_FOREACH(meth->params, m, param)
               {
                  printf("    param: %s %s : %s (%s)\n",
                         _param_way_str[param->way], param->name,
                         param->type, param->comment);
               }
          }

     }

}

Eina_Bool
eo_tokenizer_database_fill(const char *filename)
{
   const char *s;
   Eina_List *k, *l, *m;

   Eo_Class_Def *kls;
   Eo_Property_Def *prop;
   Eo_Method_Def *meth;
   Eo_Param_Def *param;
   Eo_Accessor_Def *accessor;
   /* Eo_Ret_Def *ret; */

   Eo_Tokenizer *toknz = eo_tokenizer_get();
   if (!toknz)
     {
        ERR("can't create eo_tokenizer");
        return EINA_FALSE;
     }

   if (access(filename, F_OK) != 0)
     {
        ERR("error accessing file %s : %s", filename, strerror(errno));
        return EINA_FALSE;
     }
   eo_tokenizer_walk(toknz, filename);

   EINA_LIST_FOREACH(toknz->classes, k, kls)
     {
        database_class_add(kls->name);
        if (kls->comment) database_class_description_set(kls->name, kls->comment);

        EINA_LIST_FOREACH(kls->inherits, l, s)
           database_class_inherit_add(kls->name, s);

        EINA_LIST_FOREACH(kls->constructors, l, meth)
          {
             Function_Id foo_id = database_function_new(meth->name, CONSTRUCTOR);
             database_class_function_add(kls->name, foo_id);
             database_function_description_set(foo_id, RETURN_COMMENT, meth->ret.comment);
             database_function_data_set(foo_id, LEGACY, meth->legacy);
             EINA_LIST_FOREACH(meth->params, m, param)
               {
                  database_function_parameter_add(foo_id, (Parameter_Dir)param->way, param->type, param->name, param->comment);
               }
          }

        EINA_LIST_FOREACH(kls->properties, l, prop)
          {
             Function_Id foo_id = database_function_new(prop->name, UNRESOLVED);
             EINA_LIST_FOREACH(prop->params, m, param)
               {
                  /* IN_PARAM doesn't care */
                  database_function_parameter_add(foo_id, IN_PARAM, param->type, param->name, param->comment);
               }
             EINA_LIST_FOREACH(prop->accessors, m, accessor)
               {
                  database_function_type_set(foo_id, (accessor->type == SETTER?SET:GET));
               }
             database_class_function_add(kls->name, foo_id);
          }

        EINA_LIST_FOREACH(kls->methods, l, meth)
          {
             Function_Id foo_id = database_function_new(meth->name, METHOD_FUNC);
             database_class_function_add(kls->name, foo_id);
             database_function_description_set(foo_id, RETURN_COMMENT, meth->ret.comment);
             database_function_data_set(foo_id, LEGACY, meth->legacy);
             EINA_LIST_FOREACH(meth->params, m, param)
               {
                  database_function_parameter_add(foo_id, (Parameter_Dir)param->way, param->type, param->name, param->comment);
               }
          }

     }

   eo_tokenizer_free(toknz);
   return EINA_TRUE;
}

void
eo_tokenizer_free(Eo_Tokenizer *toknz)
{
   Eo_Class_Def *kls;

   if (toknz->source)
     eina_stringshare_del(toknz->source);

   EINA_LIST_FREE(toknz->classes, kls)
      eo_definitions_class_def_free(kls);

   free(toknz);
}

