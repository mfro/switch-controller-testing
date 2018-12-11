#ifndef FIBER_H
#define FIBER_H

#include "common.h"

#include <queue>

struct identity;

namespace fiber
{

usize run();

void delay(usize ms);

void input(const std::function<void()> &evt);

void create(const std::string &name, const std::function<void()> &run);

} // namespace fiber

class condition
{
public:
    usize waiting() { return waiting_queue.size(); }

    void wait();

    void notify();

private:
    std::queue<identity *> waiting_queue;
};

class task
{
public:
    void wait()
    {
        if (!resolved)
            resolve_cv.wait();
    }

    void resolve()
    {
        resolved = true;
        resolve_cv.notify();
    }

private:
    bool resolved = false;
    condition resolve_cv;
};

template <typename T>
class promise
{
public:
    T wait()
    {
        t.wait();
        return value;
    }

    void resolve(const T &arg)
    {
        value = arg;
        t.resolve();
    }

private:
    task t;
    T value;
};

template <typename T>
class emitter
{
public:
    usize listening() { return cv.waiting(); }

    void next(T *out)
    {
        outputs.push_back(out);
        cv.wait();
    }

    void emit(const T &arg)
    {
        for (auto &fork : forks)
            fiber::create("emitter", [&] { fork(arg); });

        for (auto out : outputs)
            *out = arg;

        outputs.clear();
        cv.notify();
    }

    void listen(const std::function<void(T)> &arg)
    {
        forks.push_back(arg);
    }

private:
    std::vector<std::function<void(T)>> forks;
    std::vector<T *> outputs;
    condition cv;
};

#endif
