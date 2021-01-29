#pragma once

#include "MqttSnClient.hpp"
#include "StringBuffer.hpp"


/**
 * MQTT-SN broker that serves down-links and connects to a gateway using MqttSnClient
 * Wireless implementation of should prioritize sending via DownLink over sending via UpLink to prevent congestion of broker
 */
class MqttSnBroker : public MqttSnClient {
public:
	// maximum number of remote clients that can connect
	static constexpr int MAX_CLIENT_COUNT = 31;
	static constexpr int LOCAL_CLIENT_INDEX = MAX_CLIENT_COUNT;

	// maximum number of topics that can be handled
	static constexpr int MAX_TOPIC_COUNT = 1024;
	
	// size of buffer for retained messages
	static constexpr int RETAINED_BUFFER_SIZE = 8192;
	
	

	// error types
	
	// error parsing the broker message, e.g. too short
	static constexpr int ERROR_BROKER_MESSAGE = 3;

	// client returned mqttsn::ReturnCode::REJECTED_CONGESTED
	static constexpr int ERROR_BROKER_CONGESTED = 4;

	// message type for broker is not supported
	static constexpr int ERROR_BROKER_UNSUPPORTED = 5;


	struct TopicResult {
		uint16_t topicId;
		Result result;
	};


	MqttSnBroker();

	/**
	 * Returns true if send queue to gateway is full
	 */
	bool isClientBusy() {return MqttSnClient::isBusy();}

	/**
	 * Returns true if send queue to clients of this broker is full
	 */
	bool isBrokerBusy() {return this->busy;}

	/**
	 * Register a topic at this broker and its gateway if connected.
	 * @param topicId receives the topic id. Must be the same id if it was non-zero
	 * @param topicName topic name (path without wildcards)
	 * @return result of registering the topic at the broker
	 */
	Result registerTopic(uint16_t &topicId, String topicName);

	/**
	 * Unregister a topic from this broker (currently not supported)
	 * @param topicId topic id to unregister, gets set to zero
	 */
	Result unregisterTopic(uint16_t &topicId) {
		assert(topicId != 0);
		topicId = 0;
		return Result::OK;
	}

	/**
	 * Publish a message on a topic
	 * @param topicId topic id obtained using registerTopic()
	 * @param data payload data
	 * @param length payload data length
	 * @param qos quality of service: 0, 1, 2 or -1
	 * @param retain retain message and deliver to new subscribers
	 * @return result result code
	 */
	Result publish(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain = false);

	Result publish(uint16_t topicId, String data, int8_t qos, bool retain = false) {
		return publish(topicId, reinterpret_cast<uint8_t const*>(data.data), data.length, qos, retain);
	}

	/**
	 * Subscribe to a topic. Multiple subscriptions to the same topic are reference counted
	 * @param topic topic filter (path that may contain wildcards)
	 * @param qos quality of service of the subscription, qos of a published message is minimum of publishing and subscription
	 * @return topic id and result of registering at gateway
	 */
	//TopicResult subscribeTopic(String topicFilter, int8_t qos);
	
	/**
	 * Subscribe to a topic at this broker and its gateway if connected. Multiple subscriptions to the same topic are
	 * reference counted.
	 * @param topicId receives the topic id. A new reference is counted if it was zero, otherwise it must be the same id
	 * @param topic topic filter (path that may contain wildcards)
	 * @param qos quality of service of the subscription, qos of a published message is minimum of publishing and subscription
	 * @return result of subscribung to the topic at the broker
	 */
	Result subscribeTopic(uint16_t &topicId, String topicFilter, int8_t qos);

	/**
	 * Increment the reference counter of an existing subscription
	 * @param topicId topic id of existing subscription. Nothing happens if topicId is zero
	 */
	void addSubscriptionReference(uint16_t topicId) {
		if (topicId != 0)
			++getTopic(topicId).subscribeCount;
	}

	/**
	 * Unsubscribe from a topic.
	 * @param topicId topic id, must match topicFilter, gets set to zero
	 * @param topicFilter topic filter (path that may contain wildcards)
	 * @return result containing success status and message id
	 */
	Result unsubscribeTopic(uint16_t &topicId, String topicFilter);

protected:

// user callbacks
// --------------

	/**
	 * A message was published on a topic. Does not get called for messages with zero length
	 * @param topicId id of topic
	 * @param data message data
	 * @param length message length, at least 1
	 * @param qos quality of service
	 * @param retain true if this is a retained message
	 * @return return code
	 */
	virtual void onPublished(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) = 0;

protected:

// MqttSnClient user callbacks
// ---------------------------
	
	void onRegistered(uint16_t msgId, String topicName, uint16_t topicId) override;

	void onSubscribed(uint16_t msgId, String topicName, uint16_t topicId, int8_t qos) override;

	mqttsn::ReturnCode onPublish(uint16_t topicId, uint8_t const *data, int length, int8_t qos, bool retain) override;

protected:

// transport
// ---------

	void onDownReceived(uint16_t clientId, uint8_t const *data, int length) override;
	
	void onDownSent() override;

