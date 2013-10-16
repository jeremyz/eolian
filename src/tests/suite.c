/*
   gcc -o start some_func.c test.c `pkg-config --libs check`
   */
#include <check.h>
#include "Eolian.h"
#include "database.h"

#define EO_COMMENT "   MyClassName   = \n\
                       {\n         \
                         inherit\n   \
                         {\n\
                             MyBaseClass;\n\
                         }\n\
                       }"
START_TEST(first_test)
{
   eolian_database_init();
   eolian_eo_class_desc_parse(EO_COMMENT);
   fail_if(!database_class_exists("MyClassName"));
   eolian_database_shutdown();
}
END_TEST


int main(void)
{
   Suite *s = suite_create("First");

   TCase *tc = tcase_create("test case");

   tcase_add_test(tc, first_test);
   suite_add_tcase(s, tc);

   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   int number_failed = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (number_failed == 0) ? 0 : 255;
}
