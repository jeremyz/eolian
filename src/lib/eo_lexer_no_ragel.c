
#line 1 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
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


#line 297 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"



#line 234 "eo_lexer.c"
static const char _eo_tokenizer_actions[] = {
	0, 1, 0, 1, 2, 1, 8, 1, 
	12, 1, 13, 1, 14, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	21, 1, 22, 1, 23, 1, 24, 1, 
	25, 1, 26, 1, 27, 1, 28, 1, 
	29, 1, 30, 1, 31, 1, 32, 1, 
	33, 1, 34, 1, 35, 1, 36, 1, 
	37, 1, 38, 1, 39, 1, 40, 1, 
	41, 1, 42, 1, 43, 1, 44, 1, 
	45, 1, 46, 1, 47, 1, 48, 1, 
	49, 1, 50, 1, 51, 1, 52, 1, 
	53, 1, 54, 1, 56, 1, 57, 1, 
	58, 1, 59, 1, 60, 1, 61, 1, 
	62, 1, 63, 1, 64, 1, 65, 1, 
	66, 1, 67, 1, 68, 1, 69, 1, 
	70, 1, 71, 1, 72, 1, 73, 1, 
	74, 1, 75, 1, 76, 1, 77, 1, 
	78, 1, 79, 1, 80, 1, 81, 1, 
	82, 1, 83, 1, 84, 1, 85, 2, 
	0, 19, 2, 0, 28, 2, 0, 36, 
	2, 0, 45, 2, 0, 53, 2, 0, 
	63, 2, 0, 71, 2, 0, 80, 2, 
	4, 24, 2, 5, 20, 2, 6, 32, 
	2, 8, 0, 2, 8, 46, 2, 10, 
	59, 2, 11, 55, 2, 12, 0, 2, 
	12, 64, 2, 13, 0, 2, 14, 0, 
	2, 14, 81, 2, 17, 1, 2, 17, 
	2, 2, 17, 3, 2, 17, 7, 2, 
	17, 9
};

static const short _eo_tokenizer_key_offsets[] = {
	0, 2, 5, 6, 17, 21, 24, 26, 
	29, 40, 42, 45, 46, 47, 48, 49, 
	50, 53, 60, 67, 75, 76, 77, 78, 
	79, 82, 89, 100, 112, 116, 117, 118, 
	128, 130, 133, 135, 138, 139, 151, 155, 
	156, 157, 167, 169, 172, 174, 177, 178, 
	179, 183, 184, 185, 186, 187, 191, 192, 
	196, 198, 201, 202, 213, 217, 220, 222, 
	225, 236, 238, 241, 242, 243, 244, 245, 
	246, 249, 256, 263, 271, 272, 273, 274, 
	275, 279, 280, 281, 282, 283, 286, 293, 
	304, 316, 320, 321, 322, 332, 334, 337, 
	339, 342, 343, 354, 358, 361, 363, 366, 
	377, 379, 382, 383, 384, 385, 386, 387, 
	388, 389, 393, 401, 408, 420, 425, 432, 
	433, 434, 435, 436, 437, 438, 442, 443, 
	444, 445, 446, 447, 448, 449, 450, 454, 
	462, 465, 467, 474, 481, 484, 486, 487, 
	488, 492, 492, 493, 502, 505, 507, 518, 
	522, 522, 523, 531, 534, 536, 537, 538, 
	539, 540, 549, 552, 554, 561, 562, 570, 
	573, 575, 576, 577, 578, 582, 582, 583, 
	592, 595, 597, 604, 605, 613, 616, 618, 
	619, 620, 621
};

