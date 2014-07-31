#include <ulver.h>

ulver_object *ulver_fun_open(ulver_env *env, ulver_form *argv) {
        if (!argv) return ulver_error(env, "open requires an argument");
        ulver_object *uo = ulver_eval(env, argv);
        if (!uo) return NULL;
        if (uo->type != ULVER_STRING) return ulver_error_form(env, argv, "must be a string");
        int mode = O_RDONLY;
        if (argv->next) {
                ulver_form *direction = argv->next;
                if (direction->type != ULVER_KEYWORD) return ulver_error_form(env, direction, "must be a keyword");
                if (direction->len == 6 && !ulver_utils_memicmp(direction->value, ":input", 6)) {
                        mode = O_RDONLY;
                }
                else if (direction->len == 7 && !ulver_utils_memicmp(direction->value, ":output", 7)) {
                        mode = O_WRONLY;
                }
                else if (direction->len == 3 && !ulver_utils_memicmp(direction->value, ":io", 3)) {
                        mode = O_WRONLY;
                }
        }
	// ensure the hub is running
	ulver_hub(env);
	ulver_thread *ut = ulver_current_thread(env);
	ulver_object *stream = ulver_object_new(env, ULVER_STREAM);
	if (uv_fs_open(ut->hub_loop, &stream->file, uo->str, mode, 0, NULL) < 0) {
                return ulver_error(env, "unable to open file");
        }
        return stream;
}


static uv_buf_t ulver_reader_alloc(uv_handle_t *handle, size_t suggested_size) {
	ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
        ulver_env *env = uvs->env;
	uv_buf_t buf;
	buf.base = env->alloc(env, uvs->len);
	buf.len = uvs->len;
	return buf;
}

static void ulver_reader_switch_cb(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
	ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
        ulver_env *env = uvs->env;
	uvs->coro->blocked = 0;
        // back to coro
	uvs->buf = buf.base;
	uvs->len = buf.len;
        ulver_coro_switch(env, uvs->coro);
}

ulver_object *ulver_fun_read_string(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, "read-string requires two arguments");

        ulver_object *stream = ulver_eval(env, argv);
        if (!stream) return NULL;
        if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");

        ulver_object *amount = ulver_eval(env, argv->next);
        if (!amount) return NULL;
        if (amount->type != ULVER_NUM) return ulver_error_form(env, argv->next, "is not a num");

        ulver_thread *ut = ulver_current_thread(env);

        ut->current_coro->blocked = 1;
	ulver_uv_stream *uvs = stream->stream;
	uvs->len = amount->n;
        uv_read_start(&uvs->handle.s, ulver_reader_alloc, ulver_reader_switch_cb);

        ulver_coro_switch(env, ut->hub);

	uv_read_stop(&uvs->handle.s);

	ulver_object *ret = ulver_object_from_string(env, uvs->buf, uvs->len);

	env->free(env, uvs->buf, amount->n);

        return ret;
}



static void ulver_writer_switch_cb(uv_write_t* handle, int status) {
        ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
        ulver_env *env = uvs->env;
        uvs->coro->blocked = 0;
        // back to coro
        ulver_coro_switch(env, uvs->coro);
}

ulver_object *ulver_fun_write_string(ulver_env *env, ulver_form *argv) {
	printf("READY TO WRITE STRING\n");
        if (!argv || !argv->next) return ulver_error(env, "write-string requires two arguments");

	ulver_object *stream = ulver_eval(env, argv);
	if (!stream) return NULL;
	if (stream->type != ULVER_STREAM) return ulver_error_form(env, argv, "is not a stream");

	ulver_object *body = ulver_eval(env, argv->next);
        if (!body) return NULL;
        if (body->type != ULVER_STRING) return ulver_error_form(env, argv->next, "is not a string");

	ulver_thread *ut = ulver_current_thread(env);

	ut->current_coro->blocked = 1;
	ulver_uv_stream *uvs = stream->stream;
	uvs->wbuf.base = body->str;
	uvs->wbuf.len = body->len;
	uvs->writer.data = uvs;
	uv_write(&uvs->writer, &uvs->handle.s, &uvs->wbuf, 1, ulver_writer_switch_cb);	

	printf("switch to hub again\n");

	ulver_coro_switch(env, ut->hub);	

	return body;
}

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
	printf("TIMER ELAPSED\n");
        ulver_uv_timer *uvt = (ulver_uv_timer *) handle->data;
	ulver_env *env = uvt->env;
        ulver_hub_schedule_waiters(env, uvt->ut, uvt->coro);
        uvt->coro->blocked = 0;
        // back to coro
	printf("BACK TO CORO %p\n", uvt->coro);
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
	if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
		ulver_uv_new_timer(env, uo->n * 1000, 0);
        }
        else if (uo->type == ULVER_FLOAT) {
		ulver_uv_new_timer(env, uo->d * 1000, 0);
        }
        return env->nil;

}

