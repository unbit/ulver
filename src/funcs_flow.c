#include <ulver.h>

ulver_object *ulver_fun_if(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        ulver_object *uo = ulver_eval(env, argv);
        if (uo != env->nil) {
                return ulver_eval(env, argv->next);
        }
        if (argv->next->next) {
                return ulver_eval(env, argv->next->next);
        }
        else {
                return env->nil;
        }
}

ulver_object *ulver_fun_cond(ulver_env *env, ulver_form *argv) {
        while(argv) {
                ulver_object *uo = ulver_eval(env, argv->list);
                if (uo != env->nil) {
                        if (argv->list->next) {
                                return ulver_eval(env, argv->list->next);
                        }
                        else {
                                return uo;
                        }
                }
                argv = argv->next;
        }
        return env->nil;
}

ulver_object *ulver_fun_and(ulver_env *env, ulver_form *argv) {
	// fast check
	if (!argv) return env->t;
	ulver_object *current = ulver_eval(env, argv);
	if (!current) return NULL;
	if (current == env->nil) return env->nil;
	argv = argv->next;
	while(argv) {
                current = ulver_eval(env, argv->list);
		if (!current) return NULL;
		if (current == env->nil) return env->nil;
		argv = argv->next;
	}
	return current;
}

ulver_object *ulver_fun_or(ulver_env *env, ulver_form *argv) {
        // fast check
        if (!argv) return env->nil;
        ulver_object *current = ulver_eval(env, argv);
        if (!current) return NULL;
        if (current != env->nil) return current;
        argv = argv->next;
        while(argv) {
                current = ulver_eval(env, argv->list);
                if (!current) return NULL;
                if (current != env->nil) return current;
                argv = argv->next;
        }
        return env->nil;
}

ulver_object *ulver_fun_not(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo == env->nil) return env->t;
	return env->nil;
}
