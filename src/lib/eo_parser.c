#include <Ecore_File.h>
#include <stdio.h>

#include "Eolian.h"

// Word = ([a-z][A-Z][0-9]_)+
// We first drop the escapes
static char *_search_word(char *buffer, unsigned int *length)
{
   // Search for the first letter of the word
   while (*buffer && *buffer != ' ') buffer++;
   if (!*buffer) return NULL;
   char *tmp = buffer;
   char c = *tmp;
   while (((*tmp & 0xDF) > 'A' && (*tmp & 0xDF) <= 'Z') ||
         (*tmp > '0' && *tmp <= '9') ||
         (*tmp == '_')) tmp++;
   if (length) *length = (tmp-buffer);
   return buffer;
}

static unsigned int
_class_parse(char *buffer)
{
   int len = 0;
   char *class_name = _search_word(buffer, &len);
   if (class_name)
     {
        buffer = class_name + len;
        while (*buffer && *buffer != ' ') buffer++;
     }
}

Eina_Bool eolian_eo_file_parse(char *filename)
{
   if (!ecore_file_exists(filename)) return EINA_FALSE;

   long long sz = ecore_file_size(filename);
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
        if (_class_parse(tmp)
     }
   return EINA_TRUE;
}

