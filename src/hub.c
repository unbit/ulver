#include <ulver.h>

/*
        This is the hub, it schedules coroutines
        using libuv. It is spawned when needed (on new coro creation or libuv call)
        and dies when no more coroutines and events are running
        Once the hub is spawned it is responsable for managing the whole
        ulver thread.
*/
static void coro_schedule(ulver_thread *ut, ulver_coro *coro) {
	// first of all check if the coro is already scheduled
	ulver_coro *head = ut->scheduled_coros_head;
	while(head) {
		if (head == coro) return;
		head = head->scheduled_next;
	}
	if (!ut->scheduled_coros_head) {
		ut->scheduled_coros_head = coro;
		ut->scheduled_coros_tail = coro;
		coro->scheduled_prev = NULL;
		coro->scheduled_next = NULL;
		return;
	}

	ulver_coro *tail = ut->scheduled_coros_tail;
	ut->scheduled_coros_tail = coro;
	coro->scheduled_prev = tail;
	coro->scheduled_next = NULL;
	tail->scheduled_next = coro;
}

static void coro_unschedule(ulver_thread *ut, ulver_coro *coro) {
	ulver_coro *next = coro->scheduled_next;
	ulver_coro *prev = coro->scheduled_prev;

	if (next) {
		next->scheduled_prev = coro->scheduled_prev;
	}

	if (prev) {
		prev->scheduled_next = coro->scheduled_next;
	}

	if (ut->scheduled_coros_head == coro) {
		ut->scheduled_coros_head = next;
	}

	if (ut->scheduled_coros_tail == coro) {
		ut->scheduled_coros_tail = prev;
	}

}


static ulver_coro *coro_new(ulver_env *env) {
	ulver_coro *coro = env->alloc(env, sizeof(ulver_coro));
	coro->env = env;
	getcontext(&coro->context);
        size_t len= 0 ;
        int off = 0;
        void *stack = __splitstack_makecontext(8192, coro->ss_contexts, &len);
        __splitstack_block_signals_context(coro->ss_contexts, &off, NULL);
        coro->context.uc_link = 0;
        coro->context.uc_stack.ss_sp = stack;
        coro->context.uc_stack.ss_size = len;
        coro->context.uc_stack.ss_flags = 0;
	return coro;
}
static void schedule_coros(uv_prepare_t* handle, int status) {
	ulver_thread *ut = (ulver_thread *) handle->data;
        ulver_coro *coros = ut->scheduled_coros_head;
        while(coros) {
                // switch to the coroutine
                // do not change the stack in the main coro
		ut->current_coro = coros;
		__splitstack_getcontext(ut->hub->ss_contexts);
                if (coros != ut->main_coro) {
                        __splitstack_setcontext(coros->ss_contexts);
                }
                swapcontext(&ut->hub->context, &coros->context);
                // back to hub
                coros = coros->scheduled_next;
        }
	// unschedule all
	coros = ut->scheduled_coros_head;
        while(coros) {
		ulver_coro *next = coros->scheduled_next;
		coro_unschedule(ut, coros);
		coros = next;
	}

	// check if no more coros are running
	coros = ut->coros;
	while(coros) {
		if (!coros->dead) goto alive;
		coros = coros->next;
	}
	uv_prepare_stop(handle);
alive:
	ut->current_coro = ut->hub;
}
static void hub_loop(ulver_env *env, ulver_thread *ut) {
        uv_prepare_t prepare;
        uv_prepare_init(ut->hub_loop, &prepare);
	prepare.data = ut;
        uv_prepare_start(&prepare, schedule_coros);
        //uv_run(ut->hub_loop, UV_RUN_NOWAIT);
        uv_run(ut->hub_loop, UV_RUN_DEFAULT);
        // here we can DESTROY the hub loop
	uv_loop_delete(ut->hub_loop);	
	env->free(env, ut->hub, sizeof(ulver_coro));
	ut->hub = NULL;
	setcontext(&ut->main_coro->context);
}
void ulver_hub(ulver_env *env) {
        ulver_thread *ut = ulver_current_thread(env);
        // is the hub already running ?
        if (ut->hub) return;
	ut->hub = coro_new(env);
        ut->hub_loop = uv_loop_new();
        makecontext(&ut->hub->context, (void (*)(void))hub_loop, 2, env, ut);

	coro_schedule(ut, ut->current_coro);

        // switch to the hub
        __splitstack_getcontext(ut->current_coro->ss_contexts);
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&ut->current_coro->context, &ut->hub->context);
}

static void coro_eval(ulver_env *env, ulver_form *argv) {
	ulver_eval(env, argv);
	ulver_thread *ut = ulver_current_thread(env);
	// once here, mark the coro as dead, and switch back to the hub
	ulver_coro *dead_coro = ut->current_coro;
	dead_coro->dead = 1;
	ut->current_coro = ut->hub;
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&dead_coro->context, &ut->hub->context);
}

ulver_object *ulver_fun_make_coro(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "make-coro requires an argument");
	ulver_thread *ut = ulver_current_thread(env);
	// ensure the hub is running ...
	// schedule the main coro
	ulver_hub(env);

	coro_schedule(ut, ut->current_coro);

	// generate the new coro
	ulver_coro *coro = coro_new(env);

	// add the new coro
	if (!ut->coros) {
		ut->coros = coro;
	}
	else {
		ulver_coro *old = ut->coros;
		ut->coros = coro;
		coro->next = old;
	}
	makecontext(&coro->context, (void (*)(void))coro_eval, 2, env, argv);
	ulver_coro *current_coro = ut->current_coro;
	ut->current_coro = coro;
	ulver_stack_push(env, ut);

	// schedule the new coro
	coro_schedule(ut, coro);

	// switch to hub
	ut->current_coro = ut->hub;
	__splitstack_getcontext(current_coro->ss_contexts);
	__splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&current_coro->context, &ut->hub->context);
	ut->current_coro = current_coro;
	// back from the hub
	return env->t;
}
