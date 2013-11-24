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
                             MyBaseClass1  ,\n\
                             MyBaseClass2  ;\n\
                         }\n\
                       }"

#define EO_COMMENT_JSON "{                               \n\
                          \"name\" : \"MyClassName\",    \n\
                          \"inherits\" : [\"MyBaseClass1\", \"MyBaseClass2\", \"\"],\n\
                          \"methods\" : {\n\
                            \"func_name\" : {\n\
                             \"comment\" : \"Comment\",\n\
                              \"parameters\" : [] \n\
                          }\n\
                          }\n\
                         }"

#define EO_COMMENT_JSON2 "{                               \n\
                          \"name\" : \"MyClassName\",    \n\
                          \"inherits\" : [\"MyBaseClass1\", \"MyBaseClass2\", \"\"]\n\
                         }"


#define JSON

START_TEST(class_name_test)
{
   Eina_Bool ret;
   eolian_database_init();
#ifndef JSON
   eolian_eo_class_desc_parse(EO_COMMENT);
#else
   ret = eolian_eo_class_desc_parse_json(EO_COMMENT_JSON);
#endif
   fail_if(!ret);
   fail_if(!database_class_exists("MyClassName"));
   fail_if(!database_class_function_exists("MyClassName", "func_name", METHOD_FUNC));

   ret = eolian_eo_class_desc_parse_json(EO_COMMENT_JSON2);
   fail_if(ret);

   eolian_database_shutdown();
}
END_TEST

START_TEST(inherits_test)
{
   Eina_List *compare = NULL, *itr1, *itr2;
   Eina_List *inherits_list = NULL;
   char *str1, *str2;
   eina_init();
   compare = eina_list_append(compare, "MyBaseClass1");
   compare = eina_list_append(compare, "MyBaseClass2");

   eolian_database_init();
#ifndef JSON
   eolian_eo_class_desc_parse(EO_COMMENT);
#else
   eolian_eo_class_desc_parse_json(EO_COMMENT_JSON);
#endif
   inherits_list = (Eina_List*) database_class_inherits_list_get("MyClassName");
   fail_if(!inherits_list);
   ck_assert_int_eq(eina_list_count(inherits_list), 2);
   itr2 = compare;
   EINA_LIST_FOREACH(inherits_list, itr1, str1)
     {
        str2 = eina_list_data_get(itr2);
        ck_assert_str_eq(str1, str2);

        itr2 = eina_list_next(itr2);
     }

   eina_list_free(compare);
   eolian_database_shutdown();
   eina_shutdown();
}
END_TEST

int main(void)
{
   Suite *s = suite_create("First");

   TCase *tc = tcase_create("test case");

   tcase_add_test(tc, class_name_test);
   tcase_add_test(tc, inherits_test);
   suite_add_tcase(s, tc);

   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   int number_failed = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (number_failed == 0) ? 0 : 255;
}
