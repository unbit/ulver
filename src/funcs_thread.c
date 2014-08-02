#include <ulver.h>

ulver_object *ulver_fun_all_threads(ulver_env *env, ulver_form *argv) {
        ulver_object *threads = ulver_object_new(env, ULVER_LIST);
        pthread_rwlock_rdlock(&env->threads_lock);
        ulver_thread *ut = env->threads;
        while(ut) {
                ulver_object_push(env, threads, env->t);
                ut = ut->next;
        }
        pthread_rwlock_unlock(&env->threads_lock);
        return threads;
}


struct ulver_thread_args {
        ulver_object *to;
        ulver_env *env;
        ulver_form *argv;
};

void *run_new_thread(void *arg) {
        struct ulver_thread_args *uta = (struct ulver_thread_args *) arg;
        ulver_object *to = uta->to;
        ulver_env *env = uta->env;
        ulver_form *argv = uta->argv;
        env->free(env, uta, sizeof(struct ulver_thread_args));
        // this will create the memory structures
        // for the new thread
        ulver_thread *ut = ulver_current_thread(env);
        ut->t = pthread_self();

        // wake up the spinning creator thread
        to->thread = ut;

        to->ret = ulver_eval(env, argv);

        // to get the return value we need to join the thread
        ulver_report_error(env);

        pthread_rwlock_unlock(&env->gc_lock);
	// setting dead to 1 will allow the thread to be discarded
        ut->dead = 1;
	// setting ready to 1, will allow the waiting thread to get the
	// return value
	to->ready = 1;
        return NULL;
}

ulver_object *ulver_fun_make_thread(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "make-thread requires an argument");
        ulver_object *thread = ulver_object_new(env, ULVER_THREAD);
        pthread_t t;
        struct ulver_thread_args *uta = env->alloc(env, sizeof(struct ulver_thread_args));
        uta->to = thread;
        uta->env = env;
        uta->argv = argv;
        if (pthread_create(&t, NULL, run_new_thread, uta)) {
                env->free(env, uta, sizeof(struct ulver_thread_args));
                return ulver_error(env, "unable to spawn thread");
        }

        // ok, we can now start polling for thread initialization
	// this is a blocking operation
	pthread_rwlock_unlock(&env->gc_lock);
        for(;;) {
                if (thread->ready) break;
        }
	pthread_rwlock_rdlock(&env->gc_lock);
        return thread;
}

ulver_object *ulver_fun_join_thread(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "join-thread requires an argument");
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type != ULVER_THREAD) return ulver_error_form(env, argv, "is not a thread");
	// unlock the gc, as this will be a blocking operation
	pthread_rwlock_unlock(&env->gc_lock);
	for(;;) {
		if (uo->ret) break;
	}
	pthread_rwlock_rdlock(&env->gc_lock);
        return uo->ret;
}

