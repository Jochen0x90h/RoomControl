#include "Message.hpp"


bool convert(MessageType dstType, void *dstMessage, MessageType srcType, void const *srcMessage) {
	Message &dst = *reinterpret_cast<Message *>(dstMessage);
	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (dstType) {
		case MessageType::UNKNOWN:
			return false;
		case MessageType::ON_OFF:
			switch (srcType) {
				case MessageType::ON_OFF:
					dst.onOff = src.onOff;
					break;
				case MessageType::ON_OFF2:
					// invert on/off (0, 1, 2 -> 1, 0, 2)
					dst.onOff = src.onOff ^ 1 ^ (src.onOff >> 1);
					break;
				case MessageType::TRIGGER:
					// trigger (button) toggles on/off
					if (src.trigger == 0)
						return false; // conversion failed
					dst.onOff = 2;
					break;
				case MessageType::UP_DOWN:
					// up switches off, down switches on (1, 2 -> 0, 1)
					if (src.upDown == 0)
						return false; // conversion failed
					dst.onOff = src.upDown - 1;
					break;
				default:
					return false;
			}
			break;
		case MessageType::ON_OFF2:
			switch (srcType) {
				case MessageType::ON_OFF:
					// invert on/off (0, 1, 2 -> 1, 0, 2)
					dst.onOff = src.onOff ^ 1 ^ (src.onOff >> 1);
					break;
				case MessageType::ON_OFF2:
					dst.onOff = src.onOff;
					break;
				case MessageType::TRIGGER:
					// use trigger (button) state as switch state
					dst.onOff = src.trigger;
					break;
				case MessageType::UP_DOWN:
					// up switches on, down switches off (1, 2 -> 1, 0)
					if (src.upDown == 0)
						return false; // conversion failed
					dst.onOff = 2 - src.upDown;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::TRIGGER:
			switch (srcType) {
				case MessageType::TRIGGER:
					dst.trigger = src.trigger;
					break;
				case MessageType::UP_DOWN:
					// use up as press (0, 1 -> 0, 1)
					if (src.upDown == 2)
						return false; // conversion failed
					dst.trigger = src.upDown;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::TRIGGER2:
			switch (srcType) {
				case MessageType::TRIGGER:
					dst.trigger = src.trigger;
					break;
				case MessageType::UP_DOWN:
					// use down as press (0, 2 -> 0, 1)
					if (src.upDown == 1)
						return false; // conversion failed
					dst.trigger = src.upDown >> 1;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::UP_DOWN:
			switch (srcType) {
				case MessageType::TRIGGER:
					// use press as up (0, 1 -> 0, 1)
					dst.upDown = src.trigger;
					break;
				case MessageType::UP_DOWN:
					dst.trigger = src.upDown;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
		case MessageType::UP_DOWN2:
			switch (srcType) {
				case MessageType::TRIGGER:
					// use press as down (0, 1 -> 0, 2)
					dst.upDown = src.trigger << 1;
					break;
				case MessageType::UP_DOWN:
					// exchange up and down (0, 1, 2 -> 0, 2, 1)
					dst.upDown = (src.upDown << 1) | (src.upDown >> 1);
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::LEVEL:
			switch (srcType) {
				case MessageType::LEVEL:
					dst.level = src.level;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::TEMPERATURE:
			switch (srcType) {
				case MessageType::TEMPERATURE:
					dst.temperature = src.temperature;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
	/*
		case MessageType::CELSIUS:
			switch (srcType) {
			case MessageType::CELSIUS:
				dst.temperature = src.temperature;
				break;
			case MessageType::FAHRENHEIT:
				// fahrenheit -> celsius
				dst.temperature.set((src.temperature - 32.0f) * 5.0 / 9.0, src.temperature.getFlag());
				break;
			default:
				// conversion failed
				return false;
			}
			break;
		case MessageType::FAHRENHEIT:
			switch (srcType) {
			case MessageType::CELSIUS:
				dst.temperature = src.temperature;
				break;
			case MessageType::FAHRENHEIT:
				// celsius -> fahrenheit
				dst.temperature.set(src.temperature * 9.0f / 5.0f + 32.0f, src.temperature.getFlag());
				break;
			default:
				// conversion failed
				return false;
			}
			break;
	*/
		case MessageType::PRESSURE:
			switch (srcType) {
				case MessageType::PRESSURE:
					dst.pressure = src.pressure;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::AIR_HUMIDITY:
			switch (srcType) {
				case MessageType::AIR_HUMIDITY:
					dst.airHumidity = src.airHumidity;
					break;
				default:
					// conversion failed
					return false;
			}
			break;

		case MessageType::AIR_VOC:
			switch (srcType) {
				case MessageType::AIR_VOC:
					dst.airVoc = src.airVoc;
					break;
				default:
					// conversion failed
					return false;
			}
			break;
	}

	// conversion successful
	return true;
}

/*
bool convertOnOffIn(MessageType dstType, Message &dst, uint8_t src) {
	if (src > 2)
		return false;

	switch (dstType) {
		case MessageType::ON_OFF1:
			// identity
			dst.onOff = src;
			break;
		case MessageType::ON_OFF2:
			// inverse (0, 1, 2 -> 1, 0, 2)
			dst.onOff = src ^ 1 ^ (src >> 1);
			break;
		case MessageType::ON_OFF3:
			// on -> toggle, toggle -> on (1, 2 -> 2, 1)
			if (src == 0)
				return false; // conversion failed
			dst.onOff = 3 - src;
			break;
		case MessageType::ON_OFF4:
			// off -> toggle, toggle -> off (0, 2 -> 2, 0)
			if (src == 1)
				return false; // conversion failed
			dst.onOff = 2 - src;
			break;
		case MessageType::TRIGGER1:
			// off -> inactive, on -> activate, toggle -> activate (0, 1, 2 -> 0, 1, 1)
			dst.onOff = src == 0 ? 0 : 1;
			break;
		case MessageType::TRIGGER2:
			// off -> activate, on -> inactive, toggle -> activate (0, 1, 2 -> 1, 0, 1)
			dst.onOff = src == 0 ? 1 : src - 1;
			break;
		case MessageType::UP_DOWN1:
			// off -> inactive, on -> up, toggle -> up (0, 1, 2 -> 0, 1, 1)
			dst.onOff = src == 0 ? 0 : 1;
			break;
		case MessageType::UP_DOWN2:
			// off -> inactive, on -> down, toggle -> down (0, 1, 2 -> 0, 2, 2)
			dst.onOff = src == 0 ? 0 : 2;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
*/
bool convertOnOffIn(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src > 2)
		return false;

	switch (dstType) {
		case MessageType::INDEXED_COMMAND:
			dst.indexedCommand.index = convertOptions.u32 >> 24;
			// fall through
		case MessageType::COMMAND: {
			int c = (convertOptions.i32 >> src * 3) & 7;
			if (c >= 6)
				return false; // conversion failed
			dst.command = Command(c);
			break;
		}
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
/*
bool convertTriggerIn(MessageType dstType, Message &dst, uint8_t src) {
	if (src > 1)
		return false;

	switch (dstType) {
		case MessageType::ON_OFF1:
		case MessageType::ON_OFF2:
			// activate -> toggle (1 -> 2)
			if (src == 0)
				return false; // conversion failed
			dst.onOff = 2;
			break;
		case MessageType::ON_OFF3:
			// activate -> on (1 -> 1)
			if (src == 0)
				return false; // conversion failed
			dst.onOff = 1;
			break;
		case MessageType::ON_OFF4:
			// activate -> off (1 -> 0)
			if (src == 0)
				return false; // conversion failed
			dst.onOff = 0;
			break;
		case MessageType::TRIGGER1:
		case MessageType::TRIGGER2:
			// identity
			dst.trigger = src;
			break;
		case MessageType::UP_DOWN1:
			// inactive -> inactive, activate -> up (0, 1 -> 0, 1)
			dst.upDown = src;
			break;
		case MessageType::UP_DOWN2:
			// inactive -> inactive, activate -> down (0, 1 -> 0, 2)
			dst.upDown = src << 1;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
*/
bool convertTriggerIn(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src > 1)
		return false;

	switch (dstType) {
		case MessageType::INDEXED_COMMAND:
			dst.indexedCommand.index = convertOptions.u32 >> 24;
			// fall through
		case MessageType::COMMAND: {
			int c = (convertOptions.i32 >> src * 3) & 7;
			if (c >= 6)
				return false; // conversion failed
			dst.command = Command(c);
			break;
		}
		case MessageType::SET_TEMPERATURE: {
			if (src == 0)
				return false; // conversion failed
			dst.setTemperature = convertOptions.ff;
			break;
		}
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
/*
bool convertUpDownIn(MessageType dstType, Message &dst, uint8_t src) {
	if (src > 2)
		return false;

	switch (dstType) {
		case MessageType::ON_OFF1:
			// up -> on, down -> off (1, 2 -> 1, 0)
			if (src == 0)
				return false; // conversion failed
			dst.onOff = 2 - src;
			break;
		case MessageType::ON_OFF2:
			// up -> off, down -> on (1, 2 -> 0, 1)
			if (src == 0)
				return false; // conversion failed
			dst.onOff = src - 1;
			break;
		case MessageType::ON_OFF3:
			// up -> toggle (1 -> 2)
			if (src != 1)
				return false; // conversion failed
			dst.onOff = 2;
			break;
		case MessageType::ON_OFF4:
			// down -> toggle (2 -> 2)
			if (src != 2)
				return false; // conversion failed
			dst.onOff = 2;
			break;
		case MessageType::TRIGGER1:
			// inactive -> inactive, up -> activate (0, 1 -> 0, 1)
			if (src == 2)
				return false; // conversion failed
			dst.trigger = src;
			break;
		case MessageType::TRIGGER2:
			// inactive -> inactive, down -> activate (0, 2 -> 0, 1)
			if (src == 1)
				return false; // conversion failed
			dst.trigger = src >> 1;
			break;
		case MessageType::UP_DOWN1:
			// identity
			dst.upDown = src;
			break;
		case MessageType::UP_DOWN2:
			// inverse (0, 1, 2 -> 0, 2, 1)
			dst.upDown = ((src & 1) << 1) | (src >> 1);
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
*/
bool convertUpDownIn(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src > 2)
		return false;

	switch (dstType) {
		case MessageType::INDEXED_COMMAND:
			dst.indexedCommand.index = convertOptions.u32 >> 24;
			// fall through
		case MessageType::COMMAND: {
			int ss = (convertOptions.i32 >> src * 3) & 7;
			if (ss >= 6)
				return false; // conversion failed
			dst.command = Command(ss);
			break;
		}
		case MessageType::SET_TEMPERATURE: {
			if (src == 0)
				return false; // conversion failed
			if (!convertOptions.ff.getFlag()) {
				// absolute
				if (!convertOptions.ff.isNegative()) {
					// positive: Set on up
					if (src == 2)
						return false; // conversion failed
					dst.setTemperature = convertOptions.ff;
				} else {
					// negative: Set on down
					if (src == 1)
						return false; // conversion failed
					dst.setTemperature = -convertOptions.ff;
				}
			} else {
				// relative
				dst.setTemperature = src == 1 ? convertOptions.ff : -convertOptions.ff;
			}
			break;
		}
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertOnOffOut(uint8_t &dst, MessageType srcType, Message const &src) {
	switch (srcType) {
		case MessageType::ON_OFF1:
			dst = src.onOff;
			break;
		case MessageType::ON_OFF2:
			// invert on/off (0, 1, 2 -> 1, 0, 2)
			dst = src.onOff ^ 1 ^ (src.onOff >> 1);
			break;
		case MessageType::TRIGGER1:
			// trigger (e.g.button) toggles on/off
			if (src.trigger == 0)
				return false;
			dst = 2;
			break;
		case MessageType::UP_DOWN1:
			// use state of up contact (0, 1 -> 0, 1)
			if (src.upDown >= 2)
				return false;
			dst = src.upDown;
			break;
		case MessageType::UP_DOWN2:
			// use state of down contact (0, 2 -> 0, 1)
			if ((src.upDown & ~2) != 0)
				return false;
			dst = src.upDown >> 1;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertTriggerOut(uint8_t &dst, MessageType srcType, Message const &src) {
	switch (srcType) {
		case MessageType::ON_OFF1:
			// off -> activate (0 -> 1)
			if (src.onOff != 0)
				return false; // conversion failed
			dst = 1;
			break;
		case MessageType::ON_OFF2:
			// on -> activate (1 -> 1)
			if (src.onOff != 1)
				return false; // conversion failed
			dst = 1;
			break;
		case MessageType::TRIGGER1:
			dst = src.trigger;
			break;
		case MessageType::UP_DOWN1:
			// use up as trigger (0, 1 -> 0, 1)
			if (src.upDown >= 2)
				return false; // conversion failed
			dst = src.upDown;
			break;
		case MessageType::UP_DOWN2:
			// use down as trigger (0, 2 -> 0, 1)
			if ((src.upDown & ~2) != 0)
				return false; // conversion failed
			dst = src.upDown >> 1;
			break;
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}

bool convertUpDownOut(uint8_t &dst, MessageType srcType, Message const &src) {
	switch (srcType) {
		case MessageType::ON_OFF1:
			// off -> up, on -> down (0 -> 1, 1 -> 2)
			if (src.onOff >= 2)
				return false; // conversion failed
			dst = src.onOff + 1;
			break;
		case MessageType::ON_OFF2:
			// off -> down, on -> up (0 -> 2, 1 -> 1)
			if (src.onOff >= 2)
				return false; // conversion failed
			dst = 2 - src.onOff;
			break;
		case MessageType::TRIGGER1:
			// use press as up (0, 1 -> 0, 1)
			dst = src.trigger;
			break;
		case MessageType::TRIGGER2:
			// use press as down (0, 1 -> 0, 2)
			dst = src.trigger << 1;
			break;
		case MessageType::UP_DOWN1:
			dst = src.upDown;
			break;
		case MessageType::UP_DOWN2:
			// invert up/down (0, 1, 2 -> 0, 2, 1)
			dst = ((src.upDown & 1) << 1) | (src.upDown >> 1);
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
