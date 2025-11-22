#pragma once

#include <zephyr/kernel.h>

#include <cstddef>

#include "Configuration.hpp"

class Focuser;

class FocuserThread {
public:
	explicit FocuserThread(Focuser &focuser);

	void start();

private:
	static void thread_entry(void *, void *, void *);
	void run();

	Focuser &m_focuser;
	k_thread_stack_t m_stack[K_THREAD_STACK_LEN(config::kFocuserThread.stack_size)];
	struct k_thread m_thread;
};
