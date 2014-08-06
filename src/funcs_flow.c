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

