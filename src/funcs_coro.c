#include <ulver.h>

static void coro_eval(ulver_env *env, ulver_form *argv) {
        ulver_object *ret = ulver_eval(env, argv);
        ulver_thread *ut = ulver_current_thread(env);
        // once here, mark the coro as dead, and switch back to the hub
        ulver_coro *dead_coro = ut->current_coro;
        dead_coro->dead = 1;
        dead_coro->ret = ret;
        ulver_coro_switch(env, ut->hub);
        // never here !
}

ulver_object *ulver_fun_make_coro(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, "make-coro requires two arguments");
        // ensure the hub is running ...
        ulver_thread *ut = ulver_current_thread(env);

        ulver_hub(env);

        // generate the new coro
        ulver_coro *coro = ulver_coro_new(env, coro_eval, argv->next);

        // add the new coro
        if (!ut->coros) {
                ut->coros = coro;
        }
        else {
                ulver_coro *old = ut->coros;
                ut->coros = coro;
                coro->next = old;
                old->prev = coro;
        }

        // create the new stack
        ulver_coro *current_coro = ut->current_coro;
        ut->current_coro = coro;
        ulver_stack_push(env, ut);
        ut->current_coro = current_coro;

	// return a new coro object
        ulver_object *oc = ulver_object_new(env, ULVER_CORO);
        oc->coro = coro;
        oc->lambda_list = argv->list;
        return oc;
}

