#include <Eina.h>
#include "Eolian.h"

int main(int argc, char **argv)
{
   int i;
   Eina_Bool help = EINA_FALSE, show = EINA_FALSE;
   char *input_file = NULL; // FIXME: for the moment, only one
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
        input_file = argv[i];
     }

   if (!input_file || help)
     {
        printf("Usage: %s [-h/--help] [--show] input files\n", argv[0]);
        return 0;
     }

   if (!eolian_eo_file_parse(input_file))
     {
        printf("Error during parsing of file %s\n", input_file);
        return 1;
     }
   return 0;
}
