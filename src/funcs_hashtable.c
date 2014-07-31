#include <ulver.h>

ulver_object *ulver_fun_make_hash_table(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_object_new(env, ULVER_HASHTABLE);
	uo->map = ulver_symbolmap_new(env);
	return uo;
}

ulver_object *ulver_fun_gethash(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, "gethash requires two arguments");
	ulver_object *ht = ulver_eval(env, argv);
	if (!ht) return NULL;
	if (ht->type != ULVER_HASHTABLE) return ulver_error_form(env, argv, "is not a hashtable");

	ulver_object *key = ulver_eval(env, argv->next);
	if (!key) return NULL;
	if (key->type != ULVER_STRING && key->type != ULVER_KEYWORD) return ulver_error_form(env, argv->next, "is not a valid hashtable key");

	ulver_symbol *us = ulver_symbolmap_get(env, ht->map, key->str, key->len, 1);
	if (!us) {
		if (argv->next->next) {
			return ulver_eval(env, argv->next->next);
		}
		return env->nil;
	}

	return us->value;
	
}

ulver_object *ulver_fun_sethash(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next || !argv->next->next) return ulver_error(env, "sethash requires three arguments");
        ulver_object *ht = ulver_eval(env, argv);
        if (!ht) return NULL;
        if (ht->type != ULVER_HASHTABLE) return ulver_error_form(env, argv, "is not a hashtable");

        ulver_object *key = ulver_eval(env, argv->next);
        if (!key) return NULL;
        if (key->type != ULVER_STRING && key->type != ULVER_KEYWORD) return ulver_error_form(env, argv->next, "is not a valid hashtable key");

	ulver_object *val = ulver_eval(env, argv->next->next);
	if (!val) return NULL;

	ulver_symbolmap_set(env, ht->map, key->str, key->len, val, 1);

	return val;
}
