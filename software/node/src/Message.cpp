#include "Message.hpp"
//#include <Terminal.hpp>
//#include <StringOperators.hpp>


using PlugType = bus::PlugType;
#include "functions.generated.cpp"


bool convertSwitch(MessageType dstType, Message &dst, uint8_t src, ConvertOptions const &convertOptions) {
	if (src >= 3)
		return false;

	switch (dstType & PlugType::CATEGORY) {
	case PlugType::BINARY:
	case PlugType::TERNARY: {
		// switch -> switch
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		dst.value.u8 = cmd;
		break;
	}
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION: {
		// switch -> float, use values in convertOptions
		dst.value.f32 = convertOptions.value.f[src];
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		if ((dstType & PlugType::CMD) != 0) {
			dst.command = cmd;
		}
		break;
	}
	case PlugType::LIGHTING: {
		// switch -> float, use values in convertOptions
		dst.value.f32 = convertOptions.value.f[src];
		int cmd = convertOptions.getCommand(src);
		if (cmd >= 3)
			return false; // conversion failed
		if ((dstType & PlugType::CMD) != 0) {
			dst.command = cmd;
			dst.transition = convertOptions.transition;
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

bool convertInt8(MessageType dstType, Message &dst, int src, ConvertOptions const &convertOptions) {
	switch (dstType & PlugType::CATEGORY) {
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION: {
		// int -> float, use value in convertOptions
		dst.value.f32 = float(src) * convertOptions.value.f[0];
		if ((dstType & PlugType::CMD) == 0)
			return false; // conversion failed, destination must support command
		dst.command = 1;
		break;
	}
	case PlugType::LIGHTING: {
		// int -> float, use value in convertOptions
		dst.value.f32 = float(src) * convertOptions.value.f[0];
		if ((dstType & PlugType::CMD) == 0)
			return false; // conversion failed, destination must support command
		dst.command = 1;
		dst.transition = convertOptions.transition;
		break;
	}
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertFloat(MessageType dstType, Message &dst, float src, ConvertOptions const &convertOptions) {
	switch (dstType & PlugType::CATEGORY) {
	case PlugType::BINARY:
	case PlugType::TERNARY: {
		// float -> command, compare against threshold values
		float upper = convertOptions.value.f[0];
		float lower = convertOptions.value.f[1];
		int compare = src > upper ? 0 : (src < lower ? 1 : 2);
		int c = convertOptions.getCommand(compare);
		if (c >= 3)
			return false; // conversion failed
		dst.value.u8 = c;
		break;
	}
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION:
		dst.value.f32 = src;
		if ((dstType & PlugType::CMD) != 0) {
			dst.command = 0; // set
		}
		break;
	case PlugType::LIGHTING:
		dst.value.f32 = src;
		if ((dstType & PlugType::CMD) != 0) {
			dst.command = 0; // set
			dst.transition = 0;
		}
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertFloatCommand(MessageType dstType, Message &dst, float src, uint8_t command,
	ConvertOptions const &convertOptions)
{
	// sanity check
	if ((dstType & PlugType::CMD) == 0)
		return false;

	switch (dstType & PlugType::CATEGORY) {
	case PlugType::LEVEL:
	case PlugType::PHYSICAL:
	case PlugType::CONCENTRATION:
		dst.value.f32 = src;
		dst.command = command;
		break;
	case PlugType::LIGHTING:
		dst.value.f32 = src;
		dst.command = command;
		dst.transition = 0;
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

bool convertFloatTransition(MessageType dstType, Message &dst, float src, uint8_t command, uint16_t transition,
	ConvertOptions const &convertOptions)
{
	// sanity check
	if ((dstType & PlugType::CMD) == 0)
		return false;

	switch (dstType & PlugType::CATEGORY) {
	case PlugType::LIGHTING:
		dst.value.f32 = src;
		dst.command = command;
		dst.transition = transition;
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}
