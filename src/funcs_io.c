#include <ulver.h>

typedef struct ulver_uv_timer ulver_uv_timer;
struct ulver_uv_timer {
	uv_timer_t t;
	ulver_coro *coro;
	ulver_thread *ut;
	ulver_env *env;
};

static void free_uv_timer(uv_handle_t* handle) {
	ulver_uv_timer *uvt = (ulver_uv_timer *) handle->data;
	ulver_env *env = uvt->env;
	env->free(env, uvt, sizeof(ulver_uv_timer));
} 

static void ulver_timer_switch_cb(uv_timer_t* handle, int status) {
        ulver_uv_timer *uvt = (ulver_uv_timer *) handle->data;
	ulver_env *env = uvt->env;
        ulver_hub_schedule_waiters(env, uvt->ut, uvt->coro);
        uvt->coro->blocked = 0;
        // back to coro
        ulver_coro_switch(env, uvt->coro);
        uv_timer_stop(&uvt->t);
	// schedule memory free
	uv_close((uv_handle_t *) &uvt->t, free_uv_timer);
}

void ulver_uv_new_timer(ulver_env *env, uint64_t timeout, uint64_t repeat) {
        // ensure the hub is running 
        ulver_hub(env);
	ulver_thread *ut = ulver_current_thread(env);
	ulver_uv_timer *uvt = env->alloc(env, sizeof(ulver_uv_timer));
	uvt->coro = ut->current_coro;
	uvt->ut = ut;
	uvt->env = env;
	ut->current_coro->blocked = 1;
	uv_timer_init(ut->hub_loop, &uvt->t);
	uvt->t.data = uvt;
	uv_timer_start(&uvt->t, ulver_timer_switch_cb, timeout, repeat);
	ulver_coro_switch(env, ut->hub);	
}

ulver_object *ulver_fun_sleep(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "sleep requires an argument");
        ulver_thread *ut = ulver_current_thread(env);
        ulver_object *uo = ulver_eval(env, argv);
        if (uo->type == ULVER_NUM) {
		ulver_uv_new_timer(env, uo->n * 1000, 0);
        }
        else if (uo->type == ULVER_FLOAT) {
		ulver_uv_new_timer(env, uo->d * 1000, 0);
        }
        return env->nil;

}

static void tcp_server_on_connect(uv_stream_t* server_handle, int status) {
        ulver_object *o_server = (ulver_object *) server_handle->data;
        ulver_coro *coro = (ulver_coro *) o_server->data;

        ulver_thread *ut = ulver_current_thread(o_server->env);

        printf("ut = %p coro = %p env = %p\n", ut, coro, o_server->env);


        ut->current_coro = coro;
        printf("ut->current_coro = %p\n", ut->current_coro->env);
        ulver_object *client_stream = ulver_object_new(o_server->env, ULVER_STREAM);
        uv_tcp_init(ut->hub_loop, &client_stream->stream);
        printf("tcp initialized\n");
        uv_accept(server_handle, (uv_stream_t *) &client_stream->stream);

        ulver_hub_schedule_coro(coro->env, coro);
        coro->blocked = 0;

        printf("ok\n");
}

ulver_object *ulver_fun_make_tcp_server(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next || !argv->next->next) ulver_error(env, "make-tcp-server requires three arguments");
        ulver_object *addr = ulver_eval(env, argv);
        if (!addr) return NULL;
        if (addr->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");

        ulver_object *port = ulver_eval(env, argv->next);
        if (!port) return NULL;
        if (port->type != ULVER_NUM) return ulver_error_form(env, argv->next, "is not a string");

        ulver_object *on_connect = ulver_eval(env, argv->next->next);
        if (!on_connect) return NULL;
        if (on_connect->type != ULVER_CORO) return ulver_error_form(env, argv->next->next, "is not a coro");

        // ensure the hub is running
        ulver_hub(env);

        ulver_thread *ut = ulver_current_thread(env);

        ulver_object *o_server = ulver_object_new(env, ULVER_STREAM);
        o_server->data = on_connect;

        uv_tcp_init(ut->hub_loop, &o_server->stream);
        printf("on_connect->env = %p coro = %p\n", env, on_connect);
        o_server->stream.data = o_server;

        struct sockaddr_in address = uv_ip4_addr(addr->str, port->n);
        uv_tcp_bind(&o_server->stream, address);
        uv_listen((uv_stream_t *) &o_server->stream, 128, tcp_server_on_connect);
        ut->current_coro->blocked = 1;
        //switch_to_hub(env, ut);
	ulver_coro_switch(env, ut->hub);
        return env->nil;
}

