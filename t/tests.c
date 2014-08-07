#include <ulver.h>

ulver_env *env = NULL;
uint64_t tests_successfull = 0;
uint64_t tests_failed = 0;

void tests();

void test_error(char *s, uint8_t code) {
	//printf("- running test_error for \"%s\", expect %d\n", s, code);
	ulver_object *ret = ulver_run(env, s);
	if (ret) {
		printf("[FAILED] test for %s: did not returned an error\n", s);
                tests_failed++;
                return;
	}
	uint8_t error_code = ulver_error_code(env);
	if (error_code != code) {
		printf("[FAILED] test for %s: error code %d is not %d\n", s, error_code, code);
                tests_failed++;
                return;
	}
	tests_successfull++;
}

void test_float(char *s, double d) {
	//printf("- running test_float for \"%s\", expect %f\n", s, d);
	ulver_object *ret = ulver_run(env, s);
	if (!ret) {
		ulver_report_error(env);
		tests_failed++;
		return;
	}

	if (ret->type != ULVER_FLOAT) {
		printf("[FAILED] test for %s: object is not a float\n", s);
		tests_failed++;
                return;
	}

	if (ret->d != d) {
		printf("[FAILED] test for %s: %f is not %f\n", s, ret->d, d);
                tests_failed++;
                return;
	}

	tests_successfull++;
}

void test_num(char *s, int64_t n) {
        //printf("- running test_num for \"%s\", expect %lld\n", s, (long long int) n);
        ulver_object *ret = ulver_run(env, s);
        if (!ret) {
                ulver_report_error(env);
                tests_failed++;
                return;
        }

        if (ret->type != ULVER_NUM) {
                printf("[FAILED] test for %s: object is not a number\n", s);
                tests_failed++;
                return;
        }

        if (ret->n != n) {
                printf("[FAILED] test for %s: %lld is not %lld\n", s, (long long int) ret->n, (long long int) n);
                tests_failed++;
                return;
        }

        tests_successfull++;
}

void test_two_nums(char *s, int64_t n, int64_t n2) {
        //printf("- running test_two_nums for \"%s\", expect %lld and %lld\n", s, (long long int) n, (long long int) n2);
        ulver_object *ret = ulver_run(env, s);
        if (!ret) {
                ulver_report_error(env);
                tests_failed++;
                return;
        }

	if (ret->type != ULVER_MULTIVALUE) {
		printf("[FAILED] test for %s: did not return a multivalue object\n", s);
                tests_failed++;
                return;
	}

	if (!ret->list || !ret->list->next) {
		printf("[FAILED] test for %s: did not returned 2 values\n", s);
		tests_failed++;
                return;
	}

        if (ret->list->o->type != ULVER_NUM) {
                printf("[FAILED] test for %s: object is not a number\n", s);
                tests_failed++;
                return;
        }

        if (ret->list->o->n != n) {
                printf("[FAILED] test for %s: %lld is not %lld\n", s, (long long int) ret->list->o->n, (long long int) n);
                tests_failed++;
                return;
        }

	if (ret->list->next->o->type != ULVER_NUM) {
                printf("[FAILED] test for %s: second object is not a number\n", s);
                tests_failed++;
                return;
        }

        if (ret->list->next->o->n != n2) {
                printf("[FAILED] test for %s: %lld is not %lld\n", s, (long long int) ret->list->next->o->n, (long long int) n2);
                tests_failed++;
                return;
        }

        tests_successfull++;
}

void test_true(char *s) {
        //printf("- running test_true for \"%s\"\n", s);
        ulver_object *ret = ulver_run(env, s);
        if (!ret) {
                ulver_report_error(env);
                tests_failed++;
                return;
        }

        if (ret->type != ULVER_TRUE) {
                printf("[FAILED] test for %s: object is not true\n", s);
                tests_failed++;
                return;
        }

        tests_successfull++;
}

void test_string(char *s, char *str) {
        //printf("- running test_string for \"%s\"\n", s);
        ulver_object *ret = ulver_run(env, s);
        if (!ret) {
                ulver_report_error(env);
                tests_failed++;
                return;
        }

        if (ret->type != ULVER_STRING) {
                printf("[FAILED] test for %s: object is not a string\n", s);
                tests_failed++;
                return;
        }

	if (strcmp(ret->str, str)) {
                printf("[FAILED] test for %s: \"%s\" is not \"%s\"\n", s, ret->str, str);
                tests_failed++;
                return;
	}

        tests_successfull++;
}

