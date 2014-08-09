#include <ulver.h>

/*

	ulver RPC subsystem
	
	every ulver instance can receive requests from other nodes, and return back the result to them.

	every ulver call can be executed on a remote node using the fun@node syntax

	to became a cluster member just bind to a tcp address

	(ulver:cluster "192.168.173.4" 4040)

	to add a node in the cluster:

	(ulver:defnode node0001 "192.168.173.5" 4041)

	then

	(print@node0001 (+ 2 3))

	will execute (+ 2 3) on node0001 and will print its result in node0001

	as well

	(print@node0001 (+@node002 2 3))	

	will execute (+ 2 3) on node0002 and will print its result in node0001

	every global symbol is reachable with the same approach:

	(print *foobar*@node0005)

	will locally print the value of the symbol *foobar* on node0005

	the network packet is a 4byte header followed by the msgpack serialization of the form
	the 4 byte header is the big endian size of the following msgpack blob

*/

typedef struct ulver_rpc_server ulver_rpc_server;
typedef struct ulver_rpc_client ulver_rpc_client;

struct ulver_rpc_server {
	uv_tcp_t tcp;
	struct sockaddr_in address;
	ulver_env *env;
	// switch here at every connection
	ulver_coro *coro;
};

struct ulver_rpc_client {
	ulver_rpc_server *server;	
	uv_tcp_t handle;
	ulver_coro *coro;
	char *msgpack;
	uint64_t msgpack_len;
};

static void rpc_close(uv_handle_t* handle) {
        ulver_rpc_client *urc = (ulver_rpc_client *) handle->data;
        urc->coro->dead = 1;
}

static void rpc_reader_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t* buf) {
        ulver_rpc_client *rpc = (ulver_rpc_client *) handle->data;
        ulver_env *env = rpc->server->env;
        buf->base = env->alloc(env, suggested_size);
        buf->len = suggested_size;
}

static void rpc_reader_switch_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t *buf) {
        ulver_rpc_client *urc = (ulver_rpc_client *) handle->data;
        ulver_env *env = urc->server->env;

	printf("coro = %p\n", urc->coro);
        urc->coro->blocked = 0;

	if (nread <= 0) {
		printf("oooops\n");
		return;
	}

	if (!urc->msgpack) {
		urc->msgpack = env->alloc(env, nread);
		urc->msgpack_len = nread;
		memcpy(urc->msgpack, buf->base, nread);
	}
	else {
		char *new_msgpack = env->alloc(env, urc->msgpack_len + nread);
		memcpy(new_msgpack, urc->msgpack, urc->msgpack_len);
		memcpy(new_msgpack + urc->msgpack_len, buf->base, nread);
		env->free(env, urc->msgpack, urc->msgpack_len);
		urc->msgpack = new_msgpack;
		urc->msgpack_len += nread;
	}
	printf("coro = %p\n", urc->coro);
        ulver_coro_switch(env, urc->coro);
}

static void rpc_client(ulver_coro *rpc_coro) {
        ulver_env *env = rpc_coro->env;
        ulver_rpc_client *client = (ulver_rpc_client *) rpc_coro->data;

        ulver_thread *ut = ulver_current_thread(env);

	// start reading until 4 bytes + msgpack are ready

	printf("reading headers\n");
	
	uv_read_start(&client->handle, rpc_reader_alloc, rpc_reader_switch_cb);

	for(;;) {
        	ulver_coro_switch(env, ut->hub);
		printf("data received %d\n", client->msgpack_len);
		if (client->msgpack_len >= 4) {
			uint32_t form_len = ntohl( *((uint32_t *)client->msgpack)); 
			printf("form_len = %d\n", form_len);
			if (form_len + 4 >= client->msgpack_len) {
				break;
			}
		}
		client->coro->blocked = 1;
	}

        uv_read_stop(&client->handle);

	printf("deserialize form\n");

	// the message is ready, let's deserialize it

	char *msgpack = client->msgpack+4;
	uint64_t msgpack_len = client->msgpack_len-4;
	ulver_form *argv = ulver_form_deserialize(rpc_coro->env, NULL, &msgpack, &msgpack_len);

	printf("argv = %p\n", argv);

	ulver_object *ret = ulver_eval(rpc_coro->env, argv);

	printf("ret = %p\n", ret);

	if (!ret) {
		ulver_report_error(env);
	}

	// serialize the response
	// ulver_object_serialize(env, ret);

	// send it
	//uv_write(...)

	// finally close the connection
        uv_close(&client->handle, rpc_close);

        // and switch back to the hub
        ulver_coro_switch(env, ut->hub);
}

static void rpc_server_on_connect(uv_stream_t* handle, int status) {
	printf("CONNECTION !!!\n");
        ulver_rpc_server *urs = (ulver_rpc_server *) handle->data;
        ulver_coro_switch(urs->env, urs->coro);
}

static void rpc_server(ulver_coro *rpc_coro) {
        ulver_env *env = rpc_coro->env;
        ulver_rpc_server *urs = (ulver_rpc_server *) rpc_coro->data;
        ulver_thread *ut = ulver_current_thread(env);

	// start waiting for connections
        uv_listen(&urs->tcp, 128, rpc_server_on_connect);
	printf("ready to switch %p\n", rpc_coro);
        ulver_coro_switch(env, ut->hub);

        // here we create a new coro for each connection
        for(;;) {
		printf("new connection !!!\n");
                ulver_rpc_client *client = env->alloc(env, sizeof(ulver_rpc_client));
		client->server = urs;
                uv_tcp_init(ut->hub_loop, &client->handle);
                client->handle.data = client;

                uv_accept(&urs->tcp, &client->handle);

                // generate the new coro
                ulver_coro *coro = ulver_coro_new(env, rpc_client, client);

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
        printf("no more rpc server\n");
}


ulver_object *ulver_fun_cluster(ulver_env *env, ulver_form *argv) {
	if (!argv || !argv->next) return ulver_error(env, ULVER_ERR_TWO_ARG);
	ulver_object *addr = ulver_eval(env, argv);
	if (!addr) return NULL;
	if (addr->type != ULVER_STRING) return ulver_error_form(env, ULVER_ERR_NOT_STRING, argv, NULL);

	ulver_object *port = ulver_eval(env, argv->next);
	if (!port) return NULL;
	if (port->type != ULVER_NUM) return ulver_error_form(env, ULVER_ERR_NOT_NUM, argv->next, NULL);

	// ensure hub is running
        ulver_hub(env);

	ulver_thread *ut = ulver_current_thread(env);

	ulver_rpc_server *urs = env->alloc(env, sizeof(ulver_rpc_server));

	urs->env = env;

	uv_tcp_init(ut->hub_loop, &urs->tcp);
	// map urs to handle
	urs->tcp.data = urs;

        uv_ip4_addr(addr->str, port->n, &urs->address);
        uv_tcp_bind(&urs->tcp, (struct sockaddr *)&urs->address, 0);

        // generate the new coro
        ulver_coro *coro = ulver_coro_new(env, rpc_server, urs);

        // map the coro to urs
        urs->coro = coro;

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

ulver_object *ulver_fun_defnode(ulver_env *env, ulver_form *argv) {
}
