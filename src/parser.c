#include <ulver.h>

ulver_form *ulver_form_new(ulver_env *env, ulver_source *us, uint8_t type) {
        ulver_form *uf = env->alloc(env, sizeof(ulver_form));
        uf->type = type;
        uf->parent = us->form_list_current;
	uf->line = us->lines;
	uf->line_pos = us->line_pos;
	if (!us->form_new) {
		us->form_new = uf;
	}
	return uf; 
}

ulver_form *ulver_form_push(ulver_env *env, ulver_source *us, uint8_t type) {
	ulver_form *uf = ulver_form_new(env, us, type);
	ulver_form *parent = us->form_list_current;
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

ulver_form *ulver_form_commit(ulver_env *env, ulver_source *us) {
	ulver_form *uf = NULL;
	if (us->token) {
		uint8_t type = ULVER_SYMBOL;

		if (us->is_doublequoted) {
			type = ULVER_STRING;	
		}
		else if (us->token_len > 0 && us->token[0] == ':') {
			type = ULVER_KEYWORD;
		}
        	else if (ulver_utils_is_a_number(us->token, us->token_len)) {
                	type = ULVER_NUM;
		}
        	else if (ulver_utils_is_a_float(us->token, us->token_len)) {
                	type = ULVER_FLOAT;
		}
        	uf = ulver_form_push(env, us, type);
                uf->value = us->token;
                uf->len = us->token_len;
		uf->line = us->lines;
		uf->line_pos = us->line_pos - us->token_len;
	}
        us->token = NULL;
	us->token_len = 0;
	return uf;
}

ulver_form *ulver_parse(ulver_env *env, char *buf, size_t len) {

	// append the new source to the env
	ulver_source *source = env->alloc(env, sizeof(ulver_source));
	source->str = ulver_utils_strndup(env, buf, len);
	source->len = len;

	//lock
	pthread_mutex_lock(&env->sources_lock);
	if (env->sources) {
		ulver_source *first = env->sources;
		source->next = first;
	}
	env->sources = source;
	pthread_mutex_unlock(&env->sources_lock);
	//unlock

	// use the new memory
	buf = source->str;

	// create the root form (if needed)
	if (!source->form_root) {
		source->form_root = ulver_form_new(env, source, ULVER_LIST);
		
	}

	// form_new is what the parser returns
	source->form_new = NULL;
	source->form_list_current = source->form_root;

	int is_escaped = 0;
	int is_comment = 0;

	// initialize env data for the parser
	source->is_doublequoted = 0;
	source->token = NULL;
	source->token_len = 0;
	source->lines++;
	source->line_pos = 0;

	for(source->pos=0;source->pos<len;source->pos++) {
		char c = buf[source->pos];
		source->line_pos++;

		if (c == '\n') {
			source->lines++;
			source->line_pos = 0;
		}

		if (is_comment) {
			if (c == '\n') {
				is_comment = 0;
			}
			continue;
		}

		if (source->is_doublequoted) {
			if (!source->token) {
				source->token = buf + source->pos;
				source->token_len = 0;
			}

			if (is_escaped) {
				source->token_len++;
				is_escaped = 0;
				continue;
			}

			if (c == '\\') {
				is_escaped = 1;
				continue;
			}	


			if (c == '"') {
				ulver_form *uf = ulver_form_commit(env, source);
				source->is_doublequoted = 0;
				is_escaped = 0;
				continue;
			}
			source->token_len++;
			continue;
		}

		switch(c) {
			case '(':
				ulver_form_commit(env, source);
				source->form_list_current = ulver_form_push(env, source, ULVER_LIST);
				break;
			case ')':
				ulver_form_commit(env, source);
				source->form_list_current = source->form_list_current->parent;
				if (source->form_list_current == NULL) {
					printf("[ERROR] unexpected end of program ???\n");
					return NULL;
				}
				break;
			case '"':
				source->is_doublequoted = 1;
				break;
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				ulver_form_commit(env, source);
				break;
			case ';':
				is_comment = 1;
				ulver_form_commit(env, source);
                                break;
			default:
				if (source->token == NULL) {
					source->token = buf + source->pos;
					source->token_len = 0;
				}
				source->token_len++;
				break;
		}
	}

	ulver_form_commit(env, source);

	return source->form_new;
}
