#pragma once

#include "Network.hpp"
#include "SystemTimer.hpp"
#include "String.hpp"

// MQTT-SN helper functions

namespace mqttsn {

enum MessageType {
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

enum class TopicType {
	NORMAL = 0, // topic id in publish, topic name in subscribe
	PREDEFINED = 1,
	SHORT = 2
};

enum class ReturnCode {
	ACCEPTED,
	REJECTED_CONGESTED,
	REJECTED_INVALID_TOPIC_ID,
	NOT_SUPPORTED
};

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

}



/**
 * MQTT-SN client inspired by C implementation by Nordic Semiconductor
 * https://www.oasis-open.org/committees/download.php/66091/MQTT-SN_spec_v1.2.pdf
 * Inherits platform dependent (hardware or emulator) components for network and timing
 * Gateway search is removed as we know that our gateway is always on the other side of the up-link
 */
class MqttSnClient : public SystemTimer, public Network {
public:

	// Port the MQTT-SN client binds to
	//static constexpr int CLIENT_PORT = 47194;

	// Port of the gateway
	//static constexpr int GATEWAY_PORT = 47193;

	// Default retransmission time
	static constexpr SystemDuration RETRANSMISSION_INTERVAL = 8s;

	// The MQTT broker disconnects us if we don't send anything in one and a half times the keep alive interval
	static constexpr SystemDuration KEEP_ALIVE_INTERVAL = 60s;

	// Default time in seconds for which MQTT-SN Client is considered alive by the MQTT-SN Gateway
	//static constexpr int DEFAULT_ALIVE_DURATION = 60;

	// Default time in seconds for which MQTT-SN Client is considered asleep by the MQTT-SN Gateway
	//static constexpr int DEFAULT_SLEEP_DURATION = 30;

	// Maximum length of Client ID according to the protocol spec in bytes
	static constexpr int MAX_CLIENT_ID_LENGTH = 23;

	// Maximum length of a message
	static constexpr int MAX_MESSAGE_LENGTH = 64;

	// Maximum length of will feature buffers. For internal use only
	static constexpr int WILL_TOPIC_MAX_LENGTH = 32;
	static constexpr int WILL_MSG_MAX_LENGTH = 32;


	// maximum number of queued messages
	static constexpr int MAX_SEND_COUNT = 128; // must be power of 2
	
	// size of message buffer
	static constexpr int SEND_BUFFER_SIZE = 2048;


	// error types
	
	// error parsing the message, e.g. too short
	static constexpr int ERROR_MESSAGE = 0;
	
	// gateway returned mqttsn::ReturnCode::REJECTED_CONGESTED
	static constexpr int ERROR_CONGESTED = 1;
	
	// message type is not supported
	static constexpr int ERROR_UNSUPPORTED = 2;
	

	// request result
	enum class Result {
		// method executed ok
		OK,
		
		// invalid method parameter
		INVALID_PARAMETER,
		
		// method invalid in current client state
		INVALID_STATE,
		
		// client is busy processing a request
		BUSY,
		
		// out of memory, e.g. send buffer full or no more space for topics
		OUT_OF_MEMORY
	};

	struct MessageResult {
		Result result;
		uint16_t msgId;
	};

	// state of this MQTT-SN client
	enum class State : uint8_t
	{
		STOPPED,
		//SEARCHING_GATEWAY,
		CONNECTING,
		CONNECTED,
		DISCONNECTING,
		ASLEEP,
		AWAKE,
		WAITING_FOR_SLEEP
	};
	
	struct Topic
	{
		const char *topicName;
		uint16_t topicId;
	};
	

	MqttSnClient();

	~MqttSnClient() override;

	/**
	 * Get state of the client
	 */
	State getState() {return this->state;}
	bool isStopped() {return this->state == State::STOPPED;}
	bool isConnecting() {return this->state == State::CONNECTING;}
	bool isConnected() {return this->state == State::CONNECTED;}
	bool isAsleep() {return this->state == State::ASLEEP;}
	bool isAwake() {return this->state == State::AWAKE;}
	bool canConnect() {return isStopped() || isAsleep() || isAwake();}

	/**
	 * Returns true if send queue is full
	 */
	bool isBusy() {return this->busy;}

	/**
	 * Start searching for a gateway. When a gateway is found, onGatewayFound() gets called
	 */
	//Result::Status searchGateway();
	
	/**
	 * Connect to a gateway, e.g. in onGatewayFound() when an advertisement of a gateway has arrived
	 * @param gateway gateway ip address, scope and port
	 * @param gatewayId id of gateway
	 * @param clientName client name
	 * @param cleanSession clean session flag
	 * @param willFlag if set, the gateway will request the will topic and will message
	 */
	Result connect(/*Udp6Endpoint const &gateway, uint8_t gatewayId,*/ String clientName,
		bool cleanSession = false, bool willFlag = false);
	
	/**
	 * Disconnect from gateway
	 */
	Result disconnect();
	
	//Result sleep();

	/**
	 * Send a ping to the gateway
	 * @return result containing success status and message id
	 */
	Result ping();


	/**
	 * Register a topic at the gateway. On success, onRegistered() gets called with the topicId to use in publish()
	 * @param topicName topic name (path without wildcards)
	 * @return result containing success status and message id
	 */
	MessageResult registerTopic(String topicName);

