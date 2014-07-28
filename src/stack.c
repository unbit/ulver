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

	env->free(env, old_hashtable, sizeof(ulver_symbol *) * old_hashtable_size);
}

static char *resolve_symbol_name(ulver_env *env, char *name, uint64_t *len, uint8_t no_package) {

	if (no_package) return NULL;

	char *new_name = NULL;

        // first of all check for package prefix
	char *colon = NULL;
        if (*len > 2 && name[0] != ':' && (colon = memchr(name, ':', *len)) ) {
                // now check if the package exists
		pthread_rwlock_rdlock(&env->packages_lock);
                ulver_symbol *package = ulver_symbolmap_get(env, env->packages, name, colon-name, 1);
		pthread_rwlock_unlock(&env->packages_lock);
                if (!package) {
                        ulver_error(env, "unable to find package %.*s", colon-name, name);
			*len = 0;
                        return NULL;
                }
                // the package exists, does it export the symbol ?
                ulver_symbol *us = ulver_symbolmap_get(env, package->value->map, colon + 1, *len - ((colon + 1) - name), 1);
                if (!us) {
			uint64_t i;
        		for(i=0;i<package->value->map->hashtable_size;i++) {
                		ulver_symbol *us = package->value->map->hashtable[i];
                		while(us) {
                        		us = us->next;
                		}
        		}	
                        ulver_error(env, "package %.*s does not export symbol %.*s", colon-name, name, *len - ((colon + 1) - name), colon + 1);
			*len = 0;
                        return NULL;
                }

                // ok let's check it for the real symbol
		return NULL;
        }

        // the symbol has no package prefix, use the current one (if not cl-user)
	// lock
	pthread_rwlock_rdlock(&env->packages_lock);
	ulver_object *current_package = env->current_package;
	pthread_rwlock_unlock(&env->packages_lock);

        if (current_package == env->cl_user) return NULL;
	// unlock

        // first of all check if the symbol is exported by the current package
        ulver_symbol *us = ulver_symbolmap_get(env, current_package->map, name, *len, 1);
        if (!us) return NULL;

        // it is exported, let's prefix it (we use raw memory, as we will free it asap);
        new_name = malloc(current_package->len + 1 + *len);
        memcpy(new_name, current_package->str, current_package->len);
        new_name[current_package->len] = ':';
        memcpy(new_name + current_package->len + 1, name, *len);

	*len += current_package->len + 1;

        return new_name;
}

ulver_symbol *ulver_symbolmap_set(ulver_env *env, ulver_symbolmap *smap, char *name, uint64_t len, ulver_object *uo, uint8_t no_package) {
        // first of all check for already existent item, and eventually update it
        ulver_symbol *already = ulver_symbolmap_get(env, smap, name, len, no_package);
        if (already) {
                // update the value
                already->value = uo;
                return already;
        }

	char *new_name = resolve_symbol_name(env, name, &len, no_package);
        // a 0 length means error
        if (!new_name && len == 0) return NULL;
        if (new_name) name = new_name;

        // do we need to resize the hashtable ?
        if (smap->items + 1 > smap->hashtable_size) {
                symbolmap_resize(env, smap);
        }

        // create the symbol
        ulver_symbol *us = env->alloc(env, sizeof(ulver_symbol));
	if (new_name) {
        	us->name = ulver_utils_strndup(env, name, len);;
		us->free_name = 1;
	}
	else {
		us->name = name;
	}
        us->len = len;
        us->value = uo;
        // place it in the hash map
        symbol_hash(smap, us);

        smap->items++;

        if (new_name) free(new_name);
        return us;
}


ulver_symbol *ulver_symbolmap_get(ulver_env *env, ulver_symbolmap *smap, char *name, uint64_t len, uint8_t no_package) {


	char *new_name = resolve_symbol_name(env, name, &len, no_package);
	// a 0 length means error
	if (!new_name && len == 0) return NULL;
	if (new_name) name = new_name;

	// hash the name
	uint64_t hash = djb33x_hash(name, len) % smap->hashtable_size;

	// now search for the right entry
	ulver_symbol *us = smap->hashtable[hash];
	while(us) {
		if (us->len == len) {
			if (!ulver_utils_memicmp(us->name, name, len)) {
				if (new_name) free(new_name);
				return us;
			}
		}
		us = us->next;
	}
	if (new_name) free(new_name);
	return NULL;
}

int ulver_symbolmap_delete(ulver_env *env, ulver_symbolmap *smap, char *name, uint64_t len, uint8_t no_package) {
	ulver_symbol *already = ulver_symbolmap_get(env, smap, name, len, no_package);
        if (!already) return -1;

	char *new_name = resolve_symbol_name(env, name, &len, no_package);
        // a 0 length means error
        if (!new_name && len == 0) return -1;
        if (new_name) name = new_name;

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
				if (us->free_name) env->free(env, us->name, us->len+1);
				env->free(env, us, sizeof(ulver_symbol));
				smap->items--;
				if (new_name) free(new_name);
				return 0;
                        }
                }
                us = us->next;
        }

	if (new_name) free(new_name);
	return -1;	
}

ulver_stackframe *ulver_stack_push(ulver_env *env, ulver_thread *ut) {
	ulver_stackframe *ustack = env->alloc(env, sizeof(ulver_stackframe));
	ulver_stackframe *previous_stackframe = ut->current_coro->stack;
	ut->current_coro->stack = ustack;
	ustack->prev = previous_stackframe;
	ustack->locals = ulver_symbolmap_new(env);
	return ustack;
}

void ulver_symbolmap_destroy(ulver_env *env, ulver_symbolmap *smap) {
	uint64_t i;
        for(i=0;i<smap->hashtable_size;i++) {
                ulver_symbol *us = smap->hashtable[i];
                while(us) {
                        // get the "next" symbol, before moving the current one
                        ulver_symbol *next = us->next;
			if (us->free_name) env->free(env, us->name, us->len+1);
                        env->free(env, us, sizeof(ulver_symbol));
                        us = next;
                }
        }

        env->free(env, smap->hashtable, sizeof(ulver_symbol *) * smap->hashtable_size);

        env->free(env, smap, sizeof(ulver_symbolmap));	
}

void ulver_stack_pop(ulver_env *env, ulver_thread *ut) {
	ulver_stackframe *ustack = ut->current_coro->stack;
	ut->current_coro->stack = ustack->prev;

	ulver_symbolmap_destroy(env, ustack->locals);

	env->free(env, ustack, sizeof(ulver_stackframe));
}

