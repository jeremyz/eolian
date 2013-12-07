#include <Eina.h>
#include <Ecore_File.h>
#include "Eolian.h"
#include "ch_parser.h"
#define EO_SUFFIX ".eo"

static Eina_Bool
_generate_h_file(char *filename, char *classname, Eina_Bool append)
{
   char *htext = NULL;

   if (!classname)
     {
        printf ("No class name specified to generate %s\n", filename);
        return EINA_FALSE;
     }

   if (append)
     {
        Eina_File *fn = eina_file_open(filename, EINA_FALSE);
        if (!fn)
          {
            printf ("Cant open file \"%s\" for updating.\n", filename);
            return EINA_FALSE;
          }

        Eina_Strbuf *hfile = eina_strbuf_new();
        eina_strbuf_append(hfile, (char*)eina_file_map_all(fn, EINA_FILE_SEQUENTIAL));
        eina_file_close(fn);

        ch_parser_header_append(hfile, classname);
        htext = eina_strbuf_string_steal(hfile);
     }
   else
     {
        htext = ch_parser_eo_header_generate(classname);
     }

   FILE* fd = fopen(filename, "w");

   if (!fd)
     {
        printf ("Couldn't open file %s for writing\n", filename);
        return EINA_FALSE;
     }

   if (htext) fputs(htext, fd);

   free(htext);
   fclose(fd);

   return EINA_TRUE;
}

static Eina_Bool
_generate_c_file(char *filename, char *classname)
{
   if (!classname)
     {
        printf ("No class name specified to generate %s\n", filename);
        return EINA_FALSE;
     }

   FILE* fd = fopen(filename, "w");
   if (!fd)
     {
        printf ("Couldnt open file %s for writing\n", filename);
        return EINA_FALSE;
     }

   char *ctext = ch_parser_eo_source_generate(classname);

   if (ctext) fputs(ctext, fd);
   free(ctext);

   fclose(fd);

   return EINA_TRUE;
}

int main(int argc, char **argv)
{
   eina_init();
   int i;
   Eina_Bool help = EINA_FALSE, show = EINA_FALSE;
   Eina_List *files = NULL, *itr;
   char *h_filename = NULL, *c_filename = NULL, *classname = NULL;
   Eina_Bool happend = EINA_FALSE;

   for(i = 1; i < argc; i++)
     {
        if (!strcmp(argv[i], "-gh") && (i < (argc-1)))
          {
             h_filename = argv[i + 1];
             continue;
          }
        if (!strcmp(argv[i], "-gc") && (i < (argc-1)))
          {
             c_filename = argv[i + 1];
             continue;
          }
        if (!strcmp(argv[i], "-ah") && (i < (argc-1)))
          {
             h_filename = argv[i + 1];
             happend = EINA_TRUE;
             continue;
          }
        if (!strcmp(argv[i], "-class") && (i < (argc-1)))
          {
             classname = argv[i + 1];
             continue;
          }
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
        printf("Usage: %s [-h/--help] [--show] input files [-gh|-gc|-ah] filename [-class] classname \n", argv[0]);
        printf("       -gh Generate c header file [.h] for eo class specified by classname\n");
        printf("       -gc Generate c source file [.c] for eo class specified by classname\n");
        printf("       -ah Append eo class definitions to an existing c header file [.h]\n");
        return 0;
     }

   eolian_init();
   const char *filename;
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

   if (h_filename)
    {
       printf("%s header file %s\n", (happend) ? "Appending" : "Generating", h_filename);
       _generate_h_file(h_filename, classname, happend);
    }

   if (c_filename)
    {
       printf("Generating source file %s\n", c_filename);
       _generate_c_file(c_filename, classname);
    }

   EINA_LIST_FREE(files, filename)
      free((char *)filename);
   eolian_shutdown();
   eina_shutdown();
   return 0;
}
