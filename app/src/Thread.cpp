#include "Thread.hpp"

Thread::Thread(k_thread_stack_t *stack_memory, std::size_t stack_size, int priority, const char *name)
    : m_stack(stack_memory), m_stack_size(stack_size), m_priority(priority), m_name(name), m_thread{},
      m_started(false)
{
}

bool Thread::start_thread()
{
    if (m_started)
    {
        return true;
    }

    if (m_stack == nullptr)
    {
        return false;
    }

    k_thread_create(&m_thread, m_stack, m_stack_size, thread_entry, this, nullptr, nullptr, m_priority, 0,
                    K_NO_WAIT);

    if (m_name != nullptr)
    {
        k_thread_name_set(&m_thread, m_name);
    }

    m_started = true;
    return true;
}

void Thread::thread_entry(void *arg1, void *, void *)
{
    auto *self = static_cast<Thread *>(arg1);
    if (self == nullptr)
    {
        return;
    }

    self->run();
}