static const char _eo_tokenizer_trans_keys[] = {
	10, 42, 10, 42, 47, 10, 10, 95, 
	123, 0, 32, 48, 57, 65, 90, 97, 
	122, 10, 123, 0, 32, 10, 42, 64, 
	10, 42, 10, 42, 47, 10, 42, 95, 
	0, 32, 48, 57, 65, 90, 97, 122, 
	10, 42, 10, 42, 47, 10, 103, 97, 
	99, 121, 9, 13, 32, 9, 13, 32, 
	65, 90, 97, 122, 95, 48, 57, 65, 
	90, 97, 122, 59, 95, 48, 57, 65, 
	90, 97, 122, 116, 117, 114, 110, 9, 
	13, 32, 9, 13, 32, 65, 90, 97, 
	122, 9, 13, 32, 42, 95, 48, 57, 
	65, 90, 97, 122, 9, 13, 32, 42, 
	59, 95, 48, 57, 65, 90, 97, 122, 
	9, 13, 32, 47, 42, 64, 10, 95, 
	0, 32, 48, 57, 65, 90, 97, 122, 
	10, 42, 10, 42, 47, 10, 42, 10, 
	42, 47, 10, 9, 13, 32, 42, 59, 
	95, 48, 57, 65, 90, 97, 122, 9, 
	13, 32, 47, 42, 64, 10, 95, 0, 
	32, 48, 57, 65, 90, 97, 122, 10, 
	42, 10, 42, 47, 10, 42, 10, 42, 
	47, 10, 116, 9, 13, 32, 123, 114, 
	97, 109, 115, 9, 13, 32, 123, 116, 
	9, 13, 32, 123, 10, 42, 10, 42, 
	47, 10, 10, 95, 123, 0, 32, 48, 
	57, 65, 90, 97, 122, 10, 123, 0, 
	32, 10, 42, 64, 10, 42, 10, 42, 
	47, 10, 42, 95, 0, 32, 48, 57, 
	65, 90, 97, 122, 10, 42, 10, 42, 
	47, 10, 103, 97, 99, 121, 9, 13, 
	32, 9, 13, 32, 65, 90, 97, 122, 
	95, 48, 57, 65, 90, 97, 122, 59, 
	95, 48, 57, 65, 90, 97, 122, 114, 
	97, 109, 115, 9, 13, 32, 123, 116, 
	117, 114, 110, 9, 13, 32, 9, 13, 
	32, 65, 90, 97, 122, 9, 13, 32, 
	42, 95, 48, 57, 65, 90, 97, 122, 
	9, 13, 32, 42, 59, 95, 48, 57, 
	65, 90, 97, 122, 9, 13, 32, 47, 
	42, 64, 10, 95, 0, 32, 48, 57, 
	65, 90, 97, 122, 10, 42, 10, 42, 
	47, 10, 42, 10, 42, 47, 10, 10, 
	95, 123, 0, 32, 48, 57, 65, 90, 
	97, 122, 10, 123, 0, 32, 10, 42, 
	64, 10, 42, 10, 42, 47, 10, 42, 
	95, 0, 32, 48, 57, 65, 90, 97, 
	122, 10, 42, 10, 42, 47, 10, 104, 
	101, 114, 105, 116, 115, 10, 123, 0, 
	32, 10, 125, 0, 32, 65, 90, 97, 
	122, 95, 48, 57, 65, 90, 97, 122, 
	10, 44, 95, 125, 0, 32, 48, 57, 
	65, 90, 97, 122, 10, 44, 125, 0, 
	32, 10, 0, 32, 65, 90, 97, 122, 
	59, 116, 104, 111, 100, 115, 10, 123, 
	0, 32, 111, 112, 101, 114, 116, 105, 
	101, 115, 10, 123, 0, 32, 10, 47, 
	0, 32, 65, 90, 97, 122, 10, 0, 
	32, 42, 47, 95, 48, 57, 65, 90, 
	97, 122, 10, 47, 108, 114, 125, 0, 
	32, 10, 0, 32, 42, 47, 101, 101, 
	9, 13, 32, 47, 59, 10, 47, 125, 
	0, 32, 65, 90, 97, 122, 10, 0, 
	32, 42, 47, 9, 13, 32, 42, 95, 
	48, 57, 65, 90, 97, 122, 9, 13, 
	32, 47, 59, 10, 47, 103, 112, 115, 
	125, 0, 32, 10, 0, 32, 42, 47, 
	101, 97, 101, 59, 10, 47, 125, 0, 
	32, 65, 90, 97, 122, 10, 0, 32, 
	42, 47, 95, 48, 57, 65, 90, 97, 
	122, 59, 10, 47, 108, 112, 114, 125, 
	0, 32, 10, 0, 32, 42, 47, 101, 
	97, 101, 9, 13, 32, 47, 59, 10, 
	47, 125, 0, 32, 65, 90, 97, 122, 
	10, 0, 32, 42, 47, 95, 48, 57, 
	65, 90, 97, 122, 59, 10, 47, 105, 
	109, 112, 125, 0, 32, 10, 0, 32, 
	42, 47, 110, 101, 114, 59, 0
};

static const char _eo_tokenizer_single_lengths[] = {
	2, 3, 1, 3, 2, 3, 2, 3, 
	3, 2, 3, 1, 1, 1, 1, 1, 
	3, 3, 1, 2, 1, 1, 1, 1, 
	3, 3, 5, 6, 4, 1, 1, 2, 
	2, 3, 2, 3, 1, 6, 4, 1, 
	1, 2, 2, 3, 2, 3, 1, 1, 
	4, 1, 1, 1, 1, 4, 1, 4, 
	2, 3, 1, 3, 2, 3, 2, 3, 
	3, 2, 3, 1, 1, 1, 1, 1, 
	3, 3, 1, 2, 1, 1, 1, 1, 
	4, 1, 1, 1, 1, 3, 3, 5, 
	6, 4, 1, 1, 2, 2, 3, 2, 
	3, 1, 3, 2, 3, 2, 3, 3, 
	2, 3, 1, 1, 1, 1, 1, 1, 
	1, 2, 2, 1, 4, 3, 1, 1, 
	1, 1, 1, 1, 1, 2, 1, 1, 
	1, 1, 1, 1, 1, 1, 2, 2, 
	1, 2, 1, 5, 1, 2, 1, 1, 
	4, 0, 1, 3, 1, 2, 5, 4, 
	0, 1, 6, 1, 2, 1, 1, 1, 
	1, 3, 1, 2, 1, 1, 6, 1, 
	2, 1, 1, 1, 4, 0, 1, 3, 
	1, 2, 1, 1, 6, 1, 2, 1, 
	1, 1, 1
};

