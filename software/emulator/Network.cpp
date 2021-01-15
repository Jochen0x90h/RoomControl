#include "Network.hpp"
#include <iostream>


inline uint16_t getUShort(uint8_t const *buffer) {
	return (buffer[0] << 8) + buffer[1];
}


Network::Network()
	: emulatorSocket(global::context, global::local)
{
	// post onConnectedUp() into event loop
	boost::asio::post(global::context, [this] {
		onUpConnected();
	});
	
	// start receiving
	receive();
}

Network::~Network() {
}

void Network::upSend(const uint8_t *data, int length) {
	assert(length <= sizeof(this->upSendBuffer));
	this->upSendBuffer[0] = length + 1;
	memcpy(this->upSendBuffer + 1, data, length);
	this->upSendBusy = true;
	this->emulatorSocket.async_send_to(boost::asio::buffer(this->upSendBuffer, length + 1), global::upLink,
		[this] (const boost::system::error_code &error, std::size_t sentLength) {
			//std::cout << "sent " << sentLength << " bytes " << error.message() << std::endl;
			this->upSendBusy = false;
			onUpSent();
		}
	);
}

void Network::downSend(uint16_t clientId, const uint8_t *data, int length) {
	assert(length <= sizeof(this->downSendBuffer));
	this->downSendBuffer[0] = length + 1;
	memcpy(this->downSendBuffer + 1, data, length);
	this->downSendBusy = true;
	asio::ip::udp::endpoint endpoint = this->endpoints.right.at(clientId);
	this->emulatorSocket.async_send_to(boost::asio::buffer(this->downSendBuffer, length + 1), endpoint,
		[this] (const boost::system::error_code &error, std::size_t sentLength) {
			//std::cout << "sent " << sentLength << " bytes " << error.message() << std::endl;			
			this->downSendBusy = false;
			onDownSent();
		}
	);
}

void Network::receive() {
	//std::cout << "receive" << std::endl;
	this->emulatorSocket.async_receive_from(boost::asio::buffer(this->receiveBuffer, 256), this->sender,
		[this] (const boost::system::error_code& error, std::size_t receivedLength) {
			//std::cout << "received " << receivedLength << " bytes from " << this->sender << std::endl;
			
			bool up = this->sender == global::upLink;
			//if (up)
			//	std::cout << "received from uplink" << std::endl;
			
			// get message length
			int messageLength = this->receiveBuffer[0];

			// check for extended length
			if (messageLength == 1) {
				messageLength = getUShort(this->receiveBuffer + 1);

				// check if message has valid length and is complete
				if (messageLength >= 4 && messageLength <= receivedLength) {
					if (up) {
						onUpReceived(this->receiveBuffer + 3, messageLength - 3);
					} else {
						auto p = this->endpoints.left.insert({sender, this->next});
						if (p.second)
							++this->next;
						onDownReceived(p.first->second, this->receiveBuffer + 3, messageLength - 3);
					}
				} else {
					// error: message not complete
				}
			} else {
				// check if message has valid length and is complete
				if (messageLength >= 2 && messageLength <= receivedLength) {
					if (up) {
						onUpReceived(this->receiveBuffer + 1, messageLength - 1);
					} else {
						auto p = this->endpoints.left.insert({sender, this->next});
						if (p.second)
							++this->next;
						onDownReceived(p.first->second, this->receiveBuffer + 1, messageLength - 1);
					}
				} else {
					// error: message not complete
				}
			}
			
			// continue receiving
			receive();
		}
	);
}


