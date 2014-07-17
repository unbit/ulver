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

	// REPL ?
	if (argc < 2) {
		//
		char buf[8192];
		for(;;) {
#ifndef __WIN32__
			char *line = readline("> ");
			if (!line) continue;
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
			ulver_form *uf = ulver_parse(env, line, len);		
			free(line);
			if (!uf) {
				printf("unable to parse expression\n");
				continue;
			}
			ulver_object *ret = ulver_fun_print(env, uf);
                	if (ret == NULL) {
                        	if (env->error) {
                               		printf("\n*** ERROR: %.*s ***\n", env->error_len, env->error);
                        	}
                	}
			else {
				printf("\n");
			}
        	}
		ulver_destroy(env);
		exit(0);
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open()");
		exit(1);
	}
	struct stat st;
	if (fstat(fd, &st)) {
		perror("stat()");
		exit(1);
	}

	char *buf = malloc(st.st_size);
	if (!buf) {
		perror("malloc()");
		exit(1);
	}

	ssize_t rlen = read(fd, buf, st.st_size);
	if (rlen != st.st_size) {
		perror("read()");
		exit(1);
	}

	ulver_form *uf = ulver_parse(env, buf, rlen);
	while(uf) {
		ulver_object *ret = ulver_eval(env, uf);
		if (ret == NULL) {
			if (env->error) {
				printf("\n*** ERROR: %.*s ***\n", env->error_len, env->error);
			}
			exit(1);
		}
		uf = uf->next;
	}
	ulver_destroy(env);
}
