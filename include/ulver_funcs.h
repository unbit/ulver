#include <ulver.h>

ulver_object *ulver_fun_add(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sub(ulver_env *, ulver_form *);
ulver_object *ulver_fun_mul(ulver_env *, ulver_form *);
ulver_object *ulver_fun_mod(ulver_env *, ulver_form *);
ulver_object *ulver_fun_higher(ulver_env *, ulver_form *);
ulver_object *ulver_fun_equal(ulver_env *, ulver_form *);

ulver_object *ulver_fun_write_string(ulver_env *, ulver_form *);
ulver_object *ulver_fun_read_string(ulver_env *, ulver_form *);

ulver_object *ulver_fun_hub(ulver_env *, ulver_form *);

ulver_object *ulver_fun_make_coro(ulver_env *, ulver_form *);
ulver_object *ulver_fun_sleep(ulver_env *, ulver_form *);
ulver_object *ulver_fun_make_tcp_server(ulver_env *, ulver_form *);

ulver_object *ulver_fun_coro_switch(ulver_env *, ulver_form *);
ulver_object *ulver_fun_coro_yield(ulver_env *, ulver_form *);
ulver_object *ulver_fun_coro_next(ulver_env *, ulver_form *);
