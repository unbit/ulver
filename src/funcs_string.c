#include <ulver.h>

ulver_object *ulver_fun_read_from_string(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *source = ulver_eval(env, argv);
        if (!source) return NULL;
        if (source->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);
        ulver_form *uf = ulver_parse(env, source->str, source->len);
        if (!uf) return ulver_error_form(env, ULVER_ERR_PARSE, uf, NULL);
        ulver_object *form = ulver_object_new(env, ULVER_FORM);
        form->form = uf;
        return form;
}

ulver_object *ulver_fun_string_downcase(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *source = ulver_eval(env, argv);
        if (!source) return NULL;
        if (source->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);
	ulver_object *uo = ulver_object_from_string(env, source->str, source->len);
	ulver_utils_tolower(uo->str, uo->len);	
        return uo;
}

ulver_object *ulver_fun_string_upcase(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *source = ulver_eval(env, argv);
        if (!source) return NULL;
        if (source->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);
        ulver_object *uo = ulver_object_from_string(env, source->str, source->len);
        ulver_utils_toupper(uo->str, uo->len);
        return uo;
}

ulver_object *ulver_fun_string_split(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        ulver_object *source = ulver_eval(env, argv);
        if (!source) return NULL;
        if (source->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);

        ulver_object *delim = ulver_eval(env, argv->next);
        if (!delim) return NULL;
        if (delim->type != ULVER_STRING) return ulver_error(env, ULVER_ERR_NOT_STRING);

	ulver_object *new_list = ulver_object_new(env, ULVER_LIST);

	char *next_string = source->str;
	uint64_t next_string_len = source->len;
	for(;;) {
		char *occurrence = strstr(next_string, delim->str);
		if (!occurrence) {
			ulver_object_push(env, new_list, ulver_object_from_string(env, next_string, next_string_len));
			break;
		}
		next_string_len = occurrence - next_string;
		ulver_object_push(env, new_list, ulver_object_from_string(env, next_string, next_string_len));
		next_string = occurrence + strlen(delim->str);
		next_string_len = source->len - (next_string - source->str);
	}

        return new_list;
}
