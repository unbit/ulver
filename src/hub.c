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
		next->prev = prev;
	}

	if (prev) {
		prev->next = next;
	}

	if (ut->scheduled_coros_head == sc) {
		ut->scheduled_coros_head = next;
	}

	if (ut->scheduled_coros_tail == sc) {
		ut->scheduled_coros_tail = prev;
	}

	env->free(env, sc, sizeof(ulver_scheduled_coro));
}

void ulver_hub_destroy(ulver_env *env, ulver_thread *ut) {
	if (!ut->hub) return;
	// some scheduled coro could be alive if the coro has not been
	// completely consumed
	ulver_scheduled_coro *usc = ut->scheduled_coros_head;
	while(usc) {
		ulver_scheduled_coro *next = usc->next;
		env->free(env, usc, sizeof(ulver_scheduled_coro));
		usc = next;
	}
	uv_loop_delete(ut->hub_loop);
        ulver_coro_free_context(env, ut->hub);
        env->free(env, ut->hub, sizeof(ulver_coro));
        ut->hub = NULL;
        ut->hub_loop = NULL;
}

static void hub_loop(ulver_env *env, ulver_thread *ut) {
	// back to the creator coro
	ulver_coro_switch(env, ut->hub_creator);
	// run until there are scheduled or blocked coros
	for(;;) {
		// execute all of the sceduled coros
		ulver_scheduled_coro *next, *scheduled_coros = ut->scheduled_coros_head;
        	while(scheduled_coros) {
                	if (scheduled_coros->coro->dead) goto nextcoro;
			ulver_coro_switch(env, scheduled_coros->coro);
nextcoro:
                        next = scheduled_coros->next;
			coro_unschedule(env, ut, scheduled_coros);
			scheduled_coros = next;
		}

		// any blocked coro ?
		int blocked_coros = ut->main_coro->blocked;
		if (blocked_coros) goto uvrun;
		ulver_coro *coros = ut->coros;
		while(coros) {
			if (coros->blocked) {
				blocked_coros = 1;
				break;
			}
			coros = coros->next;
		}

uvrun:
		if (blocked_coros) {
			//printf("****************** ENTERING HUB *********************\n");
			uv_run(ut->hub_loop, UV_RUN_ONCE);
			//printf("****************** EXITING HUB *********************\n");
		}
		// if no more coros are running, end the hub
		else {
			uint8_t running_coros = 0;
			coros = ut->coros;
			while(coros) {
				if (!coros->dead) {
					running_coros = 1;
					break;
				}
				coros = coros->next;
			}
			if (!running_coros) break;
		}
	}
	printf("END OF THE HUB\n");
	ulver_hub_destroy(env, ut);
	// back to main
	ulver_coro_fast_switch(env, ut->main_coro);
}

void ulver_hub(ulver_env *env) {
        ulver_thread *ut = ulver_current_thread(env);
        // is the hub already running ?
        if (ut->hub) return;
	ut->hub = ulver_coro_new(env, hub_loop, ut);
        ut->hub_loop = uv_loop_new();
	ut->hub_creator = ut->current_coro;
	ulver_coro_switch(env, ut->hub);
}

void ulver_hub_schedule_coro(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);

	// schedule the coro
	coro_schedule(env, ut, coro);	
	// and schedule me again to get the result
	coro_schedule(env, ut, ut->current_coro);	

	// switch to hub
	ulver_coro_switch(env, ut->hub);
}

void ulver_hub_schedule_waiters(ulver_env *env, ulver_thread *ut, ulver_coro *coro) {
	if (ut->main_coro->waiting_for == coro) {
		ulver_hub_schedule_coro(env, ut->main_coro);
		ut->main_coro->waiting_for = NULL;
	}

	ulver_coro *coros = ut->coros;
	while(coros) {
		if (coros->waiting_for == coro) {
			ulver_hub_schedule_coro(env, coros);
		}
		coros = coros->next;
	}
}

void ulver_hub_wait(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);
	// add the current coro as a a waiting one
	ut->current_coro->waiting_for = coro;
	ulver_coro_switch(env, ut->hub);
}

// permanently switch to the hub until it dies
ulver_object *ulver_fun_hub(ulver_env *env, ulver_form *argv) {
	// ensure teh hub is running
	ulver_hub(env);
	ulver_thread *ut = ulver_current_thread(env);
	ulver_coro_switch(env, ut->hub);
	return env->nil;
}
