#include <ulver.h>

ulver_object *ulver_fun_first(ulver_env *, ulver_form *);
ulver_object *ulver_fun_second(ulver_env *, ulver_form *);
ulver_object *ulver_fun_third(ulver_env *, ulver_form *);
ulver_object *ulver_fun_fourth(ulver_env *, ulver_form *);
ulver_object *ulver_fun_fifth(ulver_env *, ulver_form *);
ulver_object *ulver_fun_first(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sixth(ulver_env *, ulver_form *);
ulver_object *ulver_fun_seventh(ulver_env *, ulver_form *);
ulver_object *ulver_fun_eighth(ulver_env *, ulver_form *);
ulver_object *ulver_fun_ninth(ulver_env *, ulver_form *);
ulver_object *ulver_fun_tenth(ulver_env *, ulver_form *);

ulver_object *ulver_fun_subseq(ulver_env *, ulver_form *);

ulver_object *ulver_fun_read_from_string(ulver_env *, ulver_form *);

ulver_object *ulver_fun_string_downcase(ulver_env *, ulver_form *);
ulver_object *ulver_fun_string_upcase(ulver_env *, ulver_form *);

ulver_object *ulver_fun_length(ulver_env *, ulver_form *);

ulver_object *ulver_fun_setf(ulver_env *, ulver_form *);
ulver_object *ulver_fun_getf(ulver_env *, ulver_form *);

ulver_object *ulver_fun_open(ulver_env *, ulver_form *);

ulver_object *ulver_fun_make_hash_table(ulver_env *, ulver_form *);
ulver_object *ulver_fun_gethash(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sethash(ulver_env *, ulver_form *);

ulver_object *ulver_fun_add(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sub(ulver_env *, ulver_form *);
ulver_object *ulver_fun_mul(ulver_env *, ulver_form *);
ulver_object *ulver_fun_mod(ulver_env *, ulver_form *);
ulver_object *ulver_fun_higher(ulver_env *, ulver_form *);
ulver_object *ulver_fun_equal(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sin(ulver_env *, ulver_form *);
ulver_object *ulver_fun_cos(ulver_env *, ulver_form *);

ulver_object *ulver_fun_write_string(ulver_env *, ulver_form *);
ulver_object *ulver_fun_read_string(ulver_env *, ulver_form *);

ulver_object *ulver_fun_hub(ulver_env *, ulver_form *);

ulver_object *ulver_fun_make_coro(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sleep(ulver_env *, ulver_form *);
ulver_object *ulver_fun_make_tcp_server(ulver_env *, ulver_form *);

ulver_object *ulver_fun_coro_switch(ulver_env *, ulver_form *);
ulver_object *ulver_fun_coro_yield(ulver_env *, ulver_form *);
ulver_object *ulver_fun_coro_next(ulver_env *, ulver_form *);

ulver_object *ulver_fun_make_thread(ulver_env *, ulver_form *);
ulver_object *ulver_fun_join_thread(ulver_env *, ulver_form *);
ulver_object *ulver_fun_all_threads(ulver_env *, ulver_form *);
