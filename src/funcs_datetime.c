#include <ulver.h>

ulver_object *ulver_fun_get_universal_time(ulver_env *env, ulver_form *argv) {
	return ulver_object_from_num(env, time(NULL));
}
