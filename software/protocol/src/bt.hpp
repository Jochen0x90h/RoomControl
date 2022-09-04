#pragma once

#include <cstdint>
#include "enum.hpp"


namespace bt {

namespace att {

enum class Op : uint8_t {
	ErrorRsp = 0x01,

	ExchangeMtuReq = 0x02,
	ExchangeMtuRsp = 0x03,

	FindInformationReq = 0x04,
	FindInformationRsp = 0x05,
	FindByTypeValueReq = 0x06,
	FindByTypeValueRsp = 0x07,

	ReadByTypeReq = 0x08,
	ReadByTypeRsp = 0x09,
	ReadReq = 0x0a,
	ReadRsp = 0x0b,
	ReadBlobReq = 0x0c,
	ReadBlobRsp = 0x0d,
	ReadByGroupTypeReq = 0x10,
	ReadByGroupTypeRsp = 0x11,

	WriteReq = 0x12,
	WriteRsp = 0x13,
	WriteCmd = 0x52,
	SignedWriteCmd = 0xc2,

	// notification containing handle and value
	HandleValueNtf = 0x1B,
	// indication containing handle and value
	HandleValueInd = 0x1D,
	// confirmation for ATT_HANDLE_VALUE_IND
	HandleValueCfm = 0x1E,
};

} // namespace att

namespace gatt {

// attribute types
enum class Type : uint16_t {
	PrimaryService = 0x2800,
	SecondarySerice = 0x2801,
	IncludeService = 0x2802,
	Characteristic = 0x2803,
	SharacteristicExtendedProperties = 0x2900,
	CharacteristicUserDescription = 0x2901,
	ClientCharacteristicConfiguration = 0x2902,
	ServerCharacteristicConfiguration = 0x2903,
	CharacteristicPresentationFormat = 0x2904,
	CharacteristicAggregateFormat = 0x2904,
};

// characteristic properties
enum class CharacteristicProperties : uint8_t {
	Braodcast = 0x01,
	Read = 0x02,
	WriteWithoutResponse = 0x04,
	Write = 0x08,
	Notify = 0x10,
	Indicate = 0x20,
	AuthenticatedSignedWrites = 0x40,
	ExtendedProperties = 0x80
};
FLAGS_ENUM(CharacteristicProperties)

} // namespace gatt

} // namespace bt
