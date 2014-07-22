#include <ulver.h>

void ulver_utils_print_list(ulver_env *env, ulver_object *uo) {
	ulver_object *item = uo->list;
        while(item) {
                if (item->type == ULVER_LIST) {
                        printf("(");
                        ulver_utils_print_list(env, item);
                	printf(")");
                }
                else if (item->type == ULVER_STRING) {
                	printf("\"%.*s\"", (int) item->len, item->str);
                }
                else if (item->type == ULVER_KEYWORD) {
                	printf("%.*s", (int) item->len, item->str);
                }
		else if(item->type == ULVER_FUNC) {
			if (item->str) {
				printf("#<FUNCTION %.*s ", (int) item->len, item->str);
			}
			else {
				printf("#<FUNCTION :LAMBDA ");
			}
			if (item->lambda_list) {
				printf("(");
				ulver_utils_print_form(item->lambda_list);
				printf(") ");
			}
			if (item->form) {
				ulver_utils_print_form(item->form);
			}
			else {
				printf("<C FUNC>");
			}
			printf(">");
		}
                else if (item->type == ULVER_NUM) {
                	printf("%lld", (long long int) item->n);
                }
                else if (item->type == ULVER_FLOAT) {
                	printf("%f", item->d);
                }
                else if (item->type == ULVER_TRUE) {
                	printf("T");
                }
		else if (item->type == ULVER_FORM) {
                	ulver_utils_print_form(item->form);
        	}
		else if (item->type == ULVER_PACKAGE) {
                        printf("#<PACKAGE %.*s>", (int) item->len, item->str);
                }
		else {
                	printf("?%.*s?", (int) item->len, item->str);
		}

                if (item->next) {
                	printf(" ");
                }

        	item = item->next;
	}
}

void ulver_utils_tolower(char *s, uint64_t len) {
	uint64_t i;
	for(i=0;i<len;i++) {
		s[i] = tolower(s[i]);
	}
}

void ulver_utils_toupper(char *s, uint64_t len) {
        uint64_t i;
        for(i=0;i<len;i++) {
                s[i] = toupper(s[i]);
        }
}

int ulver_utils_memicmp(char *s0, char *s1, uint64_t len) {
	uint64_t i;
	for(i=0;i<len;i++) {
		int is0 = tolower(s0[i]);
		int is1 = tolower(s1[i]);
		if (is0 != is1) return -1;
	}

	return 0;
}

int ulver_utils_is_a_number(char *s, uint64_t len) {
	uint64_t i;
	// check the first char
	if (s[0] == '+') goto check;
	if (s[0] == '-') goto check;
	if (s[0] < '0' || s[0] > '9') return 0;
check:
        for(i=1;i<len;i++) {
                if (s[i] < '0' || s[i] > '9') {
			return 0;
                }
        }
	return 1;
}

int ulver_utils_is_a_float(char *s, uint64_t len) {
	uint8_t has_dot = 0;
	uint64_t i;
        // check the first char
        if (s[0] == '+') goto check;
        if (s[0] == '-') goto check;
        if (s[0] == '.') {
		has_dot = 1;
		goto check;
	}
        if (s[0] < '0' || s[0] > '9') return 0;
check:
        for(i=1;i<len;i++) {
		if (s[i] == '.') {
			if (has_dot) return 0;
			has_dot = 1;
			continue;
		}
                if (s[i] < '0' || s[i] > '9') {
                        return 0;
                }
        }

        return 1;
}

char *ulver_utils_strndup(ulver_env *env, char *str, uint64_t len) {
	char *ptr = env->alloc(env, len+1);
	memcpy(ptr, str, len);
	return ptr;
}

char *ulver_utils_strdup(ulver_env *env, char *str) {
	return ulver_utils_strndup(env, str, strlen(str));
}

int ulver_utils_eq(ulver_object *uo1, ulver_object *uo2) {
	// fast initial check
	if (uo1 == uo2) return 1;
	if (uo1->type != uo2->type) return 0;
	switch(uo1->type) {
		case ULVER_NUM:
			if (uo1->n == uo2->n) return 1;
			break;
		case ULVER_FLOAT:
			if (uo1->d == uo2->d) return 1;
			break;
		case ULVER_KEYWORD:
			if (uo1->len != uo2->len) return 0;
			if (!ulver_utils_memicmp(uo1->str, uo2->str, uo1->len)) return 1;
			break;
		default:
			break;
	}
	return 0;
}

ulver_object *ulver_utils_nth(ulver_object *list, uint64_t n) {
	uint64_t count = 0;
	ulver_object *item = list->list;
	while(item) {
		count++;
		if (count == n) {
			return item;
		}	
		item = item->next;
	}
	return NULL;
}

void ulver_utils_print_form(ulver_form *form) {
	if (form->type == ULVER_LIST) {
        	ulver_form *list = form->list;
                printf("(");
		while(list) {
                	ulver_utils_print_form(list);
			list = list->next;
			if (list) printf(" ");
		}
                printf(")");
		return;
	}

        if (form->type == ULVER_STRING) {
        	fprintf(env->stdout, "\"%.*s\"", (int)form->len, form->value);
		return;
        }
        fprintf(env->stdout, "%.*s", (int)form->len, form->value);
}

uint64_t ulver_utils_length(ulver_object *uo) {
	uint64_t count = 0;
	if (uo->type == ULVER_STRING) return uo->len;
	if (uo->type == ULVER_LIST) {
		ulver_object *item = uo->list;
		while(item) {
			count++;
			item = item->next;
		}
	}
	return count;
}

int ulver_utils_endswith(char *str, uint64_t len, char *with) {
	size_t with_len = strlen(with);
	if (with_len > len) return -1;
	char *base = (str + len) - with_len;
	return strncmp(base, with, with_len);
}

uint64_t ulver_utils_library_ext(char *filename) {
	if (!ulver_utils_endswith(filename, strlen(filename), ".so")) return 3;
	if (!ulver_utils_endswith(filename, strlen(filename), ".dylib")) return 6;
	if (!ulver_utils_endswith(filename, strlen(filename), ".dll")) return 4;
	return 0;
}

char *ulver_utils_is_library(ulver_env *env, char *filename) {
	uint64_t with_len = ulver_utils_library_ext(filename);
	if (!with_len) return NULL;

	char *name = filename;
#ifdef __WIN32__
	char *relative_name = strrchr(filename, '\\');
#else
	char *relative_name = strrchr(filename, '/');
#endif
	if (relative_name) name = relative_name+1;
	uint64_t name_len = strlen(name) - with_len;

	char *entry_point = env->alloc(env, name_len + 6);
	memcpy(entry_point, name, name_len);
	memcpy(entry_point + name_len, "_init\0", 6);
	return entry_point;
}
