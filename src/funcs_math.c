#include <ulver.h>

ulver_object *ulver_fun_mod(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, "mod requires two arguments");
        ulver_object *uo1 = ulver_eval(env, argv);
        if (!uo1) return NULL;
        int64_t n1 = 0;
        if (uo1->type == ULVER_NUM) n1 = uo1->n;
        else if (uo1->type == ULVER_FLOAT) n1 = uo1->d;
        else return ulver_error_form(env, argv, "is not a number");

        ulver_object *uo2 = ulver_eval(env, argv->next);
        if (!uo2) return NULL;
        int64_t n2 = 0;
        if (uo2->type == ULVER_NUM) n2 = uo2->n;
        else if (uo2->type == ULVER_FLOAT) n2 = uo2->d;
        else return ulver_error_form(env, argv->next, "is not a number");

        if (n2 == 0) return ulver_error(env, "division by zero");

        return ulver_object_from_num(env, n1 % n2);
}

ulver_object *ulver_fun_add(ulver_env *env, ulver_form *argv) {
        int64_t n = 0;
        double d = 0.0;
        uint8_t is_float = 0;
        while(argv) {
                ulver_object *uo = ulver_eval(env, argv);
                if (!uo) return NULL;
                if (uo->type == ULVER_NUM) {
                        n += uo->n;
                }
                else if (uo->type == ULVER_FLOAT) {
                        is_float = 1;
                        d += uo->d;
                }
                else {
                        return ulver_error_form(env, argv, "argument is not a number or a float");
                }
                argv = argv->next;
        }

        if (is_float) {
                return ulver_object_from_float(env, d+n);
        }
        return ulver_object_from_num(env, n);
}


ulver_object *ulver_fun_sub(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "- requires an argument");
        int64_t n = 0;
        double d = 0.0;
        uint8_t is_float = 0;
        ulver_form *item = argv->next;
        while(item) {
                ulver_object *uo = ulver_eval(env, item);
                if (!uo) return NULL;
                if (uo->type == ULVER_NUM) {
                        n += uo->n;
                }
                else if (uo->type == ULVER_FLOAT) {
                        is_float = 1;
                        d += uo->d;
                }
                else {
                        return ulver_error_form(env, item, "argument is not a number or a float");
                }
                item = item->next;
        }

        if (is_float) {
                ulver_object *uo = ulver_eval(env, argv);
                if (!uo) return NULL;
                if (uo->type == ULVER_NUM) {
                        return ulver_object_from_float(env, uo->n - (d+n));
                }
                else if (uo->type == ULVER_FLOAT) {
                        return ulver_object_from_float(env, uo->d - (d+n));
                }
                return ulver_error_form(env, argv, "argument is not a number or a float");
        }

        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
                return ulver_object_from_num(env, argv->next ? uo->n - n :  - uo->n);
        }
        else if (uo->type == ULVER_FLOAT) {
                return ulver_object_from_float(env, argv->next ? uo->d - n: - uo->d);
        }
        return ulver_error_form(env, argv, "argument is not a number or a float");
}


ulver_object *ulver_fun_mul(ulver_env *env, ulver_form *argv) {
        int64_t n = 1;
        double d = 1.0;
        uint8_t is_float = 0;
        while(argv) {
                ulver_object *uo = ulver_eval(env, argv);
                if (!uo) return NULL;
                if (uo->type == ULVER_NUM) {
                        n *= uo->n;
                }
                else if (uo->type == ULVER_FLOAT) {
                        is_float = 1;
                        d *= uo->d;
                }
                else {
                        return ulver_error_form(env, argv, "argument is not a number or a float");
                }
                argv = argv->next;
        }

        if (is_float) {
                return ulver_object_from_float(env, d*n);
        }
        return ulver_object_from_num(env, n);
}

// TODO fix it
ulver_object *ulver_fun_higher(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "higher requires an argument");
        ulver_object *uo0 = ulver_eval(env, argv);
        if (!uo0) return NULL;
        ulver_object *uo1 = ulver_eval(env, argv->next);
        if (!uo1) return NULL;
        int64_t n0 = uo0->n;
        int64_t n1 = uo1->n;
        return n0 > n1 ? env->t : env->nil;
}

// TODO fix it
ulver_object *ulver_fun_equal(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "higher requires an argument");
        ulver_object *uo0 = ulver_eval(env, argv);
        if (!uo0) return NULL;
        ulver_object *uo1 = ulver_eval(env, argv->next);
        if (!uo1) return NULL;
        int64_t n0 = uo0->n;
        int64_t n1 = uo1->n;
        return n0 == n1 ? env->t : env->nil;
}

ulver_object *ulver_fun_sin(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, "sin requires an argument");
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type == ULVER_NUM) {
		return ulver_object_from_float(env, sin(uo->n));
	}
	if (uo->type == ULVER_FLOAT) {
		return ulver_object_from_float(env, sin(uo->d));
	}
	if (!argv) return ulver_error_form(env, argv, "is not a number");
}

ulver_object *ulver_fun_cos(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "sin requires an argument");
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
                return ulver_object_from_float(env, cos(uo->n));
        }
        if (uo->type == ULVER_FLOAT) {
                return ulver_object_from_float(env, cos(uo->d));
        }
        if (!argv) return ulver_error_form(env, argv, "is not a number");
}