static const char _eo_tokenizer_range_lengths[] = {
	0, 0, 0, 4, 1, 0, 0, 0, 
	4, 0, 0, 0, 0, 0, 0, 0, 
	0, 2, 3, 3, 0, 0, 0, 0, 
	0, 2, 3, 3, 0, 0, 0, 4, 
	0, 0, 0, 0, 0, 3, 0, 0, 
	0, 4, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 4, 1, 0, 0, 0, 
	4, 0, 0, 0, 0, 0, 0, 0, 
	0, 2, 3, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 2, 3, 
	3, 0, 0, 0, 4, 0, 0, 0, 
	0, 0, 4, 1, 0, 0, 0, 4, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 3, 3, 4, 1, 3, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 3, 
	1, 0, 3, 1, 1, 0, 0, 0, 
	0, 0, 0, 3, 1, 0, 3, 0, 
	0, 0, 1, 1, 0, 0, 0, 0, 
	0, 3, 1, 0, 3, 0, 1, 1, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	1, 0, 3, 0, 1, 1, 0, 0, 
	0, 0, 0
};

static const short _eo_tokenizer_index_offsets[] = {
	0, 3, 7, 9, 17, 21, 25, 28, 
	32, 40, 43, 47, 49, 51, 53, 55, 
	57, 61, 67, 72, 78, 80, 82, 84, 
	86, 90, 96, 105, 115, 120, 122, 124, 
	131, 134, 138, 141, 145, 147, 157, 162, 
	164, 166, 173, 176, 180, 183, 187, 189, 
	191, 196, 198, 200, 202, 204, 209, 211, 
	216, 219, 223, 225, 233, 237, 241, 244, 
	248, 256, 259, 263, 265, 267, 269, 271, 
	273, 277, 283, 288, 294, 296, 298, 300, 
	302, 307, 309, 311, 313, 315, 319, 325, 
	334, 344, 349, 351, 353, 360, 363, 367, 
	370, 374, 376, 384, 388, 392, 395, 399, 
	407, 410, 414, 416, 418, 420, 422, 424, 
	426, 428, 432, 438, 443, 452, 457, 462, 
	464, 466, 468, 470, 472, 474, 478, 480, 
	482, 484, 486, 488, 490, 492, 494, 498, 
	504, 507, 510, 515, 522, 525, 528, 530, 
	532, 537, 538, 540, 547, 550, 553, 562, 
	567, 568, 570, 578, 581, 584, 586, 588, 
	590, 592, 599, 602, 605, 610, 612, 620, 
	623, 626, 628, 630, 632, 637, 638, 640, 
	647, 650, 653, 658, 660, 668, 671, 674, 
	676, 678, 680
};

