#include <ulver.h>

ulver_env *env = NULL;
uint64_t tests_successfull = 0;
uint64_t tests_failed = 0;

void tests();

void test_float(char *s, double d) {
	printf("- running test_float for \"%s\", expect %f\n", s, d);
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
        printf("- running test_num for \"%s\", expect %lld\n", s, (long long int) n);
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
        printf("- running test_two_nums for \"%s\", expect %lld and %lld\n", s, (long long int) n, (long long int) n2);
        ulver_object *ret = ulver_run(env, s);
        if (!ret) {
                ulver_report_error(env);
                tests_failed++;
                return;
        }

	if (!ret->ret_next) {
		printf("[FAILED] test for %s: did not returned 2 values\n", s);
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

	if (ret->ret_next->type != ULVER_NUM) {
                printf("[FAILED] test for %s: second object is not a number\n", s);
                tests_failed++;
                return;
        }

        if (ret->ret_next->n != n2) {
                printf("[FAILED] test for %s: %lld is not %lld\n", s, (long long int) ret->n, (long long int) n2);
                tests_failed++;
                return;
        }

        tests_successfull++;
}

void test_true(char *s) {
        printf("- running test_true for \"%s\"\n", s);
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
        printf("- running test_string for \"%s\"\n", s);
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
        printf("- running test_nil for \"%s\"\n", s);
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
	// tune gc frequency to 10
	env->gc_freq = 1;
	env->max_memory = 0;

	tests();

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
	test_true("(eq :test :test");
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

	test_two_nums("(values (+ 1 2)(+ 3 4))", 3, 7);

}
