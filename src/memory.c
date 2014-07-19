#include <ulver.h>

void *ulver_alloc(ulver_env *env, uint64_t len) {
	void *ptr = calloc(1, len);
	if (!ptr) {
		perror("calloc()");
		exit(1);
	}
	env->mem += len;
	return ptr;
}

void ulver_free(ulver_env *env, void *ptr, uint64_t amount) {
	free(ptr);
	env->mem -= amount;
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

void ulver_gc(ulver_env *env) {

	env->gc_rounds++;

	// iterate stack frames
	ulver_stackframe *stack = env->stack;
	while(stack) {
		// iterate all locals
		uint64_t i;
		ulver_symbolmap *smap = stack->locals;
		for(i=0;i<smap->hashtable_size;i++) {
			ulver_symbol *us = smap->hashtable[i];
			while(us) {
				// mark the object and its children
				object_mark(env, us->value);
				us = us->next;
			}	
		}
		smap = stack->fun_locals;
                for(i=0;i<smap->hashtable_size;i++) {
                        ulver_symbol *us = smap->hashtable[i];
                        while(us) {
                                // mark the object and its children
                                object_mark(env, us->value);
                                us = us->next;
                        }
                }
		
		// get the return value (if any)
		if (stack->ret) {
			stack->ret->gc_mark = 1;
		}
		stack = stack->prev;
	}

	// and now sweep ...

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
}