void test_nil(char *s) {
        //printf("- running test_nil for \"%s\"\n", s);
        ulver_object *ret = ulver_run(env, s);
        if (!ret) {
                ulver_report_error(env);
                tests_failed++;
                return;
        }

        if (ret->type != ULVER_LIST || ret->list) {
                printf("[FAILED] test for %s: object is not nil\n", s);
                tests_failed++;
                return;
        }

        tests_successfull++;
}

int main(int argc, char **argv) {
	printf("*** TESTING ulver ***\n\n");
	env = ulver_init();
	env->max_memory = 0;
	uint64_t i;
	for(i=0;i<100;i++) {
		env->gc_freq = i;
		printf("running tests with gc frequency %llu\n", (unsigned long long) i);
		tests();
	}

	printf("\n*** END OF TESTS ***\n");
	printf("SUCCESSFULL TESTS: %llu\n", (unsigned long long) tests_successfull);
	printf("FAILED TESTS: %llu\n", (unsigned long long) tests_failed);

	printf("CALLED FUNCS: %llu\n", (unsigned long long) env->calls);
	printf("GC ROUNDS: %llu\n", (unsigned long long) env->gc_rounds);
	uint64_t mem = ulver_destroy(env);
	printf("LEAKED MEMORY: %llu\n", (unsigned long long) mem);

	exit(tests_failed);
}

