#include <Eina.h>
#include <Ecore_File.h>
#include "Eolian.h"

#define EO_SUFFIX ".eo"

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
   EINA_LIST_FREE(files, filename)
      free(filename);
   eolian_database_shutdown();
   eina_shutdown();
   return 0;
}
