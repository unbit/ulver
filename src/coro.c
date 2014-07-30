#include <ulver.h>

#include <ucontext.h>

extern void __splitstack_block_signals_context(void *, int *, int *);
extern void __splitstack_getcontext(void *);
extern void __splitstack_setcontext(void *);
extern void *__splitstack_makecontext(size_t, void *, size_t *);
extern void __splitstack_releasecontext(void *);

ulver_coro *ulver_coro_new(ulver_env *env, void *func, void *arg2) {
        ulver_coro *coro = env->alloc(env, sizeof(ulver_coro));
        coro->env = env;
        getcontext(&coro->context);
        size_t len= 0 ;
        int off = 0;
#ifndef NO_SPLITSTACK
        void *stack = __splitstack_makecontext(8192, coro->ss_contexts, &len);
        __splitstack_block_signals_context(coro->ss_contexts, &off, NULL);
#else
        void *stack = malloc(8192);
#endif
        coro->context.uc_link = 0;
        coro->context.uc_stack.ss_sp = stack;
        coro->context.uc_stack.ss_size = len;
        coro->context.uc_stack.ss_flags = 0;

	makecontext(&coro->context, func, 2, env, arg2);
        return coro;
}

void ulver_coro_switch(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);
	ulver_coro *current_coro = ut->current_coro;
	ut->current_coro = coro;
	if (current_coro != ut->main_coro) {	
        	__splitstack_getcontext(current_coro->ss_contexts);
	}
        if (coro != ut->main_coro) {
                __splitstack_setcontext(coro->ss_contexts);
        }
        swapcontext(&current_coro->context, &coro->context);
        ut->current_coro = current_coro;
}

void ulver_coro_fast_switch(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);
	ut->current_coro = coro;
	setcontext(&coro->context);
}