void tests() {
	test_num("(+ 1 1)", 2);
	test_num("(+ 1 2 3 (* 2 2))", 10);
	test_num("(+ 1 (parse-integer \"172\" ))", 173);
	test_num("(* 2 2)", 4);
	test_num("(* 2 0)", 0);
	test_num("(- 1)", -1);
	test_num("(- 1 3)", -2);
	test_num("(- 1 -2)", 3);

	test_float("(+ 1.0 2)", 3.0);
	test_float("(- 1 0.1)", 0.9);
	test_float("(* 1.0 2 3)", 6.0);
	test_float("(* 2.0)", 2.0);

	test_true("(eq 1 1)");
	test_true("(eq (+ 1 2) 3)");
	test_true("(eq :test :test)");
	test_true("(atom :hello)");

	test_num("(first (list 1 2 3 4 5 6 7 8 9 10 11))", 1);
	test_num("(second (list 1 2 3 4 5 6 7 8 9 10 11))", 2);
	test_num("(third (list 1 2 3 4 5 6 7 8 9 10 11))", 3);
	test_num("(fourth (list 1 2 3 4 5 6 7 8 9 10 11))", 4);
	test_num("(fifth (list 1 2 3 4 5 6 7 8 9 10 11))", 5);
	test_num("(sixth (list 1 2 3 4 5 6 7 8 9 10 11))", 6);
	test_num("(seventh (list 1 2 3 4 5 6 7 8 9 10 11))", 7);
	test_num("(eighth (list 1 2 3 4 5 6 7 8 9 10 11))", 8);
	test_num("(ninth (list 1 2 3 4 5 6 7 8 9 10 11))", 9);
	test_num("(tenth (list 1 2 3 4 5 6 7 8 9 10 11))", 10);

	test_true("(setq highnumber 300)(if (> highnumber 299) (eq :a :a))");

	test_num("(setq themap (list :foo 17 :bar 30 :fake))(getf themap :bar)", 30);

	test_num("(eval (second (quote (1 (+ 2 1)))))", 3);
	test_num("(+ 1 (eval (quote 1) ))", 2);

	test_num("(setq callme (lambda (a) (* a 2)))(funcall callme 1)", 2);

	test_true("(find t (list 1 2 \"3\" :foo :bar t 0 0))");

	test_num("(position :b (eval (quote (list :a :b (list :c :d :e) (list :c :d :e (list :a :b :c))))))", 1);

	test_num("(eval (read-from-string \"(+ 3 4 10)\"))", 17);


	test_true("(defpackage :ulverpackage (:export :hello))(in-package :ulverpackage)(defun hello (x) (if (> x 2)(eval (quote t))))(in-package :cl-user)(ulverpackage:hello 4)");
	test_nil("(defpackage :ulverpackage (:export :hello))(in-package :ulverpackage)(defun hello (x) (if (> x 5)(eval (quote t))))(in-package :cl-user)(ulverpackage:hello 4)");

	test_string("\"hello\"", "hello");
	test_string("(subseq \"hello\" 0)", "hello");
	test_string("(subseq \"hello\" 1)", "ello");
	test_string("(subseq \"hello\" 3)", "lo");
	test_string("(subseq \"hello\" 0 0)", "");
	test_string("(subseq \"hello\" 1 2)", "e");
	test_string("(subseq \"hello\" 1 3)", "el");

	test_num("(third (subseq (list 1 2 3 4 5 6 7) 1 5))", 4);
	test_num("(length (list 1 2 3 (third (subseq (list 1 2 3 4 5 6 7) 1 5))) )", 4);
	test_num("(length (subseq (list 1 2 3) 0 0))", 0);
	test_num("(length (subseq \"hello\" 0 3))", 3);

	test_num("(defun additioner (x) \"i am the doc for additioner\" (+ x x))(additioner 15)", 30);

	test_two_nums("(values 4 5)", 4, 5);
	test_two_nums("(values 4 (+ 1 1 1 1 1))", 4, 5);
	test_two_nums("(values (+ 1 2)(+ 3 4))", 3, 7);

	test_two_nums("(let ((a 3) (b 10)) (values (+ a b 17) (- (* b 2) 3)))", 30, 17);

	test_num("(setq hellonum 30)(let ((hellonum 2)) (+ hellonum 15))", 17);
	test_num("(setq hellonum 17)(let ((hellonum 0)) (setq hellonum 30))", 30);
	test_num("(setq hellonum 17)(let ((hellonum 0)) (setq hellonum 30)) hellonum", 17);

	test_num("(let ((z 1)) (loop (if (= z 30) (return z)) (setq z (+ z 1) ) ) )", 30);

	test_num("(setq coro1 (make-coro () (progn (coro-yield 1)(coro-yield 2)(coro-yield 3)(coro-yield 4)(coro-yield 5))))(+ 2 (coro-next coro1)(coro-next coro1)(coro-next coro1)(coro-next coro1)(coro-next coro1))", 17);
	test_num("(coro-next (make-coro () 17))", 17);
	test_num("(setq coro1 (make-coro () (progn (coro-yield 1)(coro-yield 2)(coro-yield 3))))"
		"(setq coro2 (make-coro () (progn (coro-yield 2)(coro-yield 3)(coro-yield 4))))"
		"(setq coro3 (make-coro () (progn (coro-yield 5)(coro-yield 6)(coro-yield 7))))"
		"(+ (coro-next coro1)(coro-next coro2)(coro-next coro3)(coro-next coro1)(coro-next coro2)(coro-next coro3) -2)", 17);

	test_string("(string-upcase \"hello\")", "HELLO");
	test_string("(string-downcase \"hEllO\")", "hello");

	test_string("(join-thread (make-thread (string-upcase \"hello\")))", "HELLO");
	test_string("(join-thread (make-thread (join-thread (make-thread (string-upcase \"HellO\")))))", "HELLO");

	test_num("(+ 2 (join-thread (make-thread (+ 5 1))) (join-thread (make-thread (* 4 2))) 1)", 17);
	test_num("(join-thread (make-thread (join-thread (make-thread (join-thread (make-thread 17))))))", 17);

	test_float("(sin 1)", sin(1));
	test_float("(sin 1.0)", sin(1.0));

	test_float("(cos 3.2)", cos(3.2));
	test_float("(cos 5)", cos(5));

	test_num("(setq *sharedht* (make-hash-table))(join-thread (make-thread (sethash *sharedht* :num1 17)))(gethash *sharedht* :num1)", 17);

	test_error("(1", ULVER_ERR_PARSE);

	test_error("(+ 1 :foo)", ULVER_ERR_NOT_NUMFLOAT);

	test_error("(join-thread (make-thread (mod 1 0)))", ULVER_ERR_FPE);

	test_num("(first (last (union (list 1 2 3) (list 4 5 6))))", 6);

	test_string("(second (string-split (first (string-split \"a=b&f=c&pippo=pluto&s=hello\" \"&\")) \"=\"))", "b");

	test_num("(1+ 2)", 3);

}
