#include <ulver.h>

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

static void object_mark(ulver_env *env, ulver_object *uo) {
	uo->gc_mark = 1;
	// is it a list ?
	ulver_object *item = uo->list;
	while(item) {
		object_mark(env, item);
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

// this function MUST be always called with gc_lock held
void ulver_gc(ulver_env *env) {

	uint64_t i;

	env->gc_rounds++;

	// when the gc runs, all the threads must be locked
	// this means that gc is a stop-the-world procedure

	// iterate over threads stack frames
	pthread_rwlock_rdlock(&env->threads_lock);
	ulver_thread *ut = env->threads;
	while(ut) {
		// lock the thread (it could be running)
		pthread_mutex_lock(&ut->lock);
		printf("ready...\n");
		ulver_stackframe *stack = ut->stack;
		while(stack) {
			// iterate all locals
			mark_symbolmap(env, stack->locals);
			// get the return value (if any, a function could return multiple values)
			ulver_object *ret = stack->ret;
			while(ret) {
				object_mark(env, ret);
				ret = ret->ret_next;
			}
			stack = stack->prev;
		}
		pthread_mutex_unlock(&ut->lock);
		ut = ut->next;
	}
	pthread_rwlock_unlock(&env->threads_lock);
		printf("done...\n");
	// the thread list has been unlocked
	// now new threads can spawn

	// now mark globals, funcs and packages
	pthread_rwlock_wrlock(&env->globals_lock);
	mark_symbolmap(env, env->globals);
	pthread_rwlock_unlock(&env->globals_lock);

	pthread_rwlock_wrlock(&env->funcs_lock);
	mark_symbolmap(env, env->funcs);
        pthread_rwlock_unlock(&env->funcs_lock);

	pthread_rwlock_wrlock(&env->packages_lock);
	mark_symbolmap(env, env->packages);
        pthread_rwlock_unlock(&env->packages_lock);

	printf("sweep\n");

	// and now sweep ... (we need to lock the objects list again)
	pthread_mutex_lock(&env->gc_root_lock);
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
	pthread_mutex_unlock(&env->gc_root_lock);

	printf("sweeped\n");

	// now the gc lock can be released safely...
}
