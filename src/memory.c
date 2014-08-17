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
	uv_mutex_lock(&env->mem_lock);
	env->mem += len;
	uv_mutex_unlock(&env->mem_lock);
	return ptr;
}

void ulver_free(ulver_env *env, void *ptr, uint64_t amount) {
	free(ptr);
	uv_mutex_lock(&env->mem_lock);
	env->mem -= amount;
	uv_mutex_unlock(&env->mem_lock);
}

static void destroy_coro(ulver_env *, ulver_coro *);
void ulver_thread_destroy(ulver_env *env, ulver_thread *ut) {
        // we are here when the thread is marked as dead.

	// the main coro must be destroyed manually
	destroy_coro(env, ut->main_coro);

	// if there are coros still alive, mark them as dead
	// and set their thread attribute to NULL
	// (even if probably it will be useless)
	ulver_coro *coros = ut->coros;
	while(coros) {
		coros->dead = 1;
		coros->thread = NULL;
		coros = coros->next;
	}

        // hub ?
        ulver_hub_destroy(env, ut);

	uv_rwlock_wrlock(&env->threads_lock);

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

        uv_rwlock_wrunlock(&env->threads_lock);

        // and free its memory
        env->free(env, ut, sizeof(ulver_thread));
}

static void object_clear(ulver_env *env, ulver_object *uo) {
	if (uo->str) {
                // strings memory area is zero-suffixed
                env->free(env, uo->str, uo->len + 1);
		uo->str = NULL;
        }

        if (uo->map) {
                ulver_symbolmap_destroy(env, uo->map);
		uo->map = NULL;
        }

        if (uo->stream) {
                env->free(env, uo->stream, sizeof(ulver_uv_stream));
		uo->stream = NULL;
        }

        // if the coro get out of scope we can safely destroy it
        if (uo->coro) {
                destroy_coro(env, uo->coro);
		uo->coro = NULL;
        }

        if (uo->thread) {
                // threads are destroyed independently
                if (uo->thread->refs > 0) {
                        uo->thread->refs--;
                }
		uo->thread = NULL;
        }

        // if the object is a form without source, let's destroy it
        if (uo->form) {
                if (!uo->form->source) {
                        ulver_form_destroy(env, uo->form);
                }
		uo->form = NULL;
        }

	// if it is a channel, consume all of the messages
	// close the pipes and clear the memory structures
	if (uo->chan) {
		while(uo->chan->head) {
			ulver_channel_msg *next = uo->chan->head->next;
			env->free(env, uo->chan->head, sizeof(ulver_channel_msg));
			uo->chan->head = next;
		}	
		close(uo->chan->_pipe[0]);
		close(uo->chan->_pipe[1]);
		env->free(env, uo->chan, sizeof(ulver_channel));
		uo->chan = NULL;
	}

	// free items
        ulver_object_item *item = uo->list;
        while(item) {
                ulver_object_item *next = item->next;
                env->free(env, item, sizeof(ulver_object_item));
                item = next;
        }
	uo->list = NULL;
}

ulver_object *ulver_object_copy_to(ulver_env *env, ulver_object *src, ulver_object *dst) {
	object_clear(env, dst);
	dst->type = src->type;
	dst->len = src->len;
	if (src->str) {
        	dst->str = ulver_utils_strndup(env, src->str, src->len);
	}
        dst->n = src->n;
        dst->d = src->d;

        dst->func = src->func;
        dst->lambda_list = src->lambda_list;
        dst->form = src->form;

	if (src->map) {
		dst->map = ulver_symbolmap_copy(env, src->map);
	}

        ulver_object_item *item = src->list;
        while(item) {
        	ulver_object_push(env, dst, ulver_object_copy(env, item->o));
                item = item->next;
        }
        return dst;
}

ulver_object *ulver_object_copy(ulver_env *env, ulver_object *uo) {
        ulver_object *new = ulver_object_new(env, uo->type);
	return ulver_object_copy_to(env, uo, new);
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

	object_clear(env, uo);

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

	// is a channel ?
	if (uo->chan) {
		ulver_channel_msg *cm = uo->chan->head;
		while(cm) {
			object_mark(env, cm->o);
			cm = cm->next;	
		}
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
			if (us->key) {
				object_mark(env, us->key);
			}
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

	// a coro could be orphaned
	if (coro->thread) {
		if (coro == coro->thread->coros) {
			coro->thread->coros = next;
		}
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

// REMEMBER: get the write GC lock
void ulver_gc(ulver_env *env) {

	env->gc_rounds++;

	// first check for dead threads
        ulver_thread *threads = env->threads;
        while(threads) {
                ulver_thread *next = threads->next;
		// threads cannot die until an object maps to them
                if (threads->dead && !threads->refs) {
                        // TODO what happens if it fails ?
                        uv_thread_join(&threads->t);
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
			if (!env->globals) {
				printf("object %p of type %d is still alive\n", uo, uo->type);
			}
			uo->gc_mark = 0;
		}
		uo = next;
	}

}
