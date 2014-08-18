#include <ulver.h>

/*

	- error management -

	each ulver environment has 256 slot for known errors.

	Each slot is mapped to a specific message string and flags.

	If the "fatal" flag is on (1) for an error, the engine will call
	exit(1) when the error is raised.

	if the "to_nil" flag is on (1) for an error, the engine will
        return nil when the error is raised. This can change the whole behaviour
	of your code (pay attention)

	if the "_errno" flag is on (1) for an error, the engine will
        set the error message to the output of strerror(errno)

	Embedders can override errors behaviours (this is why flags are here)

	Whenever an eval returns NULL, an error occurred and the
	coro generating it is considered dead.

	The error information is per-thread.	

*/

/*
ulver_object *ulver_error_form(ulver_env *env, ulver_form *uf, char *msg) {
        return ulver_error(env, "[line: %llu pos: %lld] (%.*s) %s", uf->line, uf->line_pos, (int) uf->len, uf->value, msg);
}
*/

/*
	ulver_error(environment, err_code, format_string)
*/
ulver_object *ulver_error_form(ulver_env *env, uint8_t type, ulver_form *uf, char *msg) {
        ulver_thread *ut = ulver_current_thread(env);
        // when trying to set an error, check if a previous one is set
	// and eventually clear it

	ut->err_code = ULVER_ERR_NOERROR;
        ut->error = NULL;
        ut->error_len = 0;
	ut->error_form = NULL;
	ut->error_fun = NULL;

	// could be a simple clean procedure ...
	if (type == ULVER_ERR_NOERROR)
               	return NULL;

	if (uf) {
		ut->error_form = uf;
	}
	else if (ut->current_coro->stack->argv) {
		ut->error_form = ut->current_coro->stack->argv;
	}

	if (ut->current_coro->stack->caller) {
		ut->error_fun = ut->current_coro->stack->caller->str;
	}

	ut->err_code = type;
	if (env->err_table[type]._errno) {
		ut->error = strerror(errno);
		ut->error_len = strlen(ut->error);
	}
	else {
		ut->error = env->err_table[type].string;
		ut->error_len = env->err_table[type].string_len;
	}
	return NULL;
}

ulver_object *ulver_error(ulver_env *env, uint8_t type) {
	return ulver_error_form(env, type, NULL, NULL);
}

void ulver_report_error(ulver_env *env) {
        ulver_thread *ut = ulver_current_thread(env);
        if (ut->error) {
		if (ut->error_form) {
                	fprintf(env->_stderr, "*** ERROR %d: [file: \"%s\" line: %llu pos: %llu fun: #'%s] (%s%.*s%s): %.*s ***\n",
				ut->err_code,
				(ut->error_form->source && ut->error_form->source->filename) ? ut->error_form->source->filename : "-",
				(unsigned long long) ut->error_form->line,	
				(unsigned long long) ut->error_form->line_pos,	
				ut->error_fun ? ut->error_fun : "",
				ut->error_form->type == ULVER_STRING ? "\"" : "",
				(int) ut->error_form->len,
				ut->error_form->value,
				ut->error_form->type == ULVER_STRING ? "\"" : "",
				(int) ut->error_len,
				ut->error);
		}
		else {
                	fprintf(env->_stderr, "*** ERROR %d: %.*s ***\n",
				ut->err_code,
				(int) ut->error_len,
				ut->error);
		}
                // clear error
                ulver_error(env, ULVER_ERR_NOERROR);
        }
}

void ulver_err_table_fill(ulver_env *env) {
	env->err_table[ULVER_ERR_NOERROR].string = "" ;
	env->err_table[ULVER_ERR_ONE_ARG].string = "requires an argument" ;
	env->err_table[ULVER_ERR_TWO_ARG].string = "requires two arguments" ;
	env->err_table[ULVER_ERR_THREE_ARG].string = "requires three arguments" ;
	env->err_table[ULVER_ERR_NOT_NUM].string = "is not a number" ;
	env->err_table[ULVER_ERR_NOT_FLOAT].string = "is not a float" ;
	env->err_table[ULVER_ERR_NOT_STRING].string = "is not a string" ;
	env->err_table[ULVER_ERR_NOT_KEYWORD].string = "is not a keyword" ;
	env->err_table[ULVER_ERR_NOT_NUMFLOAT].string = "is not a number or a float" ;
	env->err_table[ULVER_ERR_FPE].string = "division by zero !" ;
	env->err_table[ULVER_ERR_PKG_NOTFOUND].string = "package not found" ;
	env->err_table[ULVER_ERR_PKG].string = "package error" ;
	env->err_table[ULVER_ERR_THREAD].string = "thread creation error" ;
	env->err_table[ULVER_ERR_THREAD]._errno = 1;
	env->err_table[ULVER_ERR_NOT_THREAD].string = "is not a thread" ;
	env->err_table[ULVER_ERR_NOT_CORO].string = "is not a coro" ;
	env->err_table[ULVER_ERR_NOT_STREAM].string = "is not a stream" ;
	env->err_table[ULVER_ERR_ZERO].string = "is not >= 0" ;
	env->err_table[ULVER_ERR_NOT_SEQ].string = "is not a sequence" ;
	env->err_table[ULVER_ERR_RANGE].string = "out of range" ;
	env->err_table[ULVER_ERR_NOT_LIST].string = "is not a list" ;
	env->err_table[ULVER_ERR_ODD].string = "is odd" ;
	env->err_table[ULVER_ERR_NOT_SYMBOL].string = "is not a symbol" ;
	env->err_table[ULVER_ERR_PARSE].string = "parse error" ;
	env->err_table[ULVER_ERR_NOT_FUNC].string = "is not a function" ;
	env->err_table[ULVER_ERR_UNK_FUNC].string = "unknown function" ;
	env->err_table[ULVER_ERR_NOT_FORM].string = "is not a form" ;

	env->err_table[ULVER_ERR_IO].string = "I/O error" ;
	env->err_table[ULVER_ERR_IO]._errno = 1;

	env->err_table[ULVER_ERR_NOT_KEYSTRING].string = "is not a string or a keyword" ;
	env->err_table[ULVER_ERR_LAMBDA].string = "lambda error" ;
	env->err_table[ULVER_ERR_CROSS_THREAD].string = "cross thread violation !" ;
	env->err_table[ULVER_ERR_CORO_DEAD].string = "coro is dead !" ;
	env->err_table[ULVER_ERR_CORO_BLOCKED].string = "coro is blocked !" ;
	env->err_table[ULVER_ERR_UNBOUND].string = "unbound symbol" ;
	env->err_table[ULVER_ERR_NOT_HASHTABLE].string = "is not a hashtable" ;
	env->err_table[ULVER_ERR_FOUR_ARG].string = "require four arguments" ;
	env->err_table[ULVER_ERR_NEGATIVE].string = "is not a positive number" ;
	env->err_table[ULVER_ERR_NOT_CHANNEL].string = "is not a channel" ;
	env->err_table[ULVER_ERR_CUSTOM].string = "custom error" ;

	// fix string length
	// use int to avoid overflow !!!
	int i;
	for(i=ULVER_ERR_NOERROR;i<=ULVER_ERR_CUSTOM;i++) {
		if (env->err_table[i].string) {
			env->err_table[i].string_len = strlen(env->err_table[i].string);
		}
	}
}

uint8_t ulver_error_code(ulver_env *env) {
	ulver_thread *ut = ulver_current_thread(env);
	uint8_t err_code = ut->err_code;	
	// clear error
        ulver_error(env, ULVER_ERR_NOERROR);
	return err_code;
}