static const unsigned char _eo_tokenizer_trans_targs[] = {
	0, 1, 0, 0, 1, 135, 0, 135, 
	2, 4, 3, 135, 4, 3, 3, 3, 
	135, 4, 135, 4, 135, 6, 7, 8, 
	6, 6, 7, 6, 6, 7, 139, 6, 
	8, 7, 9, 8, 9, 9, 9, 6, 
	9, 10, 9, 9, 10, 139, 9, 139, 
	11, 13, 139, 14, 139, 15, 139, 16, 
	139, 17, 17, 17, 139, 17, 17, 17, 
	18, 18, 139, 19, 19, 19, 19, 139, 
	139, 19, 19, 19, 19, 139, 21, 139, 
	22, 139, 23, 139, 24, 139, 25, 25, 
	25, 139, 25, 25, 25, 26, 26, 139, 
	27, 27, 27, 27, 27, 27, 27, 27, 
	139, 27, 27, 27, 27, 144, 27, 27, 
	27, 27, 139, 28, 28, 28, 29, 139, 
	30, 139, 31, 139, 31, 32, 31, 32, 
	32, 32, 139, 32, 33, 32, 32, 33, 
	145, 32, 34, 35, 34, 34, 35, 147, 
	34, 147, 36, 37, 37, 37, 37, 151, 
	37, 37, 37, 37, 147, 38, 38, 38, 
	39, 147, 40, 147, 41, 147, 41, 42, 
	41, 42, 42, 42, 147, 42, 43, 42, 
	42, 43, 152, 42, 44, 45, 44, 44, 
	45, 154, 44, 154, 46, 48, 154, 48, 
	48, 48, 154, 154, 50, 154, 51, 154, 
	52, 154, 53, 154, 53, 53, 53, 154, 
	154, 55, 154, 55, 55, 55, 154, 154, 
	56, 57, 56, 56, 57, 161, 56, 161, 
	58, 60, 59, 161, 60, 59, 59, 59, 
	161, 60, 161, 60, 161, 62, 63, 64, 
	62, 62, 63, 62, 62, 63, 166, 62, 
	64, 63, 65, 64, 65, 65, 65, 62, 
	65, 66, 65, 65, 66, 166, 65, 166, 
	67, 69, 166, 70, 166, 71, 166, 72, 
	166, 73, 73, 73, 166, 73, 73, 73, 
	74, 74, 166, 75, 75, 75, 75, 166, 
	166, 75, 75, 75, 75, 166, 77, 166, 
	78, 166, 79, 166, 80, 166, 80, 80, 
	80, 166, 166, 82, 166, 83, 166, 84, 
	166, 85, 166, 86, 86, 86, 166, 86, 
	86, 86, 87, 87, 166, 88, 88, 88, 
	88, 88, 88, 88, 88, 166, 88, 88, 
	88, 88, 172, 88, 88, 88, 88, 166, 
	89, 89, 89, 90, 166, 91, 166, 92, 
	166, 92, 93, 92, 93, 93, 93, 166, 
	93, 94, 93, 93, 94, 173, 93, 95, 
	96, 95, 95, 96, 175, 95, 175, 97, 
	99, 98, 175, 99, 98, 98, 98, 175, 
	99, 175, 99, 175, 101, 102, 103, 101, 
	101, 102, 101, 101, 102, 180, 101, 103, 
	102, 104, 103, 104, 104, 104, 101, 104, 
	105, 104, 104, 105, 180, 104, 180, 106, 
	108, 180, 109, 180, 110, 180, 111, 180, 
	112, 180, 113, 180, 113, 114, 113, 180, 
	114, 119, 114, 115, 115, 180, 116, 116, 
	116, 116, 180, 117, 118, 116, 119, 117, 
	116, 116, 116, 180, 117, 118, 119, 117, 
	180, 118, 118, 115, 115, 180, 180, 180, 
	121, 180, 122, 180, 123, 180, 124, 180, 
	125, 180, 125, 180, 125, 180, 127, 180, 
	128, 180, 129, 180, 130, 180, 131, 180, 
	132, 180, 133, 180, 134, 180, 134, 180, 
	134, 180, 136, 137, 136, 138, 138, 135, 
	136, 136, 135, 0, 2, 135, 3, 3, 
	3, 3, 135, 140, 141, 142, 143, 146, 
	140, 139, 140, 140, 139, 5, 11, 139, 
	12, 139, 20, 139, 28, 28, 28, 29, 
	139, 139, 139, 139, 148, 149, 153, 148, 
	150, 150, 147, 148, 148, 147, 34, 36, 
	147, 37, 37, 37, 37, 37, 37, 37, 
	37, 147, 38, 38, 38, 39, 147, 147, 
	147, 147, 155, 156, 157, 158, 159, 160, 
	155, 154, 155, 155, 154, 44, 46, 154, 
	47, 154, 49, 154, 54, 154, 154, 154, 
	162, 163, 165, 162, 164, 164, 161, 162, 
	162, 161, 56, 58, 161, 59, 59, 59, 
	59, 161, 161, 161, 167, 168, 169, 170, 
	171, 174, 167, 166, 167, 167, 166, 61, 
	67, 166, 68, 166, 76, 166, 81, 166, 
	89, 89, 89, 90, 166, 166, 166, 166, 
	176, 177, 179, 176, 178, 178, 175, 176, 
	176, 175, 95, 97, 175, 98, 98, 98, 
	98, 175, 175, 175, 181, 182, 183, 184, 
	185, 186, 181, 180, 181, 181, 180, 100, 
	106, 180, 107, 180, 120, 180, 126, 180, 
	180, 180, 135, 135, 135, 135, 135, 139, 
	139, 139, 139, 139, 139, 139, 139, 139, 
	139, 139, 139, 139, 139, 139, 139, 139, 
	139, 139, 139, 139, 139, 139, 139, 139, 
	139, 139, 139, 139, 147, 147, 147, 147, 
	147, 147, 147, 147, 147, 147, 154, 154, 
	154, 154, 154, 154, 154, 154, 154, 154, 
	154, 154, 161, 161, 161, 161, 161, 166, 
	166, 166, 166, 166, 166, 166, 166, 166, 
	166, 166, 166, 166, 166, 166, 166, 166, 
	166, 166, 166, 166, 166, 166, 166, 166, 
	166, 166, 166, 166, 166, 166, 166, 166, 
	166, 175, 175, 175, 175, 175, 180, 180, 
	180, 180, 180, 180, 180, 180, 180, 180, 
	180, 180, 180, 180, 180, 180, 180, 180, 
	180, 180, 180, 180, 180, 180, 180, 180, 
	180, 180, 180, 180, 180, 180, 180, 180, 
	180, 135, 135, 135, 139, 139, 139, 139, 
	139, 139, 139, 147, 147, 147, 147, 147, 
	147, 154, 154, 154, 154, 154, 154, 161, 
	161, 161, 161, 166, 166, 166, 166, 166, 
	166, 166, 166, 175, 175, 175, 175, 180, 
	180, 180, 180, 180, 180, 0
};

