#include <ulver.h>

/*

	channels are thread/coro-safe communication pipes
	allowing safe synchornization between execution contexts.

	They are heavily based on Go's channels

	As we are sharing the communication pipe between different
	hubs (read: different uv_loop_t) we need to use low-level functions

*/

static int set_nb(int fd) {
	int arg = fcntl(fd, F_GETFL, NULL);
	if (arg < 0) return -1;
	arg |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, arg) < 0) return -1;
	return 0;
}

ulver_object *ulver_fun_make_chan(ulver_env *env, ulver_form *argv) {
	ulver_object *uo = ulver_object_new(env, ULVER_CHANNEL);
	uo->chan = env->alloc(env, sizeof(ulver_channel));
	if (pipe(uo->chan->_pipe)) goto error;
	if (set_nb(uo->chan->_pipe[0])) goto error2;
	if (set_nb(uo->chan->_pipe[1])) goto error2;
	uv_mutex_init(&uo->chan->lock);
	return uo;
error2:
	close(uo->chan->_pipe[0]);
	close(uo->chan->_pipe[1]);
error:
	env->free(env, uo->chan, sizeof(ulver_channel));
	uo->chan = NULL;	
	return NULL;
}

struct channel_io {
	uv_poll_t handle;
	ulver_env *env;
	ulver_coro *coro;
	ulver_channel *chan;
	int fd;
	// the returned msg (if NULL, you need to re-enqueue the polling)
	ulver_object *msg;
};

static void consume_channel(uv_poll_t* handle, int status, int events) {
	struct channel_io *io = (struct channel_io *) handle->data;
	int fd = io->fd;
        uv_poll_stop(handle);
	io->coro->blocked = 0;
	char byte;
	ssize_t rlen = read(fd, &byte, 1);
	if (rlen != 1) goto tocoro;
	uv_mutex_lock(&io->chan->lock);
	if (io->chan->head) {
		ulver_channel_msg *current = io->chan->head;
		io->msg = io->chan->head->o;
		io->chan->head = current->next;
		if (current == io->chan->tail) {
			io->chan->tail = NULL;
		}
		io->env->free(io->env, current, sizeof(ulver_channel_msg));
	}
	uv_mutex_unlock(&io->chan->lock);
tocoro:
	ulver_coro_switch(io->env, io->coro);
}

static void produce_channel(uv_poll_t* handle, int status, int events) {
        struct channel_io *io = (struct channel_io *) handle->data;
        int fd = io->fd;
        uv_poll_stop(handle);
	io->coro->blocked = 0;
        char byte = 0;
        ssize_t rlen = write(fd, &byte, 1);
        if (rlen != 1) goto tocoro;
        uv_mutex_lock(&io->chan->lock);
	ulver_channel_msg *cm = io->env->alloc(io->env, sizeof(ulver_channel_msg));
	cm->o = io->msg;
        if (!io->chan->head) {
		io->chan->head = cm;
	}
	if (io->chan->tail) {
		io->chan->tail->next = cm;
	}
	io->chan->tail = cm;
	io->msg = NULL;
        uv_mutex_unlock(&io->chan->lock);
tocoro:
        ulver_coro_switch(io->env, io->coro);
}

static void close_channel(uv_handle_t *handle) {
	struct channel_io *io = (struct channel_io *) handle->data;
	printf("closing %p\n", io);
        io->env->free(io->env, io, sizeof(struct channel_io));
}

ulver_object *ulver_fun_receive(ulver_env *env, ulver_form *argv) {
	if (!argv) return ulver_error(env, ULVER_ERR_ONE_ARG);
	ulver_object *chan = ulver_eval(env, argv);
	if (!chan) return NULL;
	if (chan->type != ULVER_CHANNEL) return ulver_error(env, ULVER_ERR_NOT_CHANNEL);
	ulver_thread *ut = ulver_current_thread(env);
	// ensure the hub is running
	ulver_hub(env);

	struct channel_io *io = env->alloc(env, sizeof(struct channel_io));	
	printf("created %p\n", io);
	io->env = env;
	io->coro = ut->current_coro;
	io->chan = chan->chan;
	io->fd = chan->chan->_pipe[0];

	uv_poll_init(ut->hub_loop, &io->handle, io->fd);
	io->handle.data = io;

	uv_poll_start(&io->handle, UV_READABLE, consume_channel);
	ut->current_coro->blocked = 1;
	for(;;) {
		// switch to hub
		ulver_coro_switch(env, ut->hub);
		if (io->msg) break;
        	ut->current_coro->blocked = 1;
		uv_poll_start(&io->handle, UV_READABLE, consume_channel);
	}
	// back from hub
	uv_close((uv_handle_t *)&io->handle, close_channel);
	ulver_object *ret = io->msg;
	return ret;
}

ulver_object *ulver_fun_send(ulver_env *env, ulver_form *argv) {
        if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
        ulver_object *chan = ulver_eval(env, argv);
        if (!chan) return NULL;
        if (chan->type != ULVER_CHANNEL) return ulver_error(env, ULVER_ERR_NOT_CHANNEL);

	ulver_object *msg = ulver_eval(env, argv->next);
	if (!msg) return NULL;

        ulver_thread *ut = ulver_current_thread(env);
        // ensure the hub is running
        ulver_hub(env);

        struct channel_io *io = env->alloc(env, sizeof(struct channel_io));
	printf("created %p\n", io);
        io->env = env;
        io->coro = ut->current_coro;
        io->chan = chan->chan;
        io->fd = chan->chan->_pipe[1];
	io->msg = msg;

        uv_poll_init(ut->hub_loop, &io->handle, io->fd);
        io->handle.data = io;

        uv_poll_start(&io->handle, UV_WRITABLE, produce_channel);
        ut->current_coro->blocked = 1;
        for(;;) {
                // switch to hub
                ulver_coro_switch(env, ut->hub);
                if (!io->msg) break;
        	ut->current_coro->blocked = 1;
        	uv_poll_start(&io->handle, UV_WRITABLE, produce_channel);
        }
        // back from hub
	uv_close((uv_handle_t *)&io->handle, close_channel);
        return msg;
}
