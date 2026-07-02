#include <stdlib.h>
#include <check.h>
#include "check_core.h"
#include "check_utils.h"
#include "check_json.h"
#include "check_html.h"
#include "check_http.h"
#include "check_modules.h"

int main(void)
{
	SRunner *srunner = srunner_create(NULL);

	srunner_add_suite(srunner, core_suite());
	srunner_add_suite(srunner, utils_suite());
	srunner_add_suite(srunner, json_suite());
	srunner_add_suite(srunner, html_suite());
	srunner_add_suite(srunner, http_suite());
	srunner_add_suite(srunner, modules_suite());

	srunner_run_all(srunner, CK_NORMAL);
	int num_failed = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return (num_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
