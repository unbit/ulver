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
                else if (item->type == ULVER_NUM) {
                	printf("%lld", item->n);
                }
                else if (item->type == ULVER_FLOAT) {
                	printf("%f", item->d);
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
