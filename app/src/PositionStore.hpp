#pragma once

#include <cstdint>

class PositionStore
{
public:
	virtual ~PositionStore() = default;

	virtual bool load(uint16_t &position_out) = 0;
	virtual void save(uint16_t position) = 0;
};
