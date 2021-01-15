#pragma once

#include "global.hpp"
#include <boost/bimap.hpp>
#include "util.hpp"


/**
 * Network of nodes that form a spanning tree. Therefore there is one up-link to the parent node and multiple down-links to child nodes.
 * The transported data are "naked" mqtt-sn packets without length, starting with the packet type.
 * This emulator implementation uses ipv6 and boost::asio on linux/macOS
 */
class Network {
public:

	/**
	 * Constructor
	 * @param parameters platform dependent parameters
	 */
	Network();

	virtual ~Network();

	/**
	 * Notify that a the up-link has been established
	 */
	virtual void onUpConnected() = 0;

	/**
	 * Called when a message was received from the gateway
	 * @param data message data
	 * @param length message length
	 */
	virtual void onUpReceived(const uint8_t *data, int length) = 0;

	/**
	 * Send a message to the gateway. When finished, onUpSent() gets called
	 * @param data message data gets copied and can be discarded immediately after upSend() returns
	 * @param length message length
	 */
	void upSend(const uint8_t *data, int length);

	/**
	 * Called when send operation over up-link has finished
	 */
	virtual void onUpSent() = 0;

	/**
	 * Called when a message was received from a child node
	 * @param clientId sender of the message
	 * @param data message data
	 * @param length message length
	 */
	virtual void onDownReceived(uint16_t clientId, uint8_t const *data, int length) = 0;

	/**
	 * Send a message to a child node. When finished, onDownSent() gets called
	 * @param clientId receiver of the message
	 * @param data message data gets copied and can be discarded immediately after downSend() returns
	 * @param length message length
	 */
	void downSend(uint16_t clientId, uint8_t const *data, int length);

	/**
	 * Called when send operation over down-link has finished
	 */
	virtual void onDownSent() = 0;

	/**
	 * Returns true when sending over up-link is in progress
	 */
	bool isUpSendBusy() {return this->upSendBusy;}

	/**
	 * Returns true when sending over down-link is in progress
	 */
	bool isDownSendBusy() {return this->downSendBusy;}

private:
	void receive();

	asio::ip::udp::socket emulatorSocket;
	asio::ip::udp::endpoint sender;
	uint8_t receiveBuffer[256];
	
	uint8_t upSendBuffer[256];
	uint8_t downSendBuffer[256];
	bool upSendBusy = false;
	bool downSendBusy = false;

	// list of downlinks
	uint16_t next = 1;
	boost::bimap<asio::ip::udp::endpoint, uint16_t> endpoints;
};
