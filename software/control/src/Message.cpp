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
		
	case MessageType::HECTOPASCAL:
		switch (srcType) {
		case MessageType::HECTOPASCAL:
			dst.airPressure = src.airPressure;
			break;
		default:
			// conversion failed
			return false;
		}
		break;

	case MessageType::OHM:
		switch (srcType) {
		case MessageType::OHM:
			dst.resistance = src.resistance;
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
