#include "FocuserThread.hpp"

#include <zephyr/logging/log.h>

#include "Focuser.hpp"

LOG_MODULE_DECLARE(focuser);

FocuserThread::FocuserThread(Focuser &focuser)
	: m_focuser(focuser)
{
}

void FocuserThread::start()
{
	k_thread_create(&m_thread, m_stack, K_THREAD_STACK_SIZEOF(m_stack),
			thread_entry, this, nullptr, nullptr, config::kFocuserThread.priority,
			0, K_NO_WAIT);
	k_thread_name_set(&m_thread, "focuser");
}

void FocuserThread::thread_entry(void *arg1, void *, void *)
{
	auto *self = static_cast<FocuserThread *>(arg1);
	if (self == nullptr)
	{
		return;
	}

	self->run();
}

void FocuserThread::run()
{
	m_focuser.loop();
}
