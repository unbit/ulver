#include <ulver.h>

ulver_form *ulver_form_new(ulver_env *env, uint8_t type) {
        ulver_form *uf = env->alloc(env, sizeof(ulver_form));
        uf->type = type;
        uf->parent = env->form_list_current;
	uf->line = env->lines;
	uf->line_pos = env->line_pos;
	if (!env->form_new) {
		env->form_new = uf;
	}
	return uf; 
}

ulver_form *ulver_form_push(ulver_env *env, uint8_t type) {
	ulver_form *uf = ulver_form_new(env, type);
	ulver_form *parent = env->form_list_current;
	if (!parent->list) {
		parent->list = uf;
		return uf;	
	}

	ulver_form *next = parent->list;
	while(next) {
		if (!next->next) {
			next->next = uf;
			return uf;
		}
		next = next->next;
	}

	return NULL;
}

void ulver_form_destroy(ulver_env *env, ulver_form *uf) {
	ulver_form *list = uf->list;
	while(list) {
		ulver_form *next = list->next;
		ulver_form_destroy(env, list);
		list = next;
	}
	env->free(env, uf, sizeof(ulver_form));
}

ulver_form *ulver_form_commit(ulver_env *env) {
	ulver_form *uf = NULL;
	if (env->token) {
		uint8_t type = ULVER_SYMBOL;

		if (env->is_doublequoted) {
			type = ULVER_STRING;	
		}
		else if (env->token_len > 0 && env->token[0] == ':') {
			type = ULVER_KEYWORD;
		}
        	else if (ulver_utils_is_a_number(env->token, env->token_len)) {
                	type = ULVER_NUM;
		}
        	else if (ulver_utils_is_a_float(env->token, env->token_len)) {
                	type = ULVER_FLOAT;
		}
        	uf = ulver_form_push(env, type);
                uf->value = env->token;
                uf->len = env->token_len;
		uf->line = env->lines;
		uf->line_pos = env->line_pos - env->token_len;
	}
        env->token = NULL;
	env->token_len = 0;
	return uf;
}

ulver_form *ulver_parse(ulver_env *env, char *buf, size_t len) {

	// append the new source to the env
	ulver_source *source = env->alloc(env, sizeof(ulver_source));
	source->str = ulver_utils_strndup(env, buf, len);
	source->len = len;
	if (env->sources) {
		ulver_source *first = env->sources;
		source->next = first;
	}
	env->sources = source;

	// use the new memory
	buf = source->str;

	// create the root form (if needed)
	if (!env->form_root) {
		env->form_root = ulver_form_new(env, ULVER_LIST);
		
	}

	// form_new is what the parser returns
	env->form_new = NULL;
	env->form_list_current = env->form_root;

	int is_escaped = 0;
	int is_comment = 0;

	// initialize env data for the parser
	env->is_doublequoted = 0;
	env->token = NULL;
	env->token_len = 0;
	env->lines++;
	env->line_pos = 0;

	for(env->pos=0;env->pos<len;env->pos++) {
		char c = buf[env->pos];
		env->line_pos++;

		if (c == '\n') {
			env->lines++;
			env->line_pos = 0;
		}

		if (is_comment) {
			if (c == '\n') {
				is_comment = 0;
			}
			continue;
		}

		if (env->is_doublequoted) {
			if (!env->token) {
				env->token = buf + env->pos;
				env->token_len = 0;
			}

			if (is_escaped) {
				env->token_len++;
				is_escaped = 0;
				continue;
			}

			if (c == '\\') {
				is_escaped = 1;
				continue;
			}	


			if (c == '"') {
				ulver_form *uf = ulver_form_commit(env);
				env->is_doublequoted = 0;
				is_escaped = 0;
				continue;
			}
			env->token_len++;
			continue;
		}

		switch(c) {
			case '(':
				env->form_list_current = ulver_form_push(env, ULVER_LIST);
				break;
			case ')':
				ulver_form_commit(env);
				env->form_list_current = env->form_list_current->parent;
				if (env->form_list_current == NULL) {
					printf("end of program ???\n");
				}
				break;
			case '"':
				env->is_doublequoted = 1;
				break;
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				ulver_form_commit(env);
				break;
			case ';':
				is_comment = 1;
				ulver_form_commit(env);
                                break;
			default:
				if (env->token == NULL) {
					env->token = buf + env->pos;
					env->token_len = 0;
				}
				env->token_len++;
				break;
		}
	}

	ulver_form_commit(env);

	return env->form_new;
}
