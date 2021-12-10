#pragma once


namespace mqttsn {

enum class MessageType : uint8_t {
	ADVERTISE = 0x00,
	SEARCHGW = 0x01,
	GWINFO = 0x02,
	CONNECT = 0x04,
	CONNACK = 0x05,
	WILLTOPICREQ = 0x06,
	WILLTOPIC = 0x07,
	WILLMSGREQ = 0x08,
	WILLMSG = 0x09,
	REGISTER = 0x0a,
	REGACK = 0x0b,
	PUBLISH = 0x0c,
	PUBACK = 0x0d,
	PUBCOMP = 0x0e,
	PUBREC = 0x0f,
	PUBREL = 0x10,
	SUBSCRIBE = 0x12,
	SUBACK = 0x13,
	UNSUBSCRIBE = 0x14,
	UNSUBACK = 0x15,
	PINGREQ = 0x16,
	PINGRESP = 0x17,
	DISCONNECT = 0x18,
	WILLTOPICUPD = 0x1a,
	WILLTOPICRESP = 0x1b,
	WILLMSGUPD = 0x1c,
	WILLMSGRESP = 0x1d,
	ENCAPSULATED = 0xfe,
	//UNKNOWN_MESSAGE_TYPE = 0xff
};

enum class Flags : uint8_t {
	TOPIC_TYPE_MASK = 0x03,
	TOPIC_TYPE_NORMAL = 0x00,
	TOPIC_TYPE_PREDEFINED = 0x01,
	TOPIC_TYPE_SHORT = 0x02,
	
	CLEAN_SESSION = 0x04,

	WILL = 0x08,
	
	RETAIN = 0x10,
	
	QOS_MASK = 0x60,
	QOS_0 = 0x00,
	QOS_1 = 0x20,
	QOS_2 = 0x40,
	QOS_MINUS_1 = 0x60,

	DUP = 0x80
};
FLAGS_ENUM(Flags);

inline int8_t getQos(Flags flags) {
	int qos = uint8_t(flags) >> 5;
	return ((qos + 1) & 3) - 1;
}

inline Flags makeQos(int8_t qos) {
	return Flags((qos & 3) << 5);
}


/*
enum class TopicType : uint8_t {
	NORMAL = 0, // topic id in publish, topic name in subscribe
	PREDEFINED = 1,
	SHORT = 2
};
*/

enum class ReturnCode : uint8_t {
	ACCEPTED,
	REJECTED_CONGESTED,
	REJECTED_INVALID_TOPIC_ID,
	NOT_SUPPORTED
};

/*
inline bool getDup(uint8_t flags) {
	return (flags & (1 << 7)) != 0;
}

inline int8_t getQos(uint8_t flags) {
	int8_t qos = flags >> 5;
	return ((qos + 1) & 3) - 1;
}

inline bool getRetain(uint8_t flags) {
	return (flags & (1 << 4)) != 0;
}

inline TopicType getTopicType(uint8_t flags) {
	return TopicType(flags & 3);
}

inline uint16_t getUShort(uint8_t const *buffer) {
	return (buffer[0] << 8) + buffer[1];
}


inline uint8_t makeQos(int8_t qos) {
	return (qos & 3) << 5;
}

inline uint8_t makeRetain(bool retain) {
	return uint8_t(retain) << 4;
}

inline uint8_t makeTopicType(TopicType topicType) {
	return uint8_t(topicType);
}

inline void setUShort(uint8_t *buffer, uint16_t value) {
	buffer[0] = uint8_t(value >> 8);
	buffer[1] = uint8_t(value);
}
*/

} // namespace mqttsn
