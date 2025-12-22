#include "EepromPositionStore.hpp"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(position_store, CONFIG_APP_LOG_LEVEL);

namespace
{
#if DT_HAS_ALIAS(eeprom_0)
#define FOCUSER_EEPROM_NODE DT_ALIAS(eeprom_0)
#endif

#ifdef FOCUSER_EEPROM_NODE
const struct device *const k_eeprom = DEVICE_DT_GET(FOCUSER_EEPROM_NODE);
#endif

constexpr uint32_t kPositionMagic = 0x464F4350U; // "FOCP"
constexpr off_t kPositionOffset = 0;

} // namespace

EepromPositionStore::EepromPositionStore()
{
}

bool EepromPositionStore::load(uint16_t &position_out)
{
#ifndef FOCUSER_EEPROM_NODE
	ARG_UNUSED(position_out);
	return false;
#else
	if (!ensure_ready())
	{
		return false;
	}

	PositionRecord record{};
	const int ret = eeprom_read(k_eeprom, kPositionOffset, &record, sizeof(record));
	if (ret != 0)
	{
		LOG_WRN("Failed to read position from EEPROM (%d)", ret);
		return false;
	}

	if ((record.magic != kPositionMagic) || (record.checksum != checksum(record.position)))
	{
		return false;
	}

	position_out = record.position;
	m_last_value = record.position;
	m_has_value = true;
	return true;
#endif
}

void EepromPositionStore::save(uint16_t position)
{
#ifndef FOCUSER_EEPROM_NODE
	ARG_UNUSED(position);
	return;
#else
	if (m_has_value && (position == m_last_value))
	{
		return;
	}

	if (!ensure_ready())
	{
		return;
	}

	PositionRecord record{
		.magic = kPositionMagic,
		.position = position,
		.checksum = checksum(position),
	};

	const int ret = eeprom_write(k_eeprom, kPositionOffset, &record, sizeof(record));
	if (ret != 0)
	{
		LOG_ERR("Failed to save position to EEPROM (%d)", ret);
		return;
	}

	m_last_value = position;
	m_has_value = true;
	LOG_DBG("Saved focuser position 0x%04x (%u) to EEPROM", position, position);
#endif
}

bool EepromPositionStore::ensure_ready()
{
#ifndef FOCUSER_EEPROM_NODE
	return false;
#else
	if (!m_ready)
	{
		if (!device_is_ready(k_eeprom))
		{
			LOG_WRN("EEPROM device not ready");
			return false;
		}

		m_eeprom_size = eeprom_get_size(k_eeprom);
		if (m_eeprom_size < sizeof(PositionRecord))
		{
			LOG_ERR("EEPROM too small (%zu < %zu)", m_eeprom_size, sizeof(PositionRecord));
			return false;
		}

		m_ready = true;
	}

	return true;
#endif
}

uint16_t EepromPositionStore::checksum(uint16_t position) const
{
	return static_cast<uint16_t>(((kPositionMagic >> 16) ^ (kPositionMagic & 0xFFFFU) ^ position) & 0xFFFFU);
}
