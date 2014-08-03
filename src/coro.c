#include <ulver.h>

#ifndef __WIN32__

#include <ucontext.h>
#include <sys/mman.h>

typedef struct ulver_coro_context ulver_coro_context;
struct ulver_coro_context {
#ifdef SPLITSTACK
	void *ss_contexts[10];
#else
	void *stack;
#endif
        ucontext_t context;
};

#ifdef SPLITSTACK
extern void __splitstack_block_signals_context(void *, int *, int *);
extern void __splitstack_getcontext(void *);
extern void __splitstack_setcontext(void *);
extern void *__splitstack_makecontext(size_t, void *, size_t *);
extern void __splitstack_releasecontext(void *);
#endif

void *ulver_coro_alloc_context(ulver_env *env) {
	return env->alloc(env, sizeof(ulver_coro_context));
}

void ulver_coro_free_context(ulver_env *env, ulver_coro *coro) {
	if (coro != coro->thread->main_coro) {
		ulver_coro_context *ucc = (ulver_coro_context *) coro->context;
#ifdef SPLITSTACK
		__splitstack_releasecontext(ucc->ss_contexts);
#else
		munmap(ucc->stack - 4096, 32768 + 8192);
#endif
	}
	return env->free(env, coro->context, sizeof(ulver_coro_context));
}

ulver_coro *ulver_coro_new(ulver_env *env, void *func, void *arg2) {
        ulver_coro *coro = env->alloc(env, sizeof(ulver_coro));
	ulver_coro_context *ucc = ulver_coro_alloc_context(env);
	coro->context = ucc;
	coro->thread = ulver_current_thread(env);
        getcontext(&ucc->context);
        size_t len= 0 ;
        int off = 0;
#ifdef SPLITSTACK
        void *stack = __splitstack_makecontext(8192, ucc->ss_contexts, &len);
        __splitstack_block_signals_context(ucc->ss_contexts, &off, NULL);
#else
	void *stack = mmap(NULL, 32768 + 8192, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0) + 4096;
	mprotect(stack - 4096, 4096, PROT_NONE);
	mprotect(stack + 32768, 4096, PROT_NONE);
	len = 32768;
#endif
        ucc->context.uc_link = 0;
        ucc->context.uc_stack.ss_sp = stack;
        ucc->context.uc_stack.ss_size = len;
        ucc->context.uc_stack.ss_flags = 0;

	makecontext(&ucc->context, func, 2, env, arg2);
        return coro;
}

void ulver_coro_switch(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);
	ulver_coro *current_coro = ut->current_coro;
	ulver_coro_context *ucc = (ulver_coro_context *) current_coro->context;
	ulver_coro_context *ucc2 = (ulver_coro_context *) coro->context;
	ut->current_coro = coro;
#ifdef SPLITSTACK
	if (current_coro != ut->main_coro) {	
        	__splitstack_getcontext(ucc->ss_contexts);
	}
        if (coro != ut->main_coro) {

                __splitstack_setcontext(ucc2->ss_contexts);
        }
#endif
        swapcontext(&ucc->context, &ucc2->context);
        ut->current_coro = current_coro;
}

void ulver_coro_fast_switch(ulver_env *env, ulver_coro *coro) {
	ulver_thread *ut = ulver_current_thread(env);
	ut->current_coro = coro;
	ulver_coro_context *ucc = (ulver_coro_context *) coro->context;
#ifdef SPLITSTACK
	if (coro != ut->main_coro) {
                __splitstack_setcontext(ucc->ss_contexts);
	}
#endif
	setcontext(&ucc->context);
}
#else
typedef struct ulver_coro_context ulver_coro_context;
struct ulver_coro_context {
	LPVOID fiber;
};
void *ulver_coro_alloc_context(ulver_env *env) {
	ulver_thread *ut = ulver_current_thread(env);
	ulver_coro_context *ucc = env->alloc(env, sizeof(ulver_coro_context));
	if (!ut->current_coro) {
		printf("converting a thread to a fiber\n");
		ucc->fiber = ConvertThreadToFiber(NULL);
	}
	return ucc;
	
}
void ulver_coro_free_context(ulver_env *env, ulver_coro *coro) {
	ulver_coro_context *ucc = (ulver_coro_context *) coro->context;
	DeleteFiber(ucc->fiber);
	return env->free(env, coro->context, sizeof(ulver_coro_context));
}
ulver_coro *ulver_coro_new(ulver_env *env, void *func, void *arg2) {
        ulver_coro *coro = env->alloc(env, sizeof(ulver_coro));
	ulver_coro_context *ucc = ulver_coro_alloc_context(env);
	coro->context = ucc;
	ucc->fiber = CreateFiber(32768, func, env);
        return coro;
}
void ulver_coro_switch(ulver_env *env, ulver_coro *coro) {
	ulver_coro_context *ucc = (ulver_coro_context *) coro->context;
	SwitchToFiber(ucc->fiber);
	
}
void ulver_coro_fast_switch(ulver_env *env, ulver_coro *coro) {
	ulver_coro_context *ucc = (ulver_coro_context *) coro->context;
	SwitchToFiber(ucc->fiber);
}
#endif



// platform independent functions

void ulver_coro_yield(ulver_env *env, ulver_object *ret) {
        ulver_thread *ut = ulver_current_thread(env);
        ut->current_coro->ret = ret;
        ulver_coro_switch(env, ut->hub);
}