static const unsigned char _eo_tokenizer_trans_actions[] = {
	1, 0, 0, 1, 0, 139, 0, 172, 
	0, 205, 0, 208, 11, 0, 0, 0, 
	149, 1, 141, 0, 149, 1, 0, 0, 
	0, 1, 0, 0, 1, 0, 21, 0, 
	1, 0, 3, 0, 3, 3, 3, 0, 
	1, 0, 0, 1, 0, 19, 0, 151, 
	0, 0, 35, 0, 35, 0, 35, 0, 
	35, 0, 0, 0, 35, 0, 0, 0, 
	3, 3, 35, 0, 0, 0, 0, 35, 
	178, 0, 0, 0, 0, 35, 0, 35, 
	0, 35, 0, 35, 0, 35, 0, 0, 
	0, 35, 0, 0, 0, 3, 3, 35, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	35, 0, 0, 0, 0, 217, 0, 0, 
	0, 0, 35, 0, 0, 0, 0, 33, 
	0, 33, 0, 33, 1, 3, 0, 3, 
	3, 3, 33, 1, 0, 0, 1, 0, 
	0, 0, 1, 0, 0, 1, 0, 37, 
	0, 154, 0, 0, 0, 0, 0, 220, 
	0, 0, 0, 0, 51, 0, 0, 0, 
	0, 49, 0, 49, 0, 49, 1, 3, 
	0, 3, 3, 3, 49, 1, 0, 0, 
	1, 0, 0, 0, 1, 0, 0, 1, 
	0, 53, 0, 157, 0, 0, 69, 0, 
	0, 0, 55, 69, 0, 69, 0, 69, 
	0, 69, 0, 69, 0, 0, 0, 59, 
	69, 0, 69, 0, 0, 0, 57, 69, 
	1, 0, 0, 1, 0, 71, 0, 160, 
	0, 184, 0, 187, 5, 0, 0, 0, 
	83, 1, 73, 0, 83, 1, 0, 0, 
	0, 1, 0, 0, 1, 0, 87, 0, 
	1, 0, 3, 0, 3, 3, 3, 0, 
	1, 0, 0, 1, 0, 85, 0, 163, 
	0, 0, 103, 0, 103, 0, 103, 0, 
	103, 0, 0, 0, 103, 0, 0, 0, 
	3, 3, 103, 0, 0, 0, 0, 103, 
	193, 0, 0, 0, 0, 103, 0, 103, 
	0, 103, 0, 103, 0, 103, 0, 0, 
	0, 89, 103, 0, 103, 0, 103, 0, 
	103, 0, 103, 0, 0, 0, 103, 0, 
	0, 0, 3, 3, 103, 0, 0, 0, 
	0, 0, 0, 0, 0, 103, 0, 0, 
	0, 0, 223, 0, 0, 0, 0, 103, 
	0, 0, 0, 0, 101, 0, 101, 0, 
	101, 1, 3, 0, 3, 3, 3, 101, 
	1, 0, 0, 1, 0, 0, 0, 1, 
	0, 0, 1, 0, 105, 0, 166, 0, 
	196, 0, 199, 7, 0, 0, 0, 117, 
	1, 107, 0, 117, 1, 0, 0, 0, 
	1, 0, 0, 1, 0, 121, 0, 1, 
	0, 3, 0, 3, 3, 3, 0, 1, 
	0, 0, 1, 0, 119, 0, 169, 0, 
	0, 137, 0, 137, 0, 137, 0, 137, 
	0, 137, 0, 137, 1, 0, 0, 137, 
	1, 0, 0, 3, 3, 137, 0, 0, 
	0, 0, 137, 202, 9, 0, 9, 9, 
	0, 0, 0, 137, 1, 0, 0, 0, 
	137, 1, 0, 3, 3, 137, 123, 137, 
	0, 137, 0, 137, 0, 137, 0, 137, 
	0, 137, 1, 127, 0, 137, 0, 137, 
	0, 137, 0, 137, 0, 137, 0, 137, 
	0, 137, 0, 137, 0, 137, 1, 125, 
	0, 137, 1, 211, 0, 214, 214, 143, 
	1, 0, 145, 0, 0, 147, 0, 0, 
	0, 0, 147, 1, 211, 17, 17, 0, 
	0, 25, 1, 0, 27, 0, 0, 31, 
	0, 31, 0, 31, 0, 0, 0, 0, 
	29, 175, 23, 31, 1, 211, 0, 0, 
	214, 214, 41, 1, 0, 43, 0, 0, 
	47, 0, 0, 0, 0, 0, 0, 0, 
	0, 47, 0, 0, 0, 0, 45, 181, 
	39, 47, 1, 211, 17, 17, 17, 0, 
	0, 63, 1, 0, 65, 0, 0, 67, 
	0, 67, 0, 67, 0, 67, 61, 67, 
	1, 211, 0, 0, 214, 214, 77, 1, 
	0, 79, 0, 0, 81, 0, 0, 0, 
	0, 81, 75, 81, 1, 211, 17, 17, 
	17, 0, 0, 93, 1, 0, 95, 0, 
	0, 99, 0, 99, 0, 99, 0, 99, 
	0, 0, 0, 0, 97, 190, 91, 99, 
	1, 211, 0, 0, 214, 214, 111, 1, 
	0, 113, 0, 0, 115, 0, 0, 0, 
	0, 115, 109, 115, 1, 211, 17, 17, 
	17, 0, 0, 131, 1, 0, 133, 0, 
	0, 135, 0, 135, 0, 135, 0, 135, 
	129, 135, 149, 149, 149, 149, 149, 35, 
	35, 35, 35, 35, 35, 35, 35, 35, 
	35, 35, 35, 35, 35, 35, 35, 35, 
	35, 35, 35, 35, 35, 35, 33, 33, 
	33, 33, 33, 33, 51, 51, 51, 51, 
	49, 49, 49, 49, 49, 49, 69, 69, 
	69, 69, 69, 69, 69, 69, 69, 69, 
	69, 69, 83, 83, 83, 83, 83, 103, 
	103, 103, 103, 103, 103, 103, 103, 103, 
	103, 103, 103, 103, 103, 103, 103, 103, 
	103, 103, 103, 103, 103, 103, 103, 103, 
	103, 103, 103, 101, 101, 101, 101, 101, 
	101, 117, 117, 117, 117, 117, 137, 137, 
	137, 137, 137, 137, 137, 137, 137, 137, 
	137, 137, 137, 137, 137, 137, 137, 137, 
	137, 137, 137, 137, 137, 137, 137, 137, 
	137, 137, 137, 137, 137, 137, 137, 137, 
	137, 145, 147, 147, 27, 31, 31, 31, 
	29, 175, 31, 43, 47, 47, 45, 181, 
	47, 65, 67, 67, 67, 67, 67, 79, 
	81, 81, 81, 95, 99, 99, 99, 99, 
	97, 190, 99, 113, 115, 115, 115, 133, 
	135, 135, 135, 135, 135, 0
};

