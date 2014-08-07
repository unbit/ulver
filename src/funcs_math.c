#include <ulver.h>

ulver_object *ulver_fun_mod(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *uo1 = ulver_eval(env, argv);
        if (!uo1) return NULL;
        int64_t n1 = 0;
        if (uo1->type == ULVER_NUM) n1 = uo1->n;
        else if (uo1->type == ULVER_FLOAT) n1 = uo1->d;
        else return ulver_error_form(env, ULVER_ERR_NOT_NUM, argv, NULL);

        ulver_object *uo2 = ulver_eval(env, argv->next);
        if (!uo2) return NULL;
        int64_t n2 = 0;
        if (uo2->type == ULVER_NUM) n2 = uo2->n;
        else if (uo2->type == ULVER_FLOAT) n2 = uo2->d;
        else return ulver_error_form(env, ULVER_ERR_NOT_NUM, argv->next, NULL);

        if (n2 == 0) return ulver_error(env, ULVER_ERR_FPE);

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
                        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
                }
                argv = argv->next;
        }

        if (is_float) {
                return ulver_object_from_float(env, d+n);
        }
        return ulver_object_from_num(env, n);
}


ulver_object *ulver_fun_sub(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
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
                        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, item, NULL);
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
                return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
        }

        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
                return ulver_object_from_num(env, argv->next ? uo->n - n :  - uo->n);
        }
        else if (uo->type == ULVER_FLOAT) {
                return ulver_object_from_float(env, argv->next ? uo->d - n: - uo->d);
        }
        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
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
                        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
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
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
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
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *uo0 = ulver_eval(env, argv);
        if (!uo0) return NULL;
        ulver_object *uo1 = ulver_eval(env, argv->next);
        if (!uo1) return NULL;
        int64_t n0 = uo0->n;
        int64_t n1 = uo1->n;
        return n0 == n1 ? env->t : env->nil;
}

ulver_object *ulver_fun_sin(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type == ULVER_NUM) {
		return ulver_object_from_float(env, sin(uo->n));
	}
	if (uo->type == ULVER_FLOAT) {
		return ulver_object_from_float(env, sin(uo->d));
	}
	return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
}

ulver_object *ulver_fun_cos(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
                return ulver_object_from_float(env, cos(uo->n));
        }
        if (uo->type == ULVER_FLOAT) {
                return ulver_object_from_float(env, cos(uo->d));
        }
        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
}

ulver_object *ulver_fun_1add(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
                return ulver_object_from_num(env, uo->n + 1);
        }
        if (uo->type == ULVER_FLOAT) {
                return ulver_object_from_float(env, uo->d + 1.0);
        }
        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
}

ulver_object *ulver_fun_1sub(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
                return ulver_object_from_num(env, uo->n - 1);
        }
        if (uo->type == ULVER_FLOAT) {
                return ulver_object_from_float(env, uo->d - 1.0);
        }
        return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
}

ulver_object *ulver_fun_max(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *uo = ulver_eval(env, argv);
	if (!uo) return NULL;
	if (uo->type != ULVER_NUM && uo->type != ULVER_FLOAT) return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
	double winner = uo->d;
	if (uo->type == ULVER_NUM) winner = uo->n;
	ulver_object *winner_o = uo;
	argv = argv->next;
	while(argv) {
		uo = ulver_eval(env, argv);
		if (!uo) return NULL;
		if (uo->type != ULVER_NUM && uo->type != ULVER_FLOAT) return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
		if (uo->type == ULVER_NUM) {
			if (uo->n > winner) {
				winner = uo->n;
				winner_o = uo;
			}
		}
		// float ?
		else {
			if (uo->d > winner) {
                                winner = uo->d;
                                winner_o = uo;
                        }
		}
		argv = argv->next;
	}
	return winner_o;
}

ulver_object *ulver_fun_min(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type != ULVER_NUM && uo->type != ULVER_FLOAT) return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
        double winner = uo->d;
        if (uo->type == ULVER_NUM) winner = uo->n;
        ulver_object *winner_o = uo;
        argv = argv->next;
        while(argv) {
                uo = ulver_eval(env, argv);
                if (!uo) return NULL;
                if (uo->type != ULVER_NUM && uo->type != ULVER_FLOAT) return ulver_error_form(env, ULVER_ERR_NOT_NUMFLOAT, argv, NULL);
                if (uo->type == ULVER_NUM) {
                        if (uo->n < winner) {
                                winner = uo->n;
                                winner_o = uo;
                        }
                }
                // float ?
                else {
                        if (uo->d < winner) {
                                winner = uo->d;
                                winner_o = uo;
                        }
                }
                argv = argv->next;
        }
        return winner_o;
}

