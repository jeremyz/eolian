#include <Eina.h>
#include "lexer.h"

// FIXME: have to free the strduped buffers on failure
char *
Lexer_operate(char *buffer, unsigned int max_length, Parse_Direction dir, ...)
{
   va_list list;
   va_start(list, dir);
   TokenEnum token = va_arg(list, TokenEnum);
   while (token)
     {
        switch(token)
          {
           case STOP:
              return buffer;
           case SKIP_SPACES_TOKEN:
                {
                   while (*buffer && max_length && (*buffer == ' ' || *buffer == '\n'))
                     {
                        buffer += dir;
                        max_length--;
                     }
                   break;
                }
           case KWORD_TOKEN:
                {
                   // FIXME: can't work in RTl
                   char *word = va_arg(list, char *);
                   if (strncmp(buffer, word, strlen(word))) goto error;
                   buffer += (dir * strlen(word));
                   max_length -= strlen(word);
                   break;
                }
           case UWORD_TOKEN:
                {
                   char **pWord = va_arg(list, char **);
                   char *begin = buffer;
                   char c = *buffer;
                   while (max_length && (((c & 0xDF) >= 'A' && (c & 0xDF) <= 'Z') ||
                         (c >= '0' && c <= '9') || (c == '_')))
                     {
                        buffer += dir;
                        c = *buffer;
                        max_length--;
                     }
                   if (begin == buffer) goto error; // no word found
//                   buffer -= dir; // the buffer is one byte right because of the loop
                   if (pWord) *pWord = strndup((dir == LEFT_TO_RIGHT ? begin : buffer+1), dir * (buffer - begin));
                   break;
                }
           case KCHAR_TOKEN:
                {
                   char c = va_arg(list, int);
                   if (*buffer && *buffer == c) buffer += dir;
                   else goto error;
                   max_length--;
                   break;
                }
           case UCHAR_TOKEN:
                {
                   char *c = va_arg(list, char *);
                   if (*buffer)
                     {
                        if (c) *c = *buffer;
                        buffer += dir;
                        max_length--;
                     }
                   else goto error;
                   break;
                }
           case STRING_TOKEN:
                {
                   char *end = va_arg(list, char *);
                   char **ret = va_arg(list, char **);
                   char *new_buffer = strstr(buffer, end);
                   if (new_buffer)
                     {
                        /* Read about how malloc(0) works. */
                        if (new_buffer == buffer)
                          {
                             if (ret) *ret = NULL;
                          }
                        else
                          {
                             if (ret) *ret = strndup(buffer, dir * (new_buffer - buffer)); // FIXME: check for RTL
                          }
                        max_length -= (new_buffer - buffer);
                        buffer = new_buffer + strlen(end);
                     }
                   else goto error;
                   break;
                }
           case UWORDS_LIST_TOKEN:
                {
                   char *delim = va_arg(list, char *);
                   Eina_List **words_list = va_arg(list, Eina_List **);
                   char *word;
                   char *new_buffer = Lexer_operate(buffer, max_length, dir, UWORD(&word), STOP);
                   while (new_buffer)
                     {
                        max_length -= (new_buffer - buffer);
                        *words_list = eina_list_append(*words_list, word);
                        buffer = new_buffer;
                        new_buffer = Lexer_operate(buffer, max_length, dir, KWORD(delim), UWORD(&word), STOP);
                     }
                   break;
                }
           case STRINGS_LIST_TOKEN:
                {
                   char *delim = va_arg(list, char *);
                   Eina_List **strings_list = va_arg(list, Eina_List **);
                   char *new_buffer = buffer;
                   char *p_to_comment;
                   while (new_buffer)
                     {
                        new_buffer = strstr(buffer, delim);
                        p_to_comment = strstr(buffer, "/*");
                        if ((p_to_comment) && (p_to_comment < new_buffer))
                          {
                             p_to_comment = strstr(buffer, "*/");
                             new_buffer = strstr(p_to_comment, delim);
                          }
                        if (new_buffer)
                          {
                             max_length -= (new_buffer - buffer);
                             *strings_list = eina_list_append(*strings_list, strndup(buffer, new_buffer - buffer));
                             buffer = Lexer_operate(new_buffer, max_length, dir, KWORD(delim), SKIP_SPACES_TOKEN, STOP);
                          }
                        else
                          {
                             max_length = 0;
                             if (strlen(buffer))
                                *strings_list = eina_list_append(*strings_list, strdup(buffer));
                          }
                     }
                   break;
                }
           default:
                {
                   printf("Token %d not supported\n", token);
                   goto error;
                }
          }
        token = va_arg(list, TokenEnum);
     }
   return buffer; // FIXME: NULL or buffer
error:
   return NULL;
}


