#include <ulver.h>

ulver_object *ulver_fun_length(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);

        ulver_object *sequence = ulver_eval(env, argv);
        if (!sequence) return NULL;

        if (sequence->type != ULVER_LIST && sequence->type != ULVER_STRING) {
                return ulver_error(env, ULVER_ERR_NOT_SEQ);
        }

        return ulver_object_from_num(env, ulver_utils_length(sequence));
}


ulver_object *ulver_fun_subseq(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);

        ulver_object *sequence = ulver_eval(env, argv);
        if (!sequence) return NULL;

        if (sequence->type != ULVER_LIST && sequence->type != ULVER_STRING) {
                return ulver_error(env, ULVER_ERR_NOT_SEQ);
        }

        ulver_object *index = ulver_eval(env, argv->next);
        if (!index) return NULL;
        if (index->type != ULVER_NUM) return ulver_error(env, ULVER_ERR_NOT_NUM);
        if (index->n < 0) return ulver_error(env, ULVER_ERR_ZERO);

        // check for the optional "end" argument
        uint64_t sequence_len = ulver_utils_length(sequence);
        uint64_t end_index = sequence_len;
        if (argv->next->next) {
                ulver_object *end = ulver_eval(env, argv->next->next);
                // eval error ?
                if (!end) return NULL;
                // is it nil or a num ??
                if (end != env->nil && end->type != ULVER_NUM) {
                        return ulver_error(env, ULVER_ERR_NOT_NUM);
                }
                // if it is a num, set it !
                if (end->type == ULVER_NUM) {
                        if (end->n < 0) {
                                return ulver_error(env, ULVER_ERR_ZERO);
                        }
                        end_index = end->n;
                }
        }

        if (index->n > sequence_len) return ulver_error(env, ULVER_ERR_RANGE);
        if (end_index > sequence_len) return ulver_error(env, ULVER_ERR_RANGE);

        // is it a string ?
        if (sequence->type == ULVER_STRING) {
                return ulver_object_from_string(env, sequence->str + index->n, end_index - index->n);
        }

        // is it a list ?
        if (sequence->type == ULVER_LIST) {
                uint64_t how_many_items = end_index - index->n;
                if (how_many_items > 0) how_many_items--;
                uint64_t start_item = index->n;
                uint64_t current_item = 0;
                uint64_t pushed_items = 0;
                ulver_object *new_list = ulver_object_new(env, ULVER_LIST);
                ulver_object_item *item = sequence->list;
                while(item) {
                        if (pushed_items >= how_many_items) break;
                        if (current_item >= start_item) {
                                ulver_object_push(env, new_list, ulver_object_copy(env, item->o));
                                pushed_items++;
                        }
                        current_item++;
                        item = item->next;
                }
                return new_list;
        }

        return env->nil;
}


ulver_object *ulver_fun_first(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 1);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_second(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 2);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_third(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 3);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_fourth(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 4);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_fifth(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 5);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_sixth(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 6);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_seventh(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 7);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_eighth(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 8);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_ninth(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 9);
        if (ret) return ret;
        return env->nil;
}

ulver_object *ulver_fun_tenth(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *list = ulver_eval(env, argv);
        if (!list) return NULL;
        if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *ret = ulver_utils_nth(list, 10);
        if (ret) return ret;
        return env->nil;
}



ulver_object *ulver_fun_getf(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        ulver_object *plist = ulver_eval(env, argv);
        if (!plist) return NULL;
        if (plist->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
        ulver_object *place = ulver_eval(env, argv->next);
        if (!place) return NULL;

        ulver_object_item *key = plist->list;
        while(key) {
                if (ulver_utils_eq(key->o, place)) {
                        if (key->next) return key->next->o;
                        return ulver_error(env, ULVER_ERR_ODD);
                }
                ulver_object_item *value = key->next;
                if (!value) return ulver_error(env, ULVER_ERR_ODD);
                key = value->next;
        }
        return env->nil;
}

ulver_object *ulver_fun_find(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);   
        ulver_object *item = ulver_eval(env, argv);
        if (!item) return NULL;
        ulver_object *sequence = ulver_eval(env, argv->next);
        if (!sequence) return NULL;
        if (sequence->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);

        ulver_object_item *found = sequence->list;
        while(found) {
                if (ulver_utils_eq(found->o, item)) return found->o;
                found = found->next;
        }

        return env->nil;
}

ulver_object *ulver_fun_position(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        ulver_object *item = ulver_eval(env, argv);
        if (!item) return NULL;
        ulver_object *sequence = ulver_eval(env, argv->next);
        if (!sequence) return NULL;
        if (sequence->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);

        uint64_t count = 0;
        ulver_object_item *found = sequence->list;
        while(found) {
                if (ulver_utils_eq(found->o, item)) return ulver_object_from_num(env, count);
                count++;
                found = found->next;
        }
        return env->nil;
}


ulver_object *ulver_fun_last(ulver_env *env, ulver_form *argv) {
	uint64_t last = 1;
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *src = ulver_eval(env, argv);
	if (!src) return NULL;
	if (src->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);

	if (argv->next) {
		ulver_object *amount = ulver_eval(env, argv->next);
		if (!amount) return NULL;
		if (amount->type != ULVER_NUM) return ulver_error(env, ULVER_ERR_NOT_NUM);
		if (amount->n < 0) return ulver_error(env, ULVER_ERR_NEGATIVE);
		last = amount->n;
	}
	uint64_t list_len = ulver_utils_length(src);
	uint64_t first_item = 0;
	if (last <= list_len) {
		first_item = list_len - last;
	}
	ulver_object *new_list = ulver_object_new(env, ULVER_LIST);
	uint64_t i;
	for(i=first_item;i<list_len;i++) {
		ulver_object *item = ulver_utils_nth(src, i+1);		
		if (!item) return ulver_error(env, ULVER_ERR_RANGE);
		ulver_object_push(env, new_list, item);
	}
	return new_list;
}

ulver_object *ulver_fun_union(ulver_env *env, ulver_form *argv) {
	ulver_object *new_list = ulver_object_new(env, ULVER_LIST);
	while(argv) {
		ulver_object *list = ulver_eval(env, argv);
		if (!list) return NULL;
		if (list->type != ULVER_LIST) return ulver_error(env, ULVER_ERR_NOT_LIST);
		ulver_object_item *item = list->list;
		while(item) {
			ulver_object_push(env, new_list, ulver_object_copy(env, item->o));
			item = item->next;
		}
		argv = argv->next;
	}
	return new_list;
}
