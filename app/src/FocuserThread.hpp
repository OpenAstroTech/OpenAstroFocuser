#pragma once

#include "Thread.hpp"

class Focuser;

class FocuserThread : public Thread {
public:
	explicit FocuserThread(Focuser &focuser);

	void start();

private:
	void run() override;

	Focuser &m_focuser;
};
