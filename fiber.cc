#include "fiber.h"

#include <cassert>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <setjmp.h>
#include <ucontext.h>

#ifdef DEBUG
#include <valgrind/valgrind.h>
#endif

// #define FIBER_DEBUG

static const unsigned int STACK_SIZE = 1024 * 1024;
// static const unsigned int STACK_SIZE = 1024 * 256;

struct fiber_context;

struct identity
{
    jmp_buf *state;
#ifdef FIBER_DEBUG
    usize index;
    std::string name;
#endif
};

struct fiber_context
{
    char stack[STACK_SIZE];

#ifdef DEBUG
    usize valgrind_handle;

    fiber_context()
    {
        valgrind_handle = VALGRIND_STACK_REGISTER(stack, stack + STACK_SIZE);
    }

    ~fiber_context()
    {
        VALGRIND_STACK_DEREGISTER(valgrind_handle);
    }
#endif
};

#ifdef FIBER_DEBUG
#define fiber_log(...) printf(__VA_ARGS__)
static usize next_index = 1;
#else
#define fiber_log(...)
#endif

static usize active_fiber_count;

static identity anchor = {
    .state = nullptr,
#ifdef FIBER_DEBUG
    .index = 0,
    .name = "anchor",
#endif
};
static identity current = anchor;

static std::queue<identity *> ready_queue;

static fiber_context *cleanup;
static std::vector<fiber_context *> context_pool;

static std::mutex m;
static std::condition_variable cv;
static const std::function<void()> *input_func = nullptr;

static void do_cleanup()
{
    if (!cleanup)
        return;

    --active_fiber_count;

    if (context_pool.size() < 10)
        context_pool.push_back(cleanup);
    else
        delete cleanup;

    cleanup = nullptr;
}

static void load_state()
{
    fiber_log("yield %zu\n", ready_queue.size());

    if (!ready_queue.empty())
    {
        current = *ready_queue.front();
        ready_queue.pop();
    }
    else
        current = anchor;

    fiber_log("yield to %zu %s\n", current.index, current.name.c_str());

    longjmp(*current.state, 1);
}

static void init_fiber(fiber_context *fib, const std::function<void()> &run)
{
    fiber_log("return to %zu %s (init)\n", current.index, current.name.c_str());

    run();

    cleanup = fib;
    fiber_log("yield from %zu %s (destroy)\n", current.index, current.name.c_str());

    load_state();
}

void condition::wait()
{
    jmp_buf store;
    identity self = current;
    self.state = &store;

    fiber_log("yield from %zu %s\n", self.index, self.name.c_str());

    waiting_queue.push(&self);
    if (setjmp(store) == 0)
        load_state();

#ifdef FIBER_DEBUG
    assert(self.index == current.index);
#endif

    fiber_log("return to %zu %s\n", self.index, self.name.c_str());
    do_cleanup();
}

void condition::notify()
{
    while (!waiting_queue.empty())
    {
        ready_queue.push(waiting_queue.front());
        waiting_queue.pop();
    }
}

usize fiber::run()
{
    std::unique_lock<std::mutex> lk(m);

    while (ready_queue.empty() && input_func == nullptr)
        cv.wait(lk);

    if (input_func != nullptr)
    {
        input_func->operator()();
        input_func = nullptr;
        cv.notify_all();
    }

    jmp_buf store;
    anchor.state = &store;

    fiber_log("yield from anchor\n");

    if (setjmp(store) == 0)
        load_state();

    fiber_log("return to anchor\n");

    current = anchor;
    do_cleanup();

    return active_fiber_count;
}

void fiber::delay(usize ms)
{
    condition done;
    std::thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        fiber::input([&] { done.notify(); });
    })
        .detach();
    done.wait();
}

void fiber::input(const std::function<void()> &run)
{
    std::unique_lock<std::mutex> lk(m);

    while (input_func != nullptr)
        cv.wait(lk);

    input_func = &run;
    cv.notify_all();

    while (input_func == &run)
        cv.wait(lk);
}

/*
bool fiber::fork()
{
    if (current.owner == nullptr)
        throw std::runtime_error("fork called outside of fiber context");

    auto src = current.owner;
    auto copy = new fiber_context();

    jmp_buf local;
    ucontext_t context;

    volatile bool swapped = false;
    auto result = getcontext(&context);

    printf("stack %x %x\n", src->stack, context.uc_stack.ss_sp);

    printf("result %d\n", result);

    if (swapped)
    {
        getcontext(&context);
        printf("stack %x %x\n", src->stack, context.uc_stack.ss_sp);
        // jmp_buf store;

        // identity self;
        // self.state = &store;
        // self.owner = copy;
        // self.index = next_index++;
        // self.name = current.name + ":fork";

        // ready_queue.push(&self);

        // if (setjmp(store) == 0)
        //     longjmp(local, 1);

        return true;
    }
    else
    {
        swapped = true;

        // auto stack_size = (usize)((intptr_t)context.uc_stack.ss_sp - (intptr_t)src->stack);
        // context.uc_stack.ss_sp = copy->stack + stack_size;

        // printf("clone %zu\n", stack_size);

        // if (setjmp(local) == 0)
        setcontext(&context);

        // memcpy(copy->stack, src->stack, stack_size);
        return false;
    }
}
*/

void fiber::create(const std::string &name, const std::function<void()> &run)
{
    ++active_fiber_count;

    fiber_context *ctx;
    if (context_pool.empty())
        ctx = new fiber_context();
    else
    {
        ctx = context_pool.back();
        context_pool.pop_back();
    }

    ucontext_t context;
    context.uc_stack.ss_sp = ctx->stack;
    context.uc_stack.ss_size = sizeof(ctx->stack);
    context.uc_stack.ss_flags = 0;
    context.uc_link = nullptr;

    getcontext(&context);
    makecontext(&context, (void (*)())init_fiber, 2, ctx, &run);

    jmp_buf store;
    identity self = current;
    self.state = &store;
    ready_queue.push(&self);

    if (setjmp(store) == 0)
    {
#ifdef FIBER_DEBUG
        current.index = next_index++;
        current.name = name;
#endif
        setcontext(&context);
    }

    fiber_log("return to %zu %s (create)\n", current.index, current.name.c_str());

    do_cleanup();
}
