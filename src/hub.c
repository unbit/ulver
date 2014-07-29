#include <ulver.h>

/*
        This is the hub, it schedules coroutines
        using libuv. It is spawned when needed (on new coro creation or libuv call)
        and dies when no more coroutines and events are running
        Once the hub is spawned it is responsable for managing the whole
        ulver thread.
*/
static void coro_schedule(ulver_env *env, ulver_thread *ut, ulver_coro *coro) {
	ulver_scheduled_coro *sc = env->alloc(env, sizeof(ulver_scheduled_coro));
	sc->coro = coro;
	if (!ut->scheduled_coros_head) {
		ut->scheduled_coros_head = sc;
		ut->scheduled_coros_tail = sc;
		return;
	}

	ulver_scheduled_coro *tail = ut->scheduled_coros_tail;
	ut->scheduled_coros_tail = sc;
	sc->prev = tail;
	tail->next = sc;
}

static void coro_unschedule(ulver_env *env, ulver_thread *ut, ulver_scheduled_coro *sc) {
	ulver_scheduled_coro *next = sc->next;
	ulver_scheduled_coro *prev = sc->prev;

	if (next) {
		next->prev = sc->prev;
	}

	if (prev) {
		prev->next = sc->next;
	}

	if (ut->scheduled_coros_head == sc) {
		ut->scheduled_coros_head = next;
	}

	if (ut->scheduled_coros_tail == sc) {
		ut->scheduled_coros_tail = prev;
	}

	env->free(env, sc, sizeof(ulver_scheduled_coro));
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
	ulver_env *env = ut->env;
        ulver_scheduled_coro *scheduled_coros = ut->scheduled_coros_head;
        while(scheduled_coros) {
                // switch to the coroutine
                // do not change the stack in the main coro
		if (scheduled_coros->coro->dead) goto next;
		ut->current_coro = scheduled_coros->coro;
		__splitstack_getcontext(ut->hub->ss_contexts);
                if (ut->current_coro != ut->main_coro) {
                        __splitstack_setcontext(ut->current_coro->ss_contexts);
                }
                swapcontext(&ut->hub->context, &ut->current_coro->context);
                // back to hub
next:
                scheduled_coros = scheduled_coros->next;
        }


	// unschedule all
	scheduled_coros = ut->scheduled_coros_head;
        while(scheduled_coros) {
		ulver_scheduled_coro *next = scheduled_coros->next;
		coro_unschedule(env, ut, scheduled_coros);
		scheduled_coros = next;
	}

	// check if no more coros are running
	ulver_coro *coros = ut->coros;
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

	coro_schedule(env, ut, ut->current_coro);

        // switch to the hub
        __splitstack_getcontext(ut->current_coro->ss_contexts);
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&ut->current_coro->context, &ut->hub->context);
}

static void coro_eval(ulver_env *env, ulver_form *argv) {
	ulver_object *ret = ulver_eval(env, argv);
	ulver_thread *ut = ulver_current_thread(env);
	// once here, mark the coro as dead, and switch back to the hub
	ulver_coro *dead_coro = ut->current_coro;
	dead_coro->dead = 1;
	dead_coro->ret = ret;
	ut->current_coro = ut->hub;
	__splitstack_getcontext(dead_coro->ss_contexts);
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&dead_coro->context, &ut->hub->context);
}

ulver_object *ulver_fun_make_coro(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "make-coro requires an argument");
	ulver_thread *ut = ulver_current_thread(env);
	// ensure the hub is running ...
	// schedule the main coro
	ulver_hub(env);

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
	coro_schedule(env, ut, coro);
	// and schedule the current one so we can come back here
	coro_schedule(env, ut, current_coro);

	// switch to hub
	ut->current_coro = ut->hub;
	__splitstack_getcontext(current_coro->ss_contexts);
	__splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&current_coro->context, &ut->hub->context);
	ut->current_coro = current_coro;
	// back from the hub

	ulver_object *oc = ulver_object_new(env, ULVER_CORO);
	oc->coro = coro;
	return oc;
}

void ulver_hub_schedule_coro(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);
	ulver_coro *current_coro = ut->current_coro;
	// ensure the hub is running
	ulver_hub(env);

	// schedule the coro
	coro_schedule(env, ut, coro);	
	// schedule the current coro so we can return back here
	coro_schedule(env, ut, current_coro);	

	// switch to hub
        ut->current_coro = ut->hub;
        __splitstack_getcontext(current_coro->ss_contexts);
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&current_coro->context, &ut->hub->context);
        ut->current_coro = current_coro;

}

void ulver_coro_yield(ulver_env *env, ulver_object *ret) {
	ulver_thread *ut = ulver_current_thread(env);
	ulver_coro *current_coro = ut->current_coro;
	// ensure the hub is running
        ulver_hub(env);

	// set return value
	current_coro->ret = ret;
	// switch to hub
        ut->current_coro = ut->hub;
        __splitstack_getcontext(current_coro->ss_contexts);
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&current_coro->context, &ut->hub->context);
}