	void onSystemTimeout2(SystemTime time) override;
    
private:

// internal
// --------	
	
	struct ClientInfo {
		// client id used by the down-link
		uint16_t clientId;
		
		// index of this client info
		uint16_t index;
		
		// client name
		StringBuffer<32> name;
		
		void disconnect() {
			this->clientId = 0;
			this->name.clear();
		}
	};
	
	struct TopicInfo {
		// hash of topic string or zero for unused entry
		uint32_t hash;
		
		// retained message
		uint16_t retainedOffset;
		uint8_t retainedAllocated;
		uint8_t retainedLength;

		// for each client two bits that indicate subscription qos (0-2: qos, 3: not subscribed)
		uint32_t clientQosArray[(MAX_CLIENT_COUNT + 1 + 15) / 16];

		// topic id at gateway (broker at other side of up-link)
		uint16_t gatewayTopicId;
		
		// quality of service level granted by gateway
		int8_t gatewayQos;
		
		// reference counter for number of subscriptions
		uint8_t subscribeCount;
		
		// return true if at least one remote client has subscribed to the topic
		bool isRemoteSubscribed();
		
		// get maximum quality of service of all remote client subscriptions
		int8_t getMaxQos();
		
		int8_t getQos(int clientIndex) {
			int shift = (clientIndex << 1) & 31;
			return (((this->clientQosArray[clientIndex >> 4] >> shift) + 1) & 3) - 1;
		}
		
		void setQos(int clientIndex, int8_t qos);

		// Clear qos of a client and return previous value
		int8_t clearQos(int clientIndex);
	};

	union ClientSet {
		// set contains only one client
		uint16_t clientId;
		
		// set contains multiple clients indicated by flags referencing the clients array
		uint32_t flags[(MAX_CLIENT_COUNT + 1 + 31) / 32];
	};
	
	struct MessageInfo {
		// a flag for each client in clients array. If bit 0 is set, a client id is in bits 1-16
		ClientSet clientSet;
		
		// offset in message buffer
		uint16_t offset;

		// length of message
		uint16_t length;

		// message id
		uint16_t msgId;
		
		// time when the message was sent the first time
		SystemTime sentTime;
	};

	struct Message {
		uint8_t *data;
		//int length;
	};

	// Find client by id. Returns nullptr if not found
	ClientInfo *findClient(uint16_t clientId);

	/**
	 * Get or add topic by name
	 * @param name topic name (path without wildcards)
	 * @return 0 if no new topic could be allocated
	 */
	uint16_t getTopicId(String name, bool add = true);

	/**
	 * Get topic by valid id
	 * @param topicId valid topic id, must not be 0
	 * @return topic info
	 */
	TopicInfo &getTopic(uint16_t topicId) {return this->topics[topicId - 1];}

	// Find topic by id. Returns nullptr if not found
	TopicInfo *findTopic(uint16_t topicId);

	// Find topic by gateway topic id. Returns nullptr if not found
	uint16_t findTopicId(uint16_t gatewayTopicId);

	
	// Insert space into buffer for retained messages
	uint16_t insertRetained(int offset, int length);

	// Insert space into buffer for retained messages
	uint16_t eraseRetained(int offset, int length);


	// Get a message id. Use only for retries of the same message until it was acknowledged by the gateway
	uint16_t getNextMsgId() {return this->nextMsgId = this->nextMsgId == 0xffff ? 1 : this->nextMsgId + 1;}

	// allocate a send message with given length or 0 if no space available
	Message addSendMessage(int length, ClientSet clientSet, uint16_t msgId = 0);

	// send the current message and make next message current or try to garbage collect it if msgId is zero
	void sendCurrentMessage();

	// resend oldest message that is still in the queue
	void resend();

	// remove sent message with given id from message queue
	void removeSentMessage(uint16_t msgId, uint16_t clientId = MAX_CLIENT_COUNT);
	
	// send publish message to gateway, clients and local client
	void sendPublish(uint16_t topicId, TopicInfo *topic, uint8_t const *data, int length, int8_t qos, bool retain,
		uint16_t excludeClientId = 0);
		
	// send a disconnect message to a client
	void sendDisconnect(uint16_t clientId);
	

	// clients
	ClientInfo clients[MAX_CLIENT_COUNT];
	
	// topics
	int topicCount = 0;
	std::map<std::string, uint16_t> topicNames;
	TopicInfo topics[MAX_TOPIC_COUNT];
	
	// space retained messages
	int retainedSize = 0;
	uint8_t retained[RETAINED_BUFFER_SIZE];

	// id for next message to gateway to be able to associate the reply with a request
	uint16_t nextMsgId = 0;

	// send message queue
	int sendMessagesHead = 0;
	int sendMessagesCurrent = 0;
	int sendMessagesClientIndex = 0;
	int sendMessagesTail = 0;
	MessageInfo sendMessages[MAX_SEND_COUNT];
	int sendBufferHead = 0;
	int sendBufferFill = 0;
	uint8_t sendBuffer[SEND_BUFFER_SIZE];
	bool busy = false;

	bool resendPending = false;
};