static const unsigned char _eo_tokenizer_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 13, 
	0, 0, 0, 13, 0, 0, 0, 0, 
	0, 0, 0, 13, 0, 0, 0, 0, 
	0, 0, 13, 0, 0, 0, 0, 0, 
	0, 13, 0, 0, 0, 0, 13, 0, 
	0, 0, 0, 0, 0, 0, 0, 13, 
	0, 0, 0, 0, 13, 0, 0, 0, 
	0, 0, 0
};

static const unsigned char _eo_tokenizer_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 15, 
	0, 0, 0, 15, 0, 0, 0, 0, 
	0, 0, 0, 15, 0, 0, 0, 0, 
	0, 0, 15, 0, 0, 0, 0, 0, 
	0, 15, 0, 0, 0, 0, 15, 0, 
	0, 0, 0, 0, 0, 0, 0, 15, 
	0, 0, 0, 0, 15, 0, 0, 0, 
	0, 0, 0
};

static const short _eo_tokenizer_eof_trans[] = {
	687, 687, 687, 687, 687, 710, 710, 710, 
	710, 710, 710, 710, 710, 710, 710, 710, 
	710, 710, 710, 710, 710, 710, 710, 710, 
	710, 710, 710, 710, 716, 716, 716, 716, 
	716, 716, 720, 720, 720, 720, 726, 726, 
	726, 726, 726, 726, 738, 738, 738, 738, 
	738, 738, 738, 738, 738, 738, 738, 738, 
	743, 743, 743, 743, 743, 771, 771, 771, 
	771, 771, 771, 771, 771, 771, 771, 771, 
	771, 771, 771, 771, 771, 771, 771, 771, 
	771, 771, 771, 771, 771, 771, 771, 771, 
	771, 777, 777, 777, 777, 777, 777, 782, 
	782, 782, 782, 782, 817, 817, 817, 817, 
	817, 817, 817, 817, 817, 817, 817, 817, 
	817, 817, 817, 817, 817, 817, 817, 817, 
	817, 817, 817, 817, 817, 817, 817, 817, 
	817, 817, 817, 817, 817, 817, 817, 0, 
	818, 820, 820, 0, 821, 827, 827, 827, 
	825, 826, 827, 0, 828, 833, 833, 831, 
	832, 833, 0, 834, 839, 839, 839, 839, 
	839, 0, 840, 843, 843, 843, 0, 844, 
	851, 851, 851, 851, 849, 850, 851, 0, 
	852, 855, 855, 855, 0, 856, 861, 861, 
	861, 861, 861
};

static const int eo_tokenizer_start = 135;
static const int eo_tokenizer_first_final = 135;
static const int eo_tokenizer_error = -1;

static const int eo_tokenizer_en_tokenize_accessor = 139;
static const int eo_tokenizer_en_tokenize_params = 147;
static const int eo_tokenizer_en_tokenize_property = 154;
static const int eo_tokenizer_en_tokenize_properties = 161;
static const int eo_tokenizer_en_tokenize_method = 166;
static const int eo_tokenizer_en_tokenize_methods = 175;
static const int eo_tokenizer_en_tokenize_class = 180;
static const int eo_tokenizer_en_main = 135;


#line 649 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"


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

   
#line 796 "eo_lexer.c"
	{
	 toknz->cs = eo_tokenizer_start;
	 toknz->ts = 0;
	 toknz->te = 0;
	 toknz->act = 0;
	}

#line 671 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"

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

        
#line 831 "eo_lexer.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( toknz->p) == ( toknz->pe) )
		goto _test_eof;
_resume:
	_acts = _eo_tokenizer_actions + _eo_tokenizer_from_state_actions[ toknz->cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 16:
#line 1 "NONE"
	{ toknz->ts = ( toknz->p);}
	break;
#line 850 "eo_lexer.c"
		}
	}

	_keys = _eo_tokenizer_trans_keys + _eo_tokenizer_key_offsets[ toknz->cs];
	_trans = _eo_tokenizer_index_offsets[ toknz->cs];

	_klen = _eo_tokenizer_single_lengths[ toknz->cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( toknz->p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( toknz->p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _eo_tokenizer_range_lengths[ toknz->cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( toknz->p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( toknz->p)) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
_eof_trans:
	 toknz->cs = _eo_tokenizer_trans_targs[_trans];

	if ( _eo_tokenizer_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _eo_tokenizer_actions + _eo_tokenizer_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 234 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      toknz->current_line += 1;
      DBG("inc[%d] %d", toknz->cs, toknz->current_line);
   }
	break;
	case 1:
#line 239 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      toknz->saved.line = toknz->current_line;
      DBG("save line[%d] %d", toknz->cs, toknz->current_line);
   }
	break;
	case 2:
#line 244 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      toknz->saved.tok = ( toknz->p);
      DBG("save token[%d] %p %c", toknz->cs, ( toknz->p), *( toknz->p));
   }
	break;
	case 3:
#line 314 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.accessor->ret.type != NULL)
        ABORT(toknz, "accessor has already a return type");
      toknz->tmp.accessor->ret.type = _eo_tokenizer_token_get(toknz, ( toknz->p));
      INF("        %s", toknz->tmp.accessor->ret.type);
   }
	break;
	case 4:
#line 321 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.accessor->ret.comment != NULL)
        ABORT(toknz, "accessor return type has already a comment");
      toknz->tmp.accessor->ret.comment = _eo_tokenizer_token_get(toknz, ( toknz->p)-2);
      INF("        %s", toknz->tmp.accessor->ret.comment);
   }
	break;
	case 5:
