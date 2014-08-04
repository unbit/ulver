#include <ulver.h>

static void coro_eval(ulver_coro *coro) {
	ulver_env *env = coro->env;
        ulver_object *ret = ulver_eval(env, coro->argv);
        ulver_thread *ut = ulver_current_thread(env);
        coro->dead = 1;
        coro->ret = ret;
        ulver_coro_switch(env, ut->hub);
        // never here !
}

ulver_object *ulver_fun_make_coro(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        // ensure the hub is running ...
        ulver_thread *ut = ulver_current_thread(env);

        ulver_hub(env);

        // generate the new coro
        ulver_coro *coro = ulver_coro_new(env, coro_eval, NULL);
	coro->argv = argv->next;

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
        ulver_stack_push(env, ut, coro);

	// start pushing lambda_list
	ulver_form *lambda_list = argv->list;
	while(lambda_list) {
		if (lambda_list->type != ULVER_SYMBOL) exit(1);
		ulver_object *uo = ulver_symbol_get(env, lambda_list->value, lambda_list->len);	
		if (!uo) exit(1);
		ulver_symbolmap_set(env, coro->stack->locals, lambda_list->value, lambda_list->len, uo, 0);
		lambda_list = lambda_list->next;
	}

	// return a new coro object
        ulver_object *oc = ulver_object_new(env, ULVER_CORO);
        oc->coro = coro;
	// store the lambda list for future usages
        oc->lambda_list = argv->list;
        return oc;
}


ulver_object *ulver_fun_coro_switch(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *coro = ulver_eval(env, argv);
        if (!coro) return NULL;
        if (coro->type != ULVER_CORO) return ulver_error(env, ULVER_ERR_NOT_CORO);
	// check for cross-thread violation
	ulver_thread *ut = ulver_current_thread(env);
	if (ut != coro->coro->thread) {
                return ulver_error(env, ULVER_ERR_CROSS_THREAD);
	}
        // if the coro is dead, raise an error
        if (coro->coro->dead) {
                return ulver_error(env, ULVER_ERR_CORO_DEAD);
        }
        // if the coro is blocked, raise an error
        if (coro->coro->blocked) {
                return ulver_error(env, ULVER_ERR_CORO_BLOCKED);
        }
        // otherwise switch to it and get back the value
        ulver_hub_schedule_coro(env, coro->coro);
        return env->nil;
}

ulver_object *ulver_fun_coro_yield(ulver_env *env, ulver_form *argv) {
        ulver_object *ret = env->nil;
        if (argv) {
                ret = ulver_eval(env, argv);
                if (!ret) return NULL;
        }
        ulver_coro_yield(env, ret);
        return ulver_object_new(env, ULVER_CORO_DEAD);
}

ulver_object *ulver_fun_coro_next(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *coro = ulver_eval(env, argv);
        if (!coro) return NULL;
        if (coro->type != ULVER_CORO) return ulver_error(env, ULVER_ERR_NOT_CORO);
	// check for cross-thread violation
	ulver_thread *ut = ulver_current_thread(env);
	if (ut != coro->coro->thread) {
                return ulver_error(env, ULVER_ERR_CROSS_THREAD);
	}
        // if the coro is dead, raise an error
        if (coro->coro->dead) {
                return ulver_error(env, ULVER_ERR_CORO_DEAD);
        }
        // if the coro is blocked, raise an error
        if (coro->coro->blocked) {
                return ulver_error(env, ULVER_ERR_CORO_BLOCKED);
        }
        // otherwise switch to it and get back the value
        ulver_hub_schedule_coro(env, coro->coro);
        if (coro->coro->ret && coro->coro->ret->type == ULVER_CORO_DEAD) {
                return ulver_error(env, ULVER_ERR_CORO_DEAD);
        }
        // the coro coould be now blocked, so let's wait for it
        if (coro->coro->blocked) {
                ulver_hub_wait(env, coro->coro);
        }
        return coro->coro->ret;
}