static void tcp_closed(uv_handle_t* handle) {
	printf("TCP CLOSED\n");
}

static void tcp_client(ulver_env *env, ulver_uv_stream *stream) {
	ulver_object *uo = ulver_object_new(env, ULVER_STREAM);
	uo->stream = stream;

	// set the first lambda symbol (if any)
	if (stream->func->lambda_list) {
		ulver_symbol_set(env, stream->func->lambda_list->value, stream->func->lambda_list->len, uo);
	}
	stream->coro->ret = ulver_call0(env, stream->func);
	printf("END OF THE CORO !!!\n");
	// close the connection
	uv_close((uv_handle_t *) &stream->handle.s, tcp_closed);
	printf("switching back to HUB !!!\n");
	ulver_coro_switch(env, stream->ut->hub);
}

/*
	on connection, accept it, and create a new coro
	calling the server hook function getting the new client stream as argument
*/
static void tcp_server_on_connect(uv_stream_t* handle, int status) {
	ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
	ulver_coro_switch(uvs->env, uvs->coro);
}

static void tcp_server(ulver_env *env, ulver_uv_stream *uvs) {
	ulver_thread *ut = ulver_current_thread(env);
        uv_listen(&uvs->handle.s, 128, tcp_server_on_connect);
	ulver_coro_switch(env, ut->hub);
	// here we create a new coro for each connection
	for(;;) {
		printf("A CONNECTION !!!!!!!!!!!!!!\n");
		ulver_uv_stream *client = env->alloc(env, sizeof(ulver_uv_stream));
		client->env = env;
		client->ut = ut;
		client->func = uvs->func;
		uv_tcp_init(ut->hub_loop, &client->handle.tcp);
		client->handle.tcp.data = client;
		uv_accept(&uvs->handle.s, &client->handle.s);
		// generate the new coro
        	ulver_coro *coro = ulver_coro_new(env, tcp_client, client);

        	// map the coro to utcp
        	client->coro = coro;

        	// add the new coro
        	if (!ut->coros) {
                	ut->coros = coro;
        	}
        	else {
                	ulver_coro *old = ut->coros;
                	ut->coros = coro;
                	coro->next = old;
                	old->prev = coro;
        	}

        	// create the new stack
        	ulver_coro *current_coro = ut->current_coro;
        	ut->current_coro = coro;
        	ulver_stack_push(env, ut);
        	ut->current_coro = current_coro;

		// switch to the new coro
		ulver_coro_switch(env, coro);
	}
	// probably never here
	printf("no more tcp server\n");
}

ulver_object *ulver_fun_make_tcp_server(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next || !argv->next->next) ulver_error(env, "make-tcp-server requires three arguments");
        ulver_object *addr = ulver_eval(env, argv);
        if (!addr) return NULL;
        if (addr->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");

        ulver_object *port = ulver_eval(env, argv->next);
        if (!port) return NULL;
        if (port->type != ULVER_NUM) return ulver_error_form(env, argv->next, "is not a string");

        ulver_object *on_client = ulver_eval(env, argv->next->next);
        if (!on_client) return NULL;
        if (on_client->type != ULVER_FUNC) return ulver_error_form(env, argv->next->next, "is not a function");
	// manually manage the lambda_list
	on_client->no_lambda = 1;
	
	// ensure hub is running
	ulver_hub(env);

	ulver_thread *ut = ulver_current_thread(env);

	ulver_uv_stream *uvs = env->alloc(env, sizeof(ulver_uv_stream));
        uv_tcp_init(ut->hub_loop, &uvs->handle.tcp);
        uvs->env = env;
        uvs->ut = ut;
        uvs->func = on_client;

        uvs->handle.tcp.data = uvs;

        struct sockaddr_in address = uv_ip4_addr(addr->str, port->n);
        uv_tcp_bind(&uvs->handle.tcp, address);

	// generate the new coro
        ulver_coro *coro = ulver_coro_new(env, tcp_server, uvs);

	// map the coro to utcp
	uvs->coro = coro;

        // add the new coro
        if (!ut->coros) {
                ut->coros = coro;
        }
        else {
                ulver_coro *old = ut->coros;
                ut->coros = coro;
                coro->next = old;
                old->prev = coro;
        }

        // create the new stack
        ulver_coro *current_coro = ut->current_coro;
        ut->current_coro = coro;
        ulver_stack_push(env, ut);
        ut->current_coro = current_coro;

	// mark the listening coro as blocked
	coro->blocked = 1;

	// schedule the new coro to finish the server setup
	ulver_hub_schedule_coro(env, coro);

        // return a new coro object
        ulver_object *oc = ulver_object_new(env, ULVER_CORO);
        oc->coro = coro;
        return oc;
}
