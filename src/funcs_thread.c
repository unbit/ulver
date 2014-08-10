#include <ulver.h>

ulver_object *ulver_fun_all_threads(ulver_env *env, ulver_form *argv) {
        ulver_object *threads = ulver_object_new(env, ULVER_LIST);
        uv_rwlock_rdlock(&env->threads_lock);
        ulver_thread *ut = env->threads;
        while(ut) {
                ulver_object_push(env, threads, env->t);
                ut = ut->next;
        }
        uv_rwlock_rdunlock(&env->threads_lock);
        return threads;
}


struct ulver_thread_args {
        ulver_object *to;
        ulver_env *env;
        ulver_form *argv;
};

void run_new_thread(void *arg) {
        struct ulver_thread_args *uta = (struct ulver_thread_args *) arg;
        ulver_object *to = uta->to;
        ulver_env *env = uta->env;
        ulver_form *argv = uta->argv;
        env->free(env, uta, sizeof(struct ulver_thread_args));
        // this will create the memory structures
        // for the new thread
        ulver_thread *ut = ulver_current_thread(env);
        ut->t = (uv_thread_t) uv_thread_self();

	// mark the thread as used
	ut->refs = 1;

        // wake up the spinning creator thread
        to->thread = ut;

        to->ret = ulver_eval(env, argv);

        // Note: to get the return value we need to join the thread

        uv_rwlock_rdunlock(&env->gc_lock);
	// setting dead to 1 will allow the thread to be discarded
        ut->dead = 1;
	// setting ready to 1, will allow the waiting thread to get the
	// return value
	to->ready = 1;
}

ulver_object *ulver_fun_make_thread(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *thread = ulver_object_new(env, ULVER_THREAD);
        uv_thread_t t;
        struct ulver_thread_args *uta = env->alloc(env, sizeof(struct ulver_thread_args));
        uta->to = thread;
        uta->env = env;
        uta->argv = argv;
        if (uv_thread_create(&t, run_new_thread, uta)) {
                env->free(env, uta, sizeof(struct ulver_thread_args));
                return ulver_error_form(env, ULVER_ERR_THREAD, NULL, "unable to spawn thread");
        }

        // ok, we can now start polling for thread initialization
	// this is a blocking operation
	uv_rwlock_rdunlock(&env->gc_lock);
        for(;;) {
                if (thread->thread) break;
        }
	uv_rwlock_rdlock(&env->gc_lock);
        return thread;
}

ulver_object *ulver_fun_join_thread(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type != ULVER_THREAD) return ulver_error(env, ULVER_ERR_NOT_THREAD);
	// unlock the gc, as this will be a blocking operation
	uv_rwlock_rdunlock(&env->gc_lock);
	for(;;) {
		if (uo->ready) break;
	}
	uv_rwlock_rdlock(&env->gc_lock);
	// if the return value is NULL, propagate the error
	if (uo->ret == NULL) {
		ulver_thread *ut = ulver_current_thread(env);
		// first clear the current error (useless, very probably)
		ulver_error(env, ULVER_ERR_NOERROR);
		ut->err_code = uo->thread->err_code;	
		ut->error_form = uo->thread->error_form;	
		ut->error_fun = uo->thread->error_fun;	
		ut->error = uo->thread->error;	
		ut->error_len = uo->thread->error_len;	
	}
        return uo->ret;
}

ulver_thread *ulver_current_thread(ulver_env *env) {
        ulver_thread *ut = (ulver_thread *) uv_key_get(&env->thread);
        if (ut) return ut;
        ut = env->alloc(env, sizeof(ulver_thread));
        uv_key_set(&env->thread, (void *) ut);

        ut->env = env;
        ut->t = (uv_thread_t) uv_thread_self();

        ut->main_coro = env->alloc(env, sizeof(ulver_coro));
        ut->main_coro->thread = ut;
        ut->main_coro->context = ulver_coro_alloc_context(env);

        ut->current_coro = ut->main_coro;

        ulver_stack_push(env, ut, ut->current_coro);

        uv_rwlock_wrlock(&env->threads_lock);
        ulver_thread *head = env->threads;
        if (head) head->prev = ut;
        env->threads = ut;
        ut->next = head;
        uv_rwlock_wrunlock(&env->threads_lock);

        uv_rwlock_rdlock(&env->gc_lock);

        return ut;
}