#line 328 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      toknz->tmp.accessor->legacy = _eo_tokenizer_token_get(toknz, ( toknz->p));
   }
	break;
	case 6:
#line 357 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      const char *c = _eo_tokenizer_token_get(toknz, ( toknz->p)-2);
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
	break;
	case 7:
#line 371 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      toknz->tmp.param = _eo_tokenizer_param_get(toknz, ( toknz->p));
      INF("        %s : %s", toknz->tmp.param->name, toknz->tmp.param->type);
   }
	break;
	case 8:
#line 464 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.prop != NULL)
        ABORT(toknz, "there is a pending property definition %s", toknz->tmp.prop->name);
      toknz->tmp.prop = _eo_tokenizer_property_get(toknz, ( toknz->p));
   }
	break;
	case 9:
#line 501 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.meth->ret.type != NULL)
        ABORT(toknz, "method '%s' has already a return type", toknz->tmp.meth->name);
      toknz->tmp.meth->ret.type = _eo_tokenizer_token_get(toknz, ( toknz->p));
      INF("        %s", toknz->tmp.meth->ret.type);
   }
	break;
	case 10:
#line 508 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.meth->ret.comment != NULL)
        ABORT(toknz, "method '%s' return type has already a comment", toknz->tmp.meth->name);
      toknz->tmp.meth->ret.comment = _eo_tokenizer_token_get(toknz, ( toknz->p)-2);
      INF("        %s", toknz->tmp.meth->ret.comment);
   }
	break;
	case 11:
#line 515 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      toknz->tmp.meth->legacy = _eo_tokenizer_token_get(toknz, ( toknz->p));
   }
	break;
	case 12:
#line 553 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.meth != NULL)
        ABORT(toknz, "there is a pending method definition %s", toknz->tmp.meth->name);
      toknz->tmp.meth = _eo_tokenizer_method_get(toknz, ( toknz->p));
   }
	break;
	case 13:
#line 583 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      const char *base = _eo_tokenizer_token_get(toknz, ( toknz->p));
      toknz->tmp.kls->inherits = eina_list_append(toknz->tmp.kls->inherits, base);
   }
	break;
	case 14:
#line 634 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{
      if (toknz->tmp.kls != NULL)
        ABORT(toknz, "there is a pending class definition %s", toknz->tmp.kls->name);
      toknz->tmp.kls = _eo_tokenizer_class_get(toknz, ( toknz->p));
   }
	break;
	case 17:
#line 1 "NONE"
	{ toknz->te = ( toknz->p)+1;}
	break;
	case 18:
#line 307 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      if (toknz->tmp.accessor->comment != NULL)
        ABORT(toknz, "accessor has already a comment");
      toknz->tmp.accessor->comment = _eo_tokenizer_token_get(toknz, ( toknz->p)-1);
      INF("        %s", toknz->tmp.accessor->comment);
   }}
	break;
	case 19:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 20:
#line 350 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;}
	break;
	case 21:
#line 332 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("      }");
      toknz->tmp.prop->accessors = eina_list_append(toknz->tmp.prop->accessors, toknz->tmp.accessor);
      toknz->tmp.accessor = NULL;
      toknz->current_nesting--;
      { toknz->cs = 154; goto _again;}
   }}
	break;
	case 22:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 23:
#line 346 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 24:
#line 349 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 25:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 26:
#line 349 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}}
	break;
	case 27:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 28:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 29:
#line 376 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("      }");
      if (toknz->tmp.param != NULL)
        {
           if (toknz->tmp.prop)
             toknz->tmp.prop->params = eina_list_append(toknz->tmp.prop->params, toknz->tmp.param);
           else if (toknz->tmp.meth)
             toknz->tmp.meth->params = eina_list_append(toknz->tmp.meth->params, toknz->tmp.param);
           else
             ABORT(toknz, "got a pending param but there is no property nor method waiting for it");
        }
      toknz->tmp.param = NULL;
      toknz->current_nesting--;
      if (toknz->tmp.prop)
        { toknz->cs = 154; goto _again;}
      else if (toknz->tmp.meth)
        { toknz->cs = 166; goto _again;}
      else
        ABORT(toknz, "leaving tokenize_params but there is no property nor method pending");
   }}
	break;
	case 30:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 31:
#line 401 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 32:
#line 403 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 33:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 34:
#line 403 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}}
	break;
	case 35:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 36:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 37:
#line 410 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("      get {");
      toknz->tmp.accessor = _eo_tokenizer_accessor_get(toknz, GETTER);
      toknz->current_nesting++;
      { toknz->cs = 139; goto _again;}
   }}
	break;
	case 38:
#line 417 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("      set {");
      toknz->tmp.accessor = _eo_tokenizer_accessor_get(toknz, SETTER);
      toknz->current_nesting++;
      { toknz->cs = 139; goto _again;}
   }}
	break;
	case 39:
#line 424 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("      params {");
      toknz->current_nesting++;
      { toknz->cs = 147; goto _again;}
   }}
	break;
	case 40:
