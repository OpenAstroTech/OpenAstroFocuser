#pragma once

#include <zephyr/kernel.h>

#include <cstddef>

class Thread
{
public:
    Thread(k_thread_stack_t *stack_memory, std::size_t stack_size, int priority, const char *name);
    virtual ~Thread() = default;

    Thread(const Thread &) = delete;
    Thread &operator=(const Thread &) = delete;

protected:
    bool start_thread();
    virtual void run() = 0;

private:
    static void thread_entry(void *, void *, void *);

    k_thread_stack_t *m_stack;
    std::size_t m_stack_size;
    int m_priority;
    const char *m_name;
    struct k_thread m_thread;
    bool m_started;
};
