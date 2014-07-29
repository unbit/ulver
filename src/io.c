#include <ulver.h>

static void switch_to_hub(ulver_env *env, ulver_thread *ut) {
        ulver_coro *current_coro = ut->current_coro;
        ut->current_coro = ut->hub;
        if (current_coro != ut->main_coro) {
                __splitstack_getcontext(current_coro->ss_contexts);
        }
        __splitstack_setcontext(ut->hub->ss_contexts);
        swapcontext(&current_coro->context, &ut->hub->context);
        ut->current_coro = current_coro;
}

ulver_object *ulver_fun_sleep(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "sleep requires an argument");
        ulver_thread *ut = ulver_current_thread(env);
        ulver_object *uo = ulver_eval(env, argv);
        if (uo->type == ULVER_NUM) {
                // ensure the hub is running 
                ulver_hub(env);
                //pthread_rwlock_unlock(&env->unsafe_lock);
                uv_timer_t *timer = env->alloc(env, sizeof(uv_timer_t));
                uv_timer_init(ut->hub_loop, timer);
                timer->data = ut->current_coro;
                uv_timer_start(timer, ulver_timer_switch_cb, uo->n * 1000, 0);
                ut->current_coro->blocked = 1;
                switch_to_hub(env, ut);
                //pthread_rwlock_rdlock(&env->unsafe_lock);
        }
        else if (uo->type == ULVER_FLOAT) {
                pthread_rwlock_unlock(&env->unsafe_lock);
                usleep(uo->d * (1000*1000));
                pthread_rwlock_rdlock(&env->unsafe_lock);
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
        switch_to_hub(env, ut);
        return env->nil;
}