	/**
	 * Publish a message on a topic
	 * @param topicId topic (or message) id obtained using registerTopic() and onRegisterd()
	 * @param data payload data
	 * @param length payload data length
	 * @param qos quality of service level: 0, 1, 2 or -1. Must not be higher than granted qos for the topic
	 * @param retain retain message and deliver to new subscribers
	 * @param waitForTopicId true if we publish using the message id of registerTopic() before onRegistered() arrived
	 * @return result result code
	 */
	Result publish(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain = false,
		bool waitForTopicId = false);

	Result publish(uint16_t topicId, String data, int8_t qos, bool retain = false, bool waitForTopicId = false) {
		return publish(topicId, reinterpret_cast<uint8_t const*>(data.data), data.length, qos, retain, waitForTopicId);
	}

	/**
	 * Subscribe to a topic. On success, onSubscribed() is called with a topicId that is used in subsequent onPublish() calls
	 * @param topicFilter topic filter (path that may contain wildcards)
	 * @param qos requeted quality of service level of the subscription, granted level gets passed to onSubscribed()
	 * @return result containing success status and message id
	 */
	MessageResult subscribeTopic(String topicFilter, int8_t qos = 1);

	/**
	 * Unsubscribe from a topic. On success, onUnsubscribed() is called
	 * @param topicFilter topic filter (path that may contain wildcards)
	 * @return result containing success status and message id
	 */
	Result unsubscribeTopic(String topicFilter);

	//Result updateWillTopic();
	//Result updateWillMessage();
	
	/**
	 * Check if a topic is valid
	 * @return ture if topic is valid
	 */
	static bool isValid(String topic) {return uint32_t(topic.length - 1) <= uint32_t(MAX_MESSAGE_LENGTH - 5 - 1);}

protected:
	
// user callbacks
// --------------
		
	// Client has found a gateway
	//virtual void onGatewayFound(Udp6Endpoint const &sender, uint8_t gatewayId) = 0;

	/**
	 * Client has connected to the gateway. Now subscriptions can be made using subscribeTopic().
	 * When using the broker, all subscriptions have to be renewed
	 */
	virtual void onConnected() = 0;
	
	// Client was disconnected by the gateway
	virtual void onDisconnected() = 0;
	
	// Client is allowed to sleep
	virtual void onSleep() = 0;
	
	// Client should wake up
	virtual void onWakeup() = 0;	
			
	// Client has received a topic to register (when registerTopic() was called with wildcard topic)
	//virtual void onRegister(String topic, uint16_t topicId) = 0;

	/**
	 * Client has registered a topic
	 * @param msgId message id returned by subscribe()
	 * @param topicName topic name (path without wildcards). Only valid as long as no new messages are sent
	 * @param topicId topic id to use in onPublish()
	 */
	virtual void onRegistered(uint16_t msgId, String topicName, uint16_t topicId) = 0;

	/**
	 * Client as subscribed to a topic
	 * @param msgId message id returned by subscribe()
	 * @param topicName topic name (path without wildcards). Only valid as long as no new messages are sent
	 * @param topicId topic id to use in onPublish() or 0 if subscribed topic contains wildcards
	 * @param qos granted quality of service level
	 */
	virtual void onSubscribed(uint16_t msgId, String topicName, uint16_t topicId, int8_t qos) = 0;
	
	/**
	 * Gateway publishes a message on a subscribed topic
	 * @return return code
	 */
	virtual mqttsn::ReturnCode onPublish(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) = 0;

	/**
	 * Error has occurred
	 * @param error type
	 * @param messageType message type when known
	 */
	virtual void onError(int error, mqttsn::MessageType messageType) = 0;

protected:

// transport
// ---------

	void onUpReceived(uint8_t const *data, int length) override;

	void onUpSent() override;

	void onSystemTimeout1(SystemTime time) override;
    
private:

// internal
// --------

	struct MessageInfo {
		// offset in message buffer
		uint16_t offset;

		// length of message
		uint8_t length;
		
		// true for PUBLISH messages that wait for the topic id to be registered
		bool waitForTopicId;

		// message id
		uint16_t msgId;
		
		// time when the message was sent the first time
		SystemTime sentTime;
	};

	struct Message {
		uint8_t *data;
		int length;
	};

	// Get a message id. Use only for retries of the same message until it was acknowledged by the gateway
	uint16_t getNextMsgId() {return this->nextMsgId = this->nextMsgId == 0xffff ? 1 : this->nextMsgId + 1;}

	// allocate a send message with given length or 0 if no space available
	Message addSendMessage(int length, uint16_t msgId = 0, bool waitForTopicId = false);

	// send the current message and make next message current or try to garbage collect it if msgId is zero
	void sendCurrentMessage();
	
	// resend oldest message that is still in the queue
	void resend();

	// set topic id of PUBLISH messages that have waitForTopicId = true
	void setTopicId(uint16_t msgId, uint16_t topicId);

	// remove sent message with given id from message queue
	Message removeSentMessage(uint16_t msgId, mqttsn::MessageType type);
	
	// state
	State state = State::STOPPED;
		
	// gateway
	//uint8_t gatewayId;
	//Udp6Endpoint gateway;

	// id for next message to gateway to be able to associate the reply with a request
	uint16_t nextMsgId = 0;
	
	// send message queue
	int sendMessagesHead = 0;
	int sendMessagesCurrent = 0;
	int sendMessagesTail = 0;
	MessageInfo sendMessages[MAX_SEND_COUNT];
	int sendBufferHead = 0;
	int sendBufferFill = 0;
	uint8_t sendBuffer[SEND_BUFFER_SIZE];
	bool busy = false;

	// receive message buffer
	//Udp6Endpoint sender;
	//uint8_t receiveMessage[MAX_MESSAGE_LENGTH];
		
	bool resendPending = false;
};
