#include "FocuserThread.hpp"

#include <zephyr/logging/log.h>

#include "Configuration.hpp"
#include "Focuser.hpp"

LOG_MODULE_DECLARE(focuser);

FocuserThread::FocuserThread(Focuser &focuser)
	: Thread(config::threads::focuser_stack,
		 K_THREAD_STACK_SIZEOF(config::threads::focuser_stack),
		 config::threads::focuser_priority, "focuser"),
	  m_focuser(focuser)
{
}

void FocuserThread::start()
{
	if (!start_thread())
	{
		LOG_ERR("Failed to start focuser thread");
	}
}

void FocuserThread::run()
{
	m_focuser.loop();
}