#line 430 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      if (eina_list_count(toknz->tmp.prop->params) == 0)
        WRN("property '%s' has no parameters.", toknz->tmp.prop->name);
      if (eina_list_count(toknz->tmp.prop->accessors) == 0)
        WRN("property '%s' has no accessors.", toknz->tmp.prop->name);
      INF("    }");
      toknz->tmp.kls->properties = eina_list_append(toknz->tmp.kls->properties, toknz->tmp.prop);
      toknz->tmp.prop = NULL;
      toknz->current_nesting--;
      { toknz->cs = 161; goto _again;}
   }}
	break;
	case 41:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 42:
#line 447 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 43:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 44:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 45:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 46:
#line 458 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("    %s {", toknz->tmp.prop->name);
      toknz->current_nesting++;
      { toknz->cs = 154; goto _again;}
   }}
	break;
	case 47:
#line 470 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("  }");
      toknz->current_nesting--;
      { toknz->cs = 180; goto _again;}
   }}
	break;
	case 48:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 49:
#line 479 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 50:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 51:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 52:
#line 488 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      if (toknz->tmp.meth->comment != NULL)
        ABORT(toknz, "method has already a comment");
      toknz->tmp.meth->comment = _eo_tokenizer_token_get(toknz, ( toknz->p)-1);
      INF("        %s", toknz->tmp.meth->comment);
   }}
	break;
	case 53:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 54:
#line 495 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("      params {");
      toknz->current_nesting++;
      { toknz->cs = 147; goto _again;}
   }}
	break;
	case 55:
#line 540 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;}
	break;
	case 56:
#line 519 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      if (eina_list_count(toknz->tmp.meth->params) == 0)
        WRN("method '%s' has no parameters.", toknz->tmp.meth->name);
      INF("    }");
      toknz->tmp.kls->methods = eina_list_append(toknz->tmp.kls->methods, toknz->tmp.meth);
      toknz->tmp.meth = NULL;
      toknz->current_nesting--;
      { toknz->cs = 175; goto _again;}
   }}
	break;
	case 57:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 58:
#line 535 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 59:
#line 539 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 60:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 61:
#line 539 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}}
	break;
	case 62:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 63:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 64:
#line 547 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("    %s {", toknz->tmp.meth->name);
      toknz->current_nesting++;
      { toknz->cs = 166; goto _again;}
   }}
	break;
	case 65:
#line 559 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("  }");
      toknz->current_nesting--;
      { toknz->cs = 180; goto _again;}
   }}
	break;
	case 66:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 67:
#line 568 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 68:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 69:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 70:
#line 577 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      if (toknz->tmp.kls->comment != NULL)
        ABORT(toknz, "class %s has already a comment", toknz->tmp.kls->name);
      toknz->tmp.kls->comment = _eo_tokenizer_token_get(toknz, ( toknz->p)-1);
   }}
	break;
	case 71:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 72:
#line 619 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;}
	break;
	case 73:
#line 588 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("  properties {");
      toknz->current_nesting++;
      { toknz->cs = 161; goto _again;}
   }}
	break;
	case 74:
#line 594 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("  begin methods");
      toknz->current_nesting++;
      { toknz->cs = 175; goto _again;}
   }}
	break;
	case 75:
#line 600 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("end class: %s", toknz->tmp.kls->name);
      toknz->classes = eina_list_append(toknz->classes, toknz->tmp.kls);
      toknz->tmp.kls = NULL;
      toknz->current_nesting--;
      { toknz->cs = 135; goto _again;}
   }}
	break;
	case 76:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 77:
#line 616 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 78:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 79:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 80:
#line 249 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("comment[%d] line%03d:%03d", toknz->cs,
          toknz->saved.line, toknz->current_line);
   }}
	break;
	case 81:
#line 628 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      INF("begin class: %s", toknz->tmp.kls->name);
      toknz->current_nesting++;
      { toknz->cs = 180; goto _again;}
   }}
	break;
	case 82:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p)+1;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 83:
#line 643 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;}
	break;
	case 84:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{ toknz->te = ( toknz->p);( toknz->p)--;{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
	case 85:
#line 258 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
	{{( toknz->p) = (( toknz->te))-1;}{
      DBG("error[%d]", toknz->cs);
      char *s, *d;
      char buf[BUFSIZE];
      for (s = ( toknz->p), d = buf; (s <= toknz->pe); s++)
        {
           if ((*s == '\r') || (*s == '\n'))
             break;
           *d++ = *s;
        }
      *d = '\0';
      ERR("error n:%d l:%d c:'%c': %s",
          toknz->current_nesting, toknz->current_line, *( toknz->p), buf);
      toknz->cs = eo_tokenizer_error;
      {( toknz->p)++; goto _out; }  /* necessary to stop scanners */
   }}
	break;
#line 1807 "eo_lexer.c"
		}
	}

_again:
	_acts = _eo_tokenizer_actions + _eo_tokenizer_to_state_actions[ toknz->cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 15:
#line 1 "NONE"
	{ toknz->ts = 0;}
	break;
#line 1820 "eo_lexer.c"
		}
	}

	if ( ++( toknz->p) != ( toknz->pe) )
		goto _resume;
	_test_eof: {}
	if ( ( toknz->p) == ( toknz->eof) )
	{
	if ( _eo_tokenizer_eof_trans[ toknz->cs] > 0 ) {
		_trans = _eo_tokenizer_eof_trans[ toknz->cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

#line 697 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"

        if ( toknz->cs == 
#line 1841 "eo_lexer.c"
-1
#line 698 "/home/daniel/e17/eolian/src/lib/eo_lexer.rl"
 )
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

