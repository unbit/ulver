#include <ulver.h>

ulver_object *ulver_fun_get_universal_time(ulver_env *env, ulver_form *argv) {
	return ulver_object_from_num(env, time(NULL));
}

ulver_object *ulver_fun_now(ulver_env *env, ulver_form *argv) {
	ulver_thread *ut = ulver_current_thread(env);
	// ensure the hub is running
	ulver_hub(env);
	return ulver_object_from_num(env, uv_now(ut->hub_loop) );
}
