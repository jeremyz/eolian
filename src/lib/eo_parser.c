#include <Ecore_File.h>
#include <stdio.h>
#include <string.h>

#include "Eolian.h"

typedef enum
{
   STOP = 0,
   SKIP_SPACES_TOKEN,
   KWORD_TOKEN,
   UWORD_TOKEN,
   KCHAR_TOKEN,
   UCHAR_TOKEN
} TokenEnum;

#define SPACES    SKIP_SPACES_TOKEN
#define KWORD(w)  KWORD_TOKEN, w
#define UWORD(w)  UWORD_TOKEN, w
#define KCHAR(c)  KCHAR_TOKEN, c
#define UCHAR(c)  UCHAR_TOKEN, c

#define LEX(buf,...) _lexer(buf, __VA_ARGS__, STOP)

static Eina_Bool
_lexer(char *buffer, ...)
{
   va_list list;
   va_start(list, buffer);
   TokenEnum token = va_arg(list, TokenEnum);
   while (token)
     {
        switch(token)
          {
           case STOP:
              return EINA_TRUE;
           case SKIP_SPACES_TOKEN:
                {
                   while (*buffer && (*buffer == ' ' || *buffer == '\n')) buffer++;
                   break;
                }
           case KWORD_TOKEN:
                {
                   char *word = va_arg(list, char *);
                   if (strncmp(buffer, word, strlen(word))) return EINA_FALSE;
                   buffer += strlen(word);
                   break;
                }
           case UWORD_TOKEN:
                {
                   char **pWord = va_arg(list, char **);
                   char *begin = buffer;
                   char c = *buffer;
                   while (((c & 0xDF) >= 'A' && (c & 0xDF) <= 'Z') ||
                         (c >= '0' && c <= '9') ||
                         (c == '_')) c = *(buffer++);
                   if (begin == buffer) return EINA_FALSE; // no word found
                   if (pWord) *pWord = strndup(begin, buffer - begin - 1);
                   break;
                }
           case KCHAR_TOKEN:
                {
                   char c = va_arg(list, int);
                   if (*buffer && *buffer == c) buffer++;
                   else return EINA_FALSE;
                   break;
                }
           case UCHAR_TOKEN:
                {
                   char *c = va_arg(list, char *);
                   if (*buffer)
                     {
                        if (c) *c = *buffer;
                        buffer++;
                     }
                   else return EINA_FALSE;
                   break;
                }
          }
        token = va_arg(list, TokenEnum);
     }
   return EINA_TRUE;
}

static unsigned int
_class_parse(char *buffer)
{
   char *class_name = NULL;
   Eina_Bool ret = LEX(buffer, UWORD(&class_name), SPACES, KCHAR('='));
   if (ret)
     {
        if (class_name) printf("Class %s\n", class_name);
//        buffer = class_name + len;
//        while (*buffer && *buffer != ' ') buffer++;
     }
   return 0; // FIXME: have to return a real value
}

Eina_Bool eolian_eo_file_parse(char *filename)
{
   if (!ecore_file_exists(filename)) return EINA_FALSE;

   unsigned int sz = ecore_file_size(filename);
   char *buffer = malloc(sz * sizeof(char) + 1); // \0 at the end

   FILE* fd = fopen(filename, "r");
   size_t read_sz = fread(buffer, 1, sz, fd);
   buffer[read_sz] = '\0';

   if (read_sz != sz)
     {
        printf("%s: Read size %d different from file size %d. Continue.\n", __FUNCTION__, read_sz, sz);
     }

   char *tmp = buffer;
   while (tmp)
     {
        _class_parse(tmp);
        tmp = NULL;
     }
   return EINA_TRUE;
}

