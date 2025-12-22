#pragma once

#include <cstddef>
#include <cstdint>

#include "PositionStore.hpp"

class EepromPositionStore final : public PositionStore
{
public:
	EepromPositionStore();

	bool load(uint16_t &position_out) override;
	void save(uint16_t position) override;

private:
	struct PositionRecord
	{
		uint32_t magic;
		uint16_t position;
		uint16_t checksum;
	};

	bool ensure_ready();
	uint16_t checksum(uint16_t position) const;

	bool m_ready{false};
	bool m_has_value{false};
	uint16_t m_last_value{0U};
	size_t m_eeprom_size{0U};
};
