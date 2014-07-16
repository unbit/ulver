#include <ulver.h>

static uint64_t djb33x_hash(char *name, uint64_t namelen) {

        register uint64_t hash = 5381;
        uint64_t i;

        for (i = 0; i < namelen; i++) {
                hash = ((hash << 5) + hash) ^ tolower(name[i]);
        }

        return hash;
}


ulver_symbolmap *ulver_symbolmap_new(ulver_env *env) {
	ulver_symbolmap *smap = env->alloc(env, sizeof(ulver_symbolmap));
	smap->hashtable_size = 1;
	smap->hashtable = env->alloc(env, sizeof(ulver_symbol *) * smap->hashtable_size);
	return smap;
}

static void symbol_hash(ulver_symbolmap *smap, ulver_symbol *us) {
	// rehash the symbol name
	uint64_t hash = djb33x_hash(us->name, us->len) % smap->hashtable_size;
	if (smap->hashtable[hash] == NULL) {
		smap->hashtable[hash] = us;
		us->next = NULL;
		us->prev = NULL;
		return;
	}

	// collision !!!
	ulver_symbol *current_symbol = smap->hashtable[hash];
	us->next = current_symbol;
	current_symbol->prev = us;
	smap->hashtable[hash] = us;
}

static void symbolmap_resize(ulver_env *env, ulver_symbolmap *smap) {
	// double the hashtable size;
	uint64_t old_hashtable_size = smap->hashtable_size;
	ulver_symbol **old_hashtable = smap->hashtable;

	smap->hashtable_size *= 2;
	smap->hashtable = env->alloc(env, sizeof(ulver_symbol *) * smap->hashtable_size);

	// re-hash all of the items
	uint64_t i;
	for(i=0;i<old_hashtable_size;i++) {
		ulver_symbol *us = old_hashtable[i];
		while(us) {
			// get the "next" symbol, before moving the current one
			ulver_symbol *next = us->next;
			symbol_hash(smap, us);	
			us = next;
		}
	}

	env->mem -= sizeof(ulver_symbol *) * old_hashtable_size;
	env->free(env, old_hashtable);
}

ulver_symbol *ulver_symbolmap_set(ulver_env *env, ulver_symbolmap *smap, char *name, uint64_t len, ulver_object *uo) {
	// first of all check for already existent item, and eventually update it
	ulver_symbol *already = ulver_symbolmap_get(env, smap, name, len);
	if (already) {
		// update the value
		already->value = uo;
		return already;
	}

	// do we need to resize the hashtable ?
	if (smap->items + 1 > smap->hashtable_size) {
		symbolmap_resize(env, smap);
	}

	// create the symbol
	ulver_symbol *us = env->alloc(env, sizeof(ulver_symbol));
	us->name = name;
	us->len = len;
	us->value = uo;
	// place it in the hash map
	symbol_hash(smap, us);

	smap->items++;
	return us;
}

ulver_symbol *ulver_symbolmap_get(ulver_env *env, ulver_symbolmap *smap, char *name, uint64_t len) {

	// hash the name
	uint64_t hash = djb33x_hash(name, len) % smap->hashtable_size;

	// now search for the right entry
	ulver_symbol *us = smap->hashtable[hash];
	while(us) {
		if (us->len == len) {
			if (!ulver_utils_memicmp(us->name, name, len)) {
				return us;
			}
		}
		us = us->next;
	}
	return NULL;
}

int ulver_symbolmap_delete(ulver_env *env, ulver_symbolmap *smap, char *name, uint64_t len) {
	ulver_symbol *already = ulver_symbolmap_get(env, smap, name, len);
        if (!already) return -1;

	// get its hash
	uint64_t hash = djb33x_hash(name, len) % smap->hashtable_size;
	// iterate
	ulver_symbol *us = smap->hashtable[hash];
        while(us) {
                if (us->len == len) {
			// found
                        if (!ulver_utils_memicmp(us->name, name, len)) {
				// unlink it
				ulver_symbol *prev = us->prev;
				ulver_symbol *next = us->next;
				if (next) {
					next->prev = prev;
				}
				if (prev) {
					prev->next = next;
				}
				if (us == smap->hashtable[hash]) {
					smap->hashtable[hash] = next;
				}
				free(us);
				env->mem -= sizeof(ulver_symbol);
				smap->items--;
				return 0;
                        }
                }
                us = us->next;
        }

	return -1;	
}

ulver_stackframe *ulver_stack_push(ulver_env *env) {
	ulver_stackframe *ustack = env->alloc(env, sizeof(ulver_stackframe));
	ulver_stackframe *previous_stackframe = env->stack;
	env->stack = ustack;
	ustack->prev = previous_stackframe;
	ustack->locals = ulver_symbolmap_new(env);
	//ustack->fun_locals = ulver_symbolmap_new(env);
	return ustack;
}

void ulver_stack_pop(ulver_env *env) {
	ulver_stackframe *ustack = env->stack;
	env->stack = ustack->prev;

	// re-hash all of the items
        uint64_t i;
        for(i=0;i<ustack->locals->hashtable_size;i++) {
                ulver_symbol *us = ustack->locals->hashtable[i];
                while(us) {
                        // get the "next" symbol, before moving the current one
                        ulver_symbol *next = us->next;
			env->mem -= sizeof(ulver_symbol);
			free(us);
                        us = next;
                }
        }

	free(ustack->locals->hashtable);	
	env->mem -= sizeof(ulver_symbol *) * ustack->locals->hashtable_size;

	free(ustack->locals);
	env->mem -= sizeof(ulver_symbolmap);

	env->mem -= sizeof(ulver_stackframe);
	env->free(env, ustack);
}

