#include <ulver.h>
#ifndef __WIN32__
#include <readline/readline.h>
#include <readline/history.h>
#endif

void cleanup() {
	printf("\n");
}

int main(int argc, char **argv) {
	atexit(cleanup);

        ulver_env *env = ulver_init();
	env->max_memory = 0;
	env->gc_freq = 100;

	// REPL ?
	if (argc < 2) {
		//
		char buf[8192];
		for(;;) {
			ulver_thread *ut = ulver_current_thread(env);
#ifndef __WIN32__
			char *line = readline("> ");
			if (!line) continue;
			if (strlen(line) > 0)
				add_history(line);
#else
			printf("> ");
			char *line = malloc(8192);
			if (!line) {
				perror("malloc()");
				exit(1);
			}
			if (!fgets(line, 8192, stdin)) {
				free(line);
				continue;
			}		
#endif
                	size_t len = strlen(line);
			if (len == 0) {
				free(line);
				printf("\n");
				continue;
			}

			ulver_form *uf = ulver_parse(env, line, len);		
			free(line);
			if (!uf) {
				ulver_report_error(env);
				continue;
			}
			while(uf) {
				ulver_object *ret = ulver_fun_print(env, uf);
                		if (ret == NULL) {
					ulver_report_error(env);
                		}
				else {
					printf("\n");
				}
				uf = uf->next;
			}
        	}
		ulver_destroy(env);
		exit(0);
	}

	int exit_value = 0;
	ulver_object *ret = ulver_load(env, argv[1]);
	if (ret == NULL) {
		ulver_report_error(env);
		exit_value = 1;
	}
	uint64_t leaked_mem = ulver_destroy(env);
	if (leaked_mem > 0) {
		exit_value = 1;
		printf("\n[BUG] ulver did not release all of its allocated memory: wasted %llu bytes\n", (long long unsigned int) leaked_mem);
	}
	exit(exit_value);
}
