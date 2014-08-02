#include <ulver.h>

static void ulver_open_cb(uv_fs_t *fs) {
	printf("file opened\n");
}

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
	if (uv_fs_open(ut->hub_loop, &stream->file, uo->str, mode, 0, ulver_open_cb) < 0) {
                return ulver_error(env, "unable to open file");
        }

	ulver_coro_switch(env, ut->hub);

        return stream;
}


static void ulver_reader_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t* buf) {
	ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
        ulver_env *env = uvs->env;
	buf->base = env->alloc(env, uvs->len);
	buf->len = uvs->len;
}

static void ulver_reader_switch_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t *buf) {
	ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
        ulver_env *env = uvs->env;
	uvs->coro->blocked = 0;
        // back to coro
	uvs->buf = buf->base;
	uvs->len = buf->len;
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
	uvs->wbuf = uv_buf_init(body->str, body->len);
	//uvs->wbuf.base = body->str;
	//uvs->wbuf.len = body->len;
	uvs->writer.data = uvs;
	int ret = uv_write(&uvs->writer, &uvs->handle.s, &uvs->wbuf, 1, ulver_writer_switch_cb);	

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

static void ulver_timer_switch_cb(uv_timer_t* handle) {
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
	if (!uo) return NULL;
        if (uo->type == ULVER_NUM) {
		ulver_uv_new_timer(env, uo->n * 1000, 0);
        }
        else if (uo->type == ULVER_FLOAT) {
		ulver_uv_new_timer(env, uo->d * 1000, 0);
        }
        return env->nil;

}

static void tcp_close(uv_handle_t* handle) {
	ulver_uv_stream *uvs = (ulver_uv_stream *) handle->data;
	uvs->coro->dead = 1;
}

static void tcp_client(ulver_env *env, ulver_uv_stream *stream) {
	ulver_object *uo = ulver_object_new(env, ULVER_STREAM);
	uo->stream = stream;

	ulver_thread *ut = ulver_current_thread(env);

	// set the first lambda symbol (if any)
	if (stream->func->lambda_list) {
		ulver_symbolmap_set(env, ut->current_coro->stack->locals, stream->func->lambda_list->value, stream->func->lambda_list->len, uo, 0);
	}
	stream->coro->ret = ulver_call0(env, stream->func);
	// close the connection
	uv_close((uv_handle_t *) &stream->handle.s, tcp_close);
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
		ulver_uv_stream *client = env->alloc(env, sizeof(ulver_uv_stream));
		client->env = env;
		client->ut = ut;
		client->func = uvs->func;
		uv_tcp_init(ut->hub_loop, &client->handle.tcp);
		client->handle.tcp.data = client;
		uv_accept(&uvs->handle.s, &client->handle.s);
		// generate the new coro
        	ulver_coro *coro = ulver_coro_new(env, tcp_client, client);

        	// map the coro to the client
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
        	ulver_stack_push(env, ut, coro);

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

	struct sockaddr_in address;
        uv_ip4_addr(addr->str, port->n, &address);
        uv_tcp_bind(&uvs->handle.tcp, (struct sockaddr *)&address, 0);

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
        ulver_stack_push(env, ut, coro);

	// mark the listening coro as blocked
	coro->blocked = 1;

	// schedule the new coro to finish the server setup
	ulver_hub_schedule_coro(env, coro);

        // return a new coro object
        ulver_object *oc = ulver_object_new(env, ULVER_CORO);
        oc->coro = coro;
        return oc;
}

/*
ulver_object *ulver_fun_make_pipe(ulver_env *env, ulver_form *argv) {
	if (!argv) ulver_error(env, "make-pipe requires an argument");
	ulver_object *cmd = ulver_eval(env, argv);
	if (!cmd) return NULL;
	if (cmd->type != ULVER_STRING) return ulver_error_form(env, argv, "is not a string");

	// ensure the hub is running
	ulver_hub(env);

	ulver_thread *ut = ulver_current_thread(env);

	uv_process_options_t options;
	uv_stdio_container_t stdio[2];

	uv_pipe_init(ut->hub_loop, &out, 0);
	uv_pipe_init(ut->hub_loop, &in, 0);

	options.stdio = stdio;
  	options.stdio[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
	options.stdio[0].data.stream = (uv_stream_t*)&in;
	options.stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
	options.stdio[1].data.stream = (uv_stream_t*)&out;
	options.stdio_count = 2;

	uv_spawn(loop, &process, &options);
}
*/
