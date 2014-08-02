#include <ulver.h>

/*
        the ulver GC

Each thread is free to run the gc

The first step of the gc is joining dead threads

For each alive thread, the list of coros is checked for 
objects to mark.

Objects mapped to global symbols, funcs sumbols or packages are
marked too.

Finally each non-marked object (and non-protected) is destroyed.

*/

void *ulver_alloc(ulver_env *env, uint64_t len) {
	void *ptr = calloc(1, len);
	if (!ptr) {
		perror("calloc()");
		exit(1);
	}
	pthread_mutex_lock(&env->mem_lock);
	env->mem += len;
	pthread_mutex_unlock(&env->mem_lock);
	return ptr;
}

void ulver_free(ulver_env *env, void *ptr, uint64_t amount) {
	free(ptr);
	pthread_mutex_lock(&env->mem_lock);
	env->mem -= amount;
	pthread_mutex_unlock(&env->mem_lock);
}

static void destroy_coro(ulver_env *, ulver_coro *);
void ulver_thread_destroy(ulver_env *env, ulver_thread *ut) {
        // we are here when the thread is marked as dead.

	destroy_coro(env, ut->main_coro);

	ulver_coro *coro = ut->coros;
	while(coro) {
		ulver_coro *next = coro->next;
		destroy_coro(env, coro);
		coro = next;
	}

        // hub ?
        ulver_hub_destroy(env, ut);

	pthread_rwlock_wrlock(&env->threads_lock);

        ulver_thread *next = ut->next;
        ulver_thread *prev = ut->prev;
        if (next) {
                next->prev = ut->prev;
        }
        if (prev) {
                prev->next = ut->next;
        }

        if (ut == env->threads) {
                env->threads = next;
        }

        pthread_rwlock_unlock(&env->threads_lock);

        // and free its memory
        env->free(env, ut, sizeof(ulver_thread));
}


void ulver_object_destroy(ulver_env *env, ulver_object *uo) {
        ulver_object *prev = uo->gc_prev;
        ulver_object *next = uo->gc_next;

        if (next) {
                next->gc_prev = prev;
        }

        if (prev) {
                prev->gc_next = next;
        }

        if (uo == env->gc_root) {
                env->gc_root = next;
        }

        if (uo->str) {
                // strings memory area is zero-suffixed
                env->free(env, uo->str, uo->len + 1);
        }

        if (uo->map) {
                ulver_symbolmap_destroy(env, uo->map);
        }

        if (uo->stream) {
                env->free(env, uo->stream, sizeof(ulver_uv_stream));
        }

	if (uo->coro) {
		//destroy_coro(env, uo->coro);
	}

        if (uo->thread) {
		// threads are destroyed independently
        }

        // free items
        ulver_object_item *item = uo->list;
        while(item) {
                ulver_object_item *next = item->next;
                env->free(env, item, sizeof(ulver_object_item));
                item = next;
        }

        env->free(env, uo, sizeof(ulver_object));
}


static void mark_symbolmap(ulver_env *, ulver_symbolmap *);
static void object_mark(ulver_env *env,  ulver_object *uo) {
	uo->gc_mark = 1;
	// has a return value ?
	if (uo->ret) {
		object_mark(env, uo->ret);
	}
	// has a map ?
	if (uo->map) {
		mark_symbolmap(env, uo->map);
	}
	// is it a list ?
	ulver_object_item *item = uo->list;
	while(item) {
		object_mark(env, item->o);
		item = item->next;
	}
}

static void mark_symbolmap(ulver_env *env, ulver_symbolmap *smap) {
	uint64_t i;
	for(i=0;i<smap->hashtable_size;i++) {
                ulver_symbol *us = smap->hashtable[i];
                while(us) {
                        // mark the object and its children
                        object_mark(env, us->value);
                        us = us->next;
                }
        }
}

static void destroy_coro(ulver_env *env, ulver_coro *coro) {
	// consume the whole stack
        while(coro->stack) {
                ulver_stack_pop(env, coro->thread, coro);
        }

	// remove the coro from the chain
	ulver_coro *prev = coro->prev;
	ulver_coro *next = coro->next;

	if (prev) {
		prev->next = next;
	}

	if (next) {
		next->prev = prev;
	}

	if (coro == coro->thread->coros) {
		coro->thread->coros = next;
	}

	ulver_coro_free_context(env, coro);
	env->free(env, coro, sizeof(ulver_coro));
}


static void mark_coro(ulver_env *env, ulver_coro *coro) {
	ulver_stackframe *stack = coro->stack;
        while(stack) {
        	// iterate all locals
                mark_symbolmap(env, stack->locals);
                // iterate all objects
                ulver_object *so = stack->objects;
                while(so) {
                	object_mark(env, so);
                        so = so->stack_next;
                }
                // get the return value (if any)
                if (stack->ret) {
                	object_mark(env, stack->ret);
                }
                stack = stack->prev;
	}
        if (coro->ret) {
        	object_mark(env, coro->ret);
        }
}

void ulver_gc(ulver_env *env) {

	// get the write GC lock
	pthread_rwlock_wrlock(&env->gc_lock);

	env->gc_rounds++;

	// first check for dead threads
        ulver_thread *threads = env->threads;
        while(threads) {
                ulver_thread *next = threads->next;
                if (threads->dead) {
                        // TODO what happens if it fails ?
                        pthread_join(threads->t, NULL);
                        ulver_thread_destroy(env, threads);
                }
                threads = next;
        }

	// now mark objects
	threads = env->threads;
	while(threads) {
		mark_coro(env, threads->main_coro);
		ulver_coro *coros = threads->coros;
		while(coros) {
                	mark_coro(env, coros);
                	coros = coros->next;
        	}
		threads = threads->next;
	}

	// now mark globals, funcs and packages
        // referring to objects owned by the current thread
        if (env->globals) {
                mark_symbolmap(env, env->globals);
        }

        if (env->funcs) {
                mark_symbolmap(env, env->funcs);
        }

        if (env->packages) {
                mark_symbolmap(env, env->packages);
        }

	

	// sweep objects
	ulver_object *uo = env->gc_root;
	while(uo) {
		// get the next object pointer as the current one
		// could be destroyed
		ulver_object *next = uo->gc_next;
		if (uo->gc_mark == 0 && !uo->gc_protected) {
			ulver_object_destroy(env, uo);	
		}
		else {
			uo->gc_mark = 0;
		}
		uo = next;
	}

	// restart the world
	pthread_rwlock_unlock(&env->gc_lock);

}
