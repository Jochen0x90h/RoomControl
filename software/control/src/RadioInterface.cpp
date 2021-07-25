#include "RadioInterface.hpp"
#include <radio.hpp>
#include <rng.hpp>
#include <timer.hpp>
#include <crypt.hpp>
#include <Nonce.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <gp.hpp>
#include <PacketReader.hpp>
#include <PacketWriter.hpp>
#include <Pointer.hpp>
#include <util.hpp>
#include <iostream>



namespace SendFlags {
	constexpr int BEACON = 1;
	constexpr int LINK_STATUS = 2;
}

static AesKey const trustCenterLinkAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};
static AesKey const hashedTrustCenterLinkAesKey = {{0x4bab0f17, 0x3e1434a2, 0xd572e1c1, 0xef478782, 0xeabc1cc8, 0xd4a8286a, 0x01dac9ab, 0xee9d4e29, 0xb693b9e0, 0x623b918a, 0x63e15821, 0x8d7c1608, 0xa2d489bd, 0xc0ef1837, 0xa30e4016, 0x2e72561e, 0xea65fb8c, 0x2a8ae3bb, 0x8984a3ad, 0xa7f6f5b3, 0xb88396d0, 0x9209756b, 0x1b8dd6c6, 0xbc7b2375, 0xb9a50bb5, 0x2bac7ede, 0x3021a818, 0x8c5a8b6d, 0x479837d1, 0x6c34490f, 0x5c15e117, 0xd04f6a7a, 0x439aeda1, 0x2faea4ae, 0x73bb45b9, 0xa3f42fc3, 0xe78fc3ab, 0xc8216705, 0xbb9a22bc, 0x186e0d7f, 0x4e581106, 0x86797603, 0x3de354bf, 0x258d59c0}};
constexpr zb::SecurityControl securityLevel = zb::SecurityControl::LEVEL_ENC_MIC32; // encrypted + 32 bit message integrity code


enum class Role : uint8_t {
	// client sends (cluster specific) commmands to manipulate attributes in server
	CLIENT,
	
	// server
	SERVER
};

struct EndpointInfo {
	EndpointType endpointType;
	Role role;
	bool bind;
	//zb::ZclProfile profile;
	zb::ZclCluster cluster;
	uint16_t attribute;
	zb::ZclDataType dataType;
};

EndpointInfo const endpointInfos[] {
	//{EndpointType::BATTERY_PERCENTAGE, EndpointMode::INPUT, zb::ZclProfile::HOME_AUTOMATION, zb::ZclCluster::POWER_CONFIGURATION},
	//{EndpointType::SWITCH, EndpointMode::COMMAND, zb::ZclProfile::HOME_AUTOMATION, zb::ZclCluster::ON_OFF},

	// battery percentage 0-100% in 0.5% steps, i.e. value is 0-200
	{EndpointType::BATTERY_PERCENTAGE, Role::SERVER, false/*true*/, zb::ZclCluster::POWER_CONFIGURATION, zb::ZclPowerConfigurationAttribute::BATTERY_PERCENTAGE, zb::ZclDataType::UINT8},
	
	// wall switch
	{EndpointType::SWITCH, Role::CLIENT, true, zb::ZclCluster::ON_OFF, zb::ZclOnOffAttribute::ON_OFF, zb::ZclDataType::UINT8},
	
	// power on/off, e.g. relay
	{EndpointType::RELAY, Role::SERVER, false, zb::ZclCluster::ON_OFF, zb::ZclOnOffAttribute::ON_OFF, zb::ZclDataType::UINT8},
};




RadioInterface::RadioInterface(Storage::Array<Configuration, void> &configuration,
	std::function<void (uint8_t, uint8_t const *, int)> const &onReceived)
	: configuration(configuration), onReceived(onReceived)//, panId(0xffff)
{
	// note: configuration may not be valid yet

	// set short address of coordinator
	radio::setShortAddress(RADIO_ZB, 0x0000);

	// filter packets with the coordinator as destination (and broadcast pan/address)
	radio::setFlags(RADIO_ZB, radio::ContextFlags::PASS_DEST_SHORT | radio::ContextFlags::HANDLE_ACK);

	// start coroutines
	receive();
	sendBeacon();
	sendLinkStatus();
	
this->commissioning = true;
}

RadioInterface::~RadioInterface() {
}
/*
void RadioInterface::setPan(uint16_t panId) {
	this->panId = panId;
	
	// set pan of coordinator
	radio::setPan(this->radioIndex, panId);
}
*/
void RadioInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled && this->devices.size() < MAX_DEVICE_COUNT;
}

int RadioInterface::getDeviceCount() {
	return this->devices.size();
}

DeviceId RadioInterface::getDeviceId(int index) {
	assert(index >= 0 && index < this->devices.size());
	return this->devices[index].flash->deviceId;
	return 0;
}

Array<EndpointType> RadioInterface::getEndpoints(DeviceId deviceId) {
	for (auto e : this->devices) {
		Device const &device = *e.flash;
		if (device.deviceId == deviceId) {
			//return Array<EndpointType>(device.endpointTypes, device.getEndpointCount());
			return {device.endpointCount, device.getEndpoints()};
		}
	}
	return {};
}

void RadioInterface::subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
	// application wants to subscribe to a device
/*	for (auto e : this->devices) {
		Device const &device = *e.flash;
		DeviceState *state = *e.ram;
		if (device.deviceId == deviceId) {
			assert(endpointIndex >= 0 && endpointIndex < device.getEndpointCount());
			if (state->firstEndpointId == 0) {
				// allocate endpoints for this device
				state->firstEndpointId = this->nextEndpointId;
				this->nextEndpointId += device.getEndpointCount();
			}

			int newEndpointId = state->firstEndpointId + endpointIndex;
		
			if (endpointId == 0) {
				++state->referenceCounters[endpointIndex];
			} else {
				assert(endpointId == newEndpointId);
			}
			endpointId = newEndpointId;
			
			break;
		}
	}*/
}

void RadioInterface::unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) {
/*	if (endpointId != 0) {
		for (auto e : this->devices) {
			Device const &device = *e.flash;
			DeviceState *state = *e.ram;
			if (device.deviceId == deviceId) {
				assert(endpointIndex >= 0 && endpointIndex < device.getEndpointCount());
				assert(state->referenceCounters[endpointIndex] > 0);
				
				--state->referenceCounters[endpointIndex];
				endpointId = 0;
			}

			break;
		}
	}*/
}

void RadioInterface::send(uint8_t endpointId, uint8_t const *data, int length) {
}


Coroutine RadioInterface::receive() {
	while (true) {
		radio::Packet packet;
		co_await radio::receive(RADIO_ZB, packet);
		PacketReader r(packet);
		
		// ieee 802.15.4 mac
		uint8_t const *mac = r.current;

		// frame control
		auto frameControl = r.enum16<ieee::FrameControl>();

		// the radio filter flags ensure that a sequence number is present
		uint8_t macCounter = r.int8();
		
		// the radio filter flags ensure that a destination pan id is present that is either broadcast or configuration.zbPanId
		uint16_t destinationPanId = r.int16();
		bool destinationBroadcast = destinationPanId == 0xffff;
		
		// the radio filter flags ensure that a destination address is present hat is either boradcast or our address
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destAddress = r.int16();
			destinationBroadcast |= destAddress == 0xffff;
		} else {
			// long destination address is always our address
			r.skip(8);
		}
if (destinationBroadcast)
	std::cout << "broadcast" << std::endl;

		auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
		if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) == 0) {
			// no source address
			if (frameType == ieee::FrameControl::TYPE_COMMAND) {
				auto command = r.enum8<ieee::Command>();
				if (command == ieee::Command::BEACON_REQUEST) {
					// handle beacon request by sending a beacon
printf("beacon request\n");
					this->beaconBarrier.resumeAll();
				}
			} else if (frameType == ieee::FrameControl::TYPE_DATA) {
				// network layer: handle dependent on protocol version
				auto version = r.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
				if (version == gp::NwkFrameControl::VERSION_3_GP)
					handleGp(mac, r);
			}
		} else {
			// short or long source address

			// get source pan id
			uint16_t sourcePanId = destinationPanId;
			if ((frameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0) {
				sourcePanId = r.int16();
			}

			Storage::Element<Device, DeviceState*> element = {nullptr};
			if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
				// short source address
				uint16_t sourceAddress = r.int16();

				// check for command frame
				if (frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = r.enum8<ieee::Command>();
				} else {
					// try to lookup device by short address
					if (this->associationCoroutine.isRunning()) {
						if (this->tempDevice->u.zb.shortAddress == sourceAddress) {
							element.flash = this->tempDevice;
							element.ram = &this->tempState;
						}
					}
					for (auto e : this->devices) {
						Device const *device = e.flash;
						if (device->type == Device::ZB && device->u.zb.shortAddress == sourceAddress) {
							element = e;
							break;
						}
					}
				}
			} else {
				// long source address
				uint64_t sourceAddress = r.int64();

				// check for command frame
				if (frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = r.enum8<ieee::Command>();

					// check for association request with no broadcast
					if (this->commissioning && !destinationBroadcast && command == ieee::Command::ASSOCIATION_REQUEST) {
printf("association request\n");
						this->associationCoroutine.destroy();
						this->associationCoroutine.destroy();
						
						// start new association request
						auto associationRequest = r.enum8<ieee::AssociationRequest>();
						bool receiveOnWhenIdle = (associationRequest & ieee::AssociationRequest::RECEIVE_ON_WHEN_IDLE) != 0;
						auto sendFlags = receiveOnWhenIdle ? radio::SendFlags::NONE : radio::SendFlags::AWAIT_DATA_REQUEST;
						this->associationCoroutine = handleAssociationRequest(sourceAddress, sendFlags);
					}
				} else {
					// try to lookup device by long address
					if (this->associationCoroutine.isRunning()) {
						if (this->tempDevice->deviceId == sourceAddress) {
							element.flash = this->tempDevice;
							element.ram = &this->tempState;
						}
					}
					for (auto e : this->devices) {
						Device const *device = e.flash;
						if (device->type == Device::ZB && device->deviceId == sourceAddress) {
							element = e;
							break;
						}
					}
				}
			}

			// check if the device was found
			if (element.flash) {
				Device const &device = *element.flash;
				DeviceState *state = *element.ram;
				
				if (frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = r.enum8<ieee::Command>();

					// check if destination pan is not broadcast
					if (!destinationBroadcast) {
						if (command == ieee::Command::DATA_REQUEST) {
							// data request is handled automatically by the radio driver
							
							// resume the first coroutine that waits for "ready to send" from device
							//state->dataRequestBarrier.resumeFirst();
						}
					}
				} else if (frameType == ieee::FrameControl::TYPE_DATA) {
					// network layer: handle dependent on protocol version
					auto version = r.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
					if (version == gp::NwkFrameControl::VERSION_2) {
						handleNwk(r, device, state);
					}
				}
			}
		}
	}
}

void RadioInterface::handleGp(uint8_t const *mac, PacketReader &r) {
	// green power frame
	// -----------------
	// NWK header (frame control)
	// Security header (0, 1 or 4 byte counter)
	// NWK payload (optionally encrypted)
	// MIC (0, 2 or 4 byte message integrity code)
	uint8_t *frame = r.current;

	// frame control
	auto frameControl = r.enum8<gp::NwkFrameControl>();

	// extended frame conrol
	gp::NwkExtendedFrameControl extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = r.enum8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;
	
	// device id
	uint32_t deviceId = r.int32();

	if (this->commissioning && securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& r.peekEnum8<gp::Command>() == gp::Command::COMMISSIONING)
	{
		// commissioning
		handleGpCommission(deviceId, r);
		return;
	}

	// search device
	for (auto e : this->devices) {
		if (e.flash->deviceId == deviceId) {
			Device const &device = *e.flash;
			DeviceState &state = **e.ram;
						
			if (securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16) {
				// security level 1: 1 byte counter, 2 byte mic

				// security counter (use mac sequence number)
				uint32_t counter = mac[2];

				// nonce
				Nonce nonce(deviceId, counter);

				// header starts at mac sequence number and includes also payload
				uint8_t const *header = mac + 2;
				int micLength = 2;
				int headerLength = r.end - micLength - header;
				if (headerLength < 1)
					return;
					
				// decrypt and check
				if (!decrypt(nullptr, nonce, header, headerLength, header + headerLength, 0, micLength, device.u.gp.key)) {
					//printf("error while decrypting message!\n");
					return;
				}
				
				// remove mic from end
				r.end -= micLength;
			} else if (securityLevel >= gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32) {
				// security levels 2 and 3: 4 byte counter, 4 byte mic
				
				// security counter
				uint32_t counter = r.int32();

				// nonce
				Nonce nonce(deviceId, counter);

				// header starts at frame (frameControl) and either includes payload (level 2) or not (level 3)
				uint8_t *header = frame;
				int micLength = 4;
				int headerAndPayloadLength = r.end - micLength - header;
				int headerLength = securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32
					? headerAndPayloadLength : r.current - header;
				if (headerLength < 1)
					return;

				uint8_t *message = header + headerLength;
				int payloadLength = headerAndPayloadLength - headerLength;

				// decrypt in-place and check
				if (!decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, device.u.gp.key)) {
					//printf("error while decrypting message!\n");
					return;
				}

				// remove mic from end
				r.end -= micLength;
			}

			switch (device.deviceType) {
			case 0x02:
				if (r.getRemaining() >= 1) {
					int endpointIndex = -1;
					uint8_t data = 0;
					uint8_t command = r.int8();
					switch (command) {
					// A
					case 0x14:
					case 0x15:
						endpointIndex = 0;
						data = 0;
						break;
					case 0x10:
						endpointIndex = 0;
						data = 1;
						break;
					case 0x11:
						endpointIndex = 0;
						data = 2;
						break;
					// B
					case 0x17:
					case 0x16:
						endpointIndex = 1;
						data = 0;
						break;
					case 0x13:
						endpointIndex = 1;
						data = 1;
						break;
					case 0x12:
						endpointIndex = 1;
						data = 2;
						break;
					// AB
					case 0x65:
					case 0x63:
						endpointIndex = 2;
						data = 0;
						break;
					case 0x64:
						endpointIndex = 2;
						data = 1;
						break;
					case 0x62:
						endpointIndex = 2;
						data = 2;
						break;
					}
					//if (endpointIndex != -1 && state.referenceCounters[endpointIndex] > 0)
					//	this->onReceived(state.firstEndpointId + endpointIndex, &data, 1);
				}
				break;
			case 0x07:
				if (r.getRemaining() >= 2) {
					uint8_t buttons = r.int8();
					
					uint8_t change = (buttons ^ state.state) & 0x0f;
					
					// check AB0 and AB1
					if (change == 0x5 || change == 0xa) {
						uint8_t data = buttons & 0x3;
						//if (state.referenceCounters[2] > 0)
						//	this->onReceived(state.firstEndpointId + 2, &data, 1);
					} else {
						// check A0 and A1
						if (change & 0x3) {
							uint8_t data = buttons & 0x3;
							//if (state.referenceCounters[0] > 0)
							//	this->onReceived(state.firstEndpointId + 0, &data, 1);
						}

						// check B0 and B1
						if (change & 0xC) {
							uint8_t data = (buttons >> 2) & 0x3;
							//if (state.referenceCounters[1] > 0)
							//	this->onReceived(state.firstEndpointId + 1, &data, 1);
						}
					}
					
					state.state = buttons;
				}
			}
			
			break;
		}
	}
}

void RadioInterface::handleGpCommission(uint32_t deviceId, PacketReader& r) {
	// remove commissioning command (0xe0)
	r.int8();
	
	Device device;
	device.deviceId = deviceId;
	device.type = Device::GP;
	
	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	device.deviceType = r.int8();

	//device.endpointTypes[0] = EndpointType(0);
	switch (device.deviceType) {
	case 0x02:
		// switch, PTM215Z ("friends of hue")
		device.u.gp.endpoints[0] = uint8_t(EndpointType::ROCKER);
		device.u.gp.endpoints[1] = uint8_t(EndpointType::ROCKER);
		device.u.gp.endpoints[2] = uint8_t(EndpointType::ROCKER);
		device.endpointCount = 3;
		//device.endpointTypes[0] = EndpointType::ROCKER;
		//device.endpointTypes[1] = EndpointType::ROCKER;
		//device.endpointTypes[2] = EndpointType::ROCKER;
		//device.endpointTypes[3] = EndpointType(0);
		break;
	case 0x07:
		// generic switch, PTM216Z
		device.u.gp.endpoints[0] = uint8_t(EndpointType::ROCKER);
		device.u.gp.endpoints[1] = uint8_t(EndpointType::ROCKER);
		device.u.gp.endpoints[2] = uint8_t(EndpointType::ROCKER);
		device.endpointCount = 3;
		//device.endpointTypes[0] = EndpointType::ROCKER;
		//device.endpointTypes[1] = EndpointType::ROCKER;
		//device.endpointTypes[2] = EndpointType::ROCKER;
		//device.endpointTypes[3] = EndpointType(0);
		break;
	default:
		// unknown device type
		return;
	}
	
	// options
	auto options = r.enum8<gp::Options>();
	uint32_t counter = 0;
	if ((options & gp::Options::EXTENDED) != 0) {
		auto extendedOptions = r.enum8<gp::ExtendedOptions>();;
		
		// security level capability (used in messages)
		auto securityLevel = extendedOptions & gp::ExtendedOptions::SECURITY_LEVEL_CAPABILITY_MASK;

		// check for key
		if ((extendedOptions & gp::ExtendedOptions::KEY_PRESENT) != 0) {
			uint8_t *key = r.current;
			if ((extendedOptions & gp::ExtendedOptions::KEY_ENCRYPTED) != 0) {
				// Green Power A.1.5.3.3.3
				
				// nonce
				Nonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setLittleEndianInt32(0, deviceId);

				if (!decrypt(key, nonce, header.data, 4, key, 16, 4, trustCenterLinkAesKey)) {
					//printf("error while decrypting key!\n");
					return;
				}

				// skip key and MIC
				r.skip(16 + 4);
			} else {
				// skip key
				r.skip(16);
			}
			
			// set key for device
			setKey(device.u.gp.key, key);
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			counter = r.int32();
			//printf("counter: %08x\n", counter);
		}
	}

	// check if we exceeded the end
	if (r.getRemaining() < 0)
		return;

	// check if device already exists
	for (auto e : this->devices) {
		if (e.flash->deviceId == deviceId) {
			// yes: only update security counter
			(*e.ram)->securityCounter = counter;
			return;
		}
	}

	// create device state
	DeviceState *deviceState = new DeviceState();
	deviceState->securityCounter = counter;

	// store device in flash
	this->devices.write(this->devices.size(), &device, &deviceState);
}

AwaitableCoroutine RadioInterface::handleAssociationRequest(uint64_t sourceAddress, radio::SendFlags sendFlags) {
	constexpr int MAX_RETRY = 2;
printf("handleAssociationRequest\n");

	// create temporary device and state
	Device device;
	Pointer<DeviceState> deviceState = new DeviceState();

	// set pointers so that we can forward responses
	this->tempDevice = &device;
	this->tempState = &*deviceState;

	
	device.deviceId = sourceAddress;
	device.type = Device::ZB;

	// todo: allocate short address
	device.u.zb.shortAddress = 0x1337;//0xcb36;//0x991e;;


	uint8_t packet1[DeviceState::RESPONSE_LENGTH > 72 ? DeviceState::RESPONSE_LENGTH : 72]; // space for key transport or response
	uint8_t packet2[DeviceState::RESPONSE_LENGTH > 48 ? DeviceState::RESPONSE_LENGTH : 48]; // space for zdp request or response
	uint8_t sendResult;
	zb::ZdpCommand zdpCommand;
	uint16_t length;

	// send association response
	{
		// get global configuration (is valid until next co_await)
		Configuration const &configuration = *this->configuration[0].flash;

		PacketWriter w(packet1);
		
		// ieee 802.15.4 frame control
		w.enum16(ieee::FrameControl::TYPE_COMMAND
			| ieee::FrameControl::ACKNOWLEDGE_REQUEST
			| ieee::FrameControl::PAN_ID_COMPRESSION
			| ieee::FrameControl::DESTINATION_ADDRESSING_LONG
			| ieee::FrameControl::SOURCE_ADDRESSING_LONG);

		// sequence number
		w.int8(this->macCounter++);

		// destination pan
		w.int16(configuration.zbPanId);

		// destination address
		w.int64(device.deviceId);
		
		// source address
		w.int64(configuration.longAddress);
		
		
		// association response
		w.enum8(ieee::Command::ASSOCIATION_RESPONSE);
		
		// short address of device
		w.int16(device.u.zb.shortAddress);
		
		// association status
		w.int8(0);
		
		// always await data request
		w.finish(radio::SendFlags::AWAIT_DATA_REQUEST);
	}
printf("send association response\n");
	co_await radio::send(RADIO_ZB, packet1, sendResult);
printf("send association response result %d\n", sendResult);
	if (sendResult == 0)
		co_return;

	// send network key
	for (int retry = 0; ; ++retry) {
		{
			// get global configuration
			Configuration const &configuration = *this->configuration[0].flash;

			PacketWriter2 w(packet1);
			
			// nwk header
			writeNwkHeader(w, device.u.zb.shortAddress, zb::NwkFrameControl::TYPE_DATA
				| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS);
			
			// aps header
			writeApsCommand(w, zb::ApsFrameControl::DELIVERY_UNICAST
				| zb::ApsFrameControl::SECURITY);
			
			// aps command
			w.enum8<zb::ApsCommand>(zb::ApsCommand::TRANSPORT_KEY);
			w.int8(0x01); // standard network key
			w.data(configuration.networkKey);
			w.int8(0); // key sequence number
			w.int64(device.deviceId); // extended destination address
			w.int64(configuration.longAddress); // extended source address
		
			writeFooter(w, sendFlags);
		}
		co_await radio::send(RADIO_ZB, packet1, sendResult);
printf("sent key result %d\n", sendResult);
		if (sendResult != 0)
			break;
			
		if (retry == MAX_RETRY)
			co_return;
	}

	// get node descriptor
	for (int retry = 0; ; ++retry) {
		// send node descriptor request
		uint8_t zdpCounter;
		{
			PacketWriter2 w(packet1);

			// nwk header
			writeNwkHeader(w, device.u.zb.shortAddress, zb::NwkFrameControl::TYPE_DATA
				| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
				| zb::NwkFrameControl::SECURITY);

			// zdp header
			zdpCounter = writeZdpHeader(w, zb::ZdpCommand::NODE_DESCRIPTOR_REQUEST);//, device.u.zb.shortAddress);
			
			// address of interest
			w.int16(device.u.zb.shortAddress);
			
			writeFooter(w, sendFlags);
		}
		co_await radio::send(RADIO_ZB, packet1, sendResult);
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(deviceState->zdpResponseBarrier.wait({zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE, zdpCounter,
				length, packet1}), timer::delay(1s));
			
			// check if response was received
			if (r == 1)
				break;
		}
		if (retry == MAX_RETRY)
			co_return;
	}
	
	// handle node descriptor
	{
		PacketReader nodeDescriptor(length, packet1);
		uint8_t status = nodeDescriptor.int8();
		uint16_t addressOfInterest = nodeDescriptor.int16();
printf("node descriptor status %d\n", int(status));
	}

	// get list of active endpoints
	for (int retry = 0; ; ++retry) {
		// send active endpoint request
		uint8_t zdpCounter;
		{
			PacketWriter2 w(packet1);

			// nwk header
			writeNwkHeader(w, device.u.zb.shortAddress, zb::NwkFrameControl::TYPE_DATA
				| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
				| zb::NwkFrameControl::SECURITY);

			// zdp header
			zdpCounter = writeZdpHeader(w, zb::ZdpCommand::ACTIVE_ENDPOINT_REQUEST);//, device.u.zb.shortAddress);

			// address of interest
			w.int16(device.u.zb.shortAddress);

			writeFooter(w, sendFlags);
		}
		co_await radio::send(RADIO_ZB, packet1, sendResult);
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(deviceState->zdpResponseBarrier.wait({zb::ZdpCommand::ACTIVE_ENDPOINT_RESPONSE, zdpCounter,
				length, packet1}), timer::delay(1s));

			// check if response was received
			if (r == 1)
				break;
		}
		if (retry == MAX_RETRY)
			co_return;
	}

	// handle list of active endpoints
	PacketReader endpoints(length, packet1);
	{
		uint8_t status = endpoints.int8();
		uint16_t addressOfInterest = endpoints.int16();
printf("active endpoints status %d\n", int(status));
	}

	// get descriptor of each endpoint
	uint8_t zbEndpointCount = endpoints.int8();
	int endpointCount = 0;
	for (int i = 0; i < zbEndpointCount; ++i) {
		// get next endpoint
		uint8_t endpoint = endpoints.int8();
		
		// get endpoint descriptor
		uint8_t zdpCounter;
		for (int retry = 0; ; ++retry) {
			// send simple descriptor request
			{
				PacketWriter2 w(packet2);

				// nwk header
				writeNwkHeader(w, device.u.zb.shortAddress, zb::NwkFrameControl::TYPE_DATA
					| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
					| zb::NwkFrameControl::SECURITY);

				// zdp header
//this->zdpCounter++;
				zdpCounter = writeZdpHeader(w, zb::ZdpCommand::SIMPLE_DESCRIPTOR_REQUEST);

				// address of interest
				w.int16(device.u.zb.shortAddress);

				// endpoint to query
				w.int8(endpoint);

				writeFooter(w, sendFlags);
			}
			co_await radio::send(RADIO_ZB, packet2, sendResult);
			if (sendResult != 0) {
				// wait for a response from the device
				int r = co_await select(deviceState->zdpResponseBarrier.wait({zb::ZdpCommand::SIMPLE_DESCRIPTOR_RESPONSE,
					zdpCounter, length, packet2}), timer::delay(1s));

				// check if response was received
				if (r == 1)
					break;
			}
			if (retry == MAX_RETRY)
				co_return;
		}
		
		// handle endpoint descriptor
		PacketReader endpointDescriptor(length, packet2);
		{
			uint8_t status = endpointDescriptor.int8();
			uint16_t addressOfInterest = endpointDescriptor.int16();
			uint8_t descriptorLength = endpointDescriptor.int8();
			uint8_t endpoint2 = endpointDescriptor.int8();
			zb::ZclProfile profile = endpointDescriptor.enum16<zb::ZclProfile>();
			uint16_t applicationDevice = endpointDescriptor.int16();
			uint8_t applicationVersion = endpointDescriptor.int8();
printf("endpoint descriptor %d status %d\n", int(endpoint), int(status));
		}

		// iterate over input/server clusters
		uint8_t inputClusterCount = endpointDescriptor.int8();
		for (int i = 0; i < inputClusterCount && endpointDescriptor.getRemaining() >= 3; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.enum16<zb::ZclCluster>();
			
			// add our device endpoints based on input clusters
			for (int index = 0; index < array::size(endpointInfos); ++index) {
				auto const &info = endpointInfos[index];
				if (info.role == Role::SERVER /*&& info.profile == profile*/ && info.cluster == cluster) {
					// store endpoint type
					device.u.zb.endpoints[endpointCount] = uint8_t(info.endpointType);
					
					// also store index in enpointInfos and zb endpoint
					device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + endpointCount * 2] = index;
					device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + endpointCount * 2 + 1] = endpoint;
					++endpointCount;
					break;
				}
			}
		}
		
		// iterate over output/client clusters (client sends commmands to manipulate attributes in server)
		uint8_t outputClusterCount = endpointDescriptor.int8();
		for (int i = 0; i < outputClusterCount && endpointDescriptor.getRemaining() >= 2; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.enum16<zb::ZclCluster>();

			// add our device endpoints based on output clusters
			for (int index = 0; index < array::size(endpointInfos); ++index) {
				auto const &info = endpointInfos[index];
				if (info.role == Role::CLIENT /*&& info.profile == profile*/ && info.cluster == cluster) {
					// store endpoint type
					device.u.zb.endpoints[endpointCount] = uint8_t(info.endpointType);
					
					// also store index in enpointInfos and zb endpoint
					device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + endpointCount * 2] = index;
					device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + endpointCount * 2 + 1] = endpoint;
					++endpointCount;
					break;
				}
			}
		}
	}

	// check some endpoints if they are available, e.g. battery percentage
	for (int i = 0; i < endpointCount; ++i) {
		auto endpointType = EndpointType(device.u.zb.endpoints[i]);
		if (endpointType == EndpointType::BATTERY_PERCENTAGE) {
			uint8_t index = device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + i * 2];
			auto const &info = endpointInfos[index];
			uint8_t endpoint = device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + i * 2 + 1];
			do {
				// wait until device is ready to receive
				//co_await deviceState->dataRequestBarrier.wait();

				// send read attributes
				uint8_t zclCounter;
				{
					PacketWriter2 w(packet1);

					// nwk header
					writeNwkHeader(w, device.u.zb.shortAddress, zb::NwkFrameControl::TYPE_DATA
						| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
						| zb::NwkFrameControl::SECURITY);
						
					// zcl
					zclCounter = writeZclProfileWide(w, endpoint, endpoint,
						zb::ZclProfile::HOME_AUTOMATION, zb::ZclCluster::BASIC, zb::ZclCommand::READ_ATTRIBUTES);
					w.enum16<zb::ZclBasicAttribute>(zb::ZclBasicAttribute::POWER_SOURCE);
					
					writeFooter(w, sendFlags);
				}
				co_await radio::send(RADIO_ZB, packet1, sendResult);

				// try again if send was not successful
				if (!sendResult)
					continue;

				// wait for a response from the device
				int r = co_await select(deviceState->zclResponseBarrier.wait({endpoint, endpoint,
					zb::ZclProfile::HOME_AUTOMATION, zb::ZclCluster::BASIC, zclCounter,
					zb::ZclCommand::READ_ATTRIBUTES_RESPONSE, length, packet2}), timer::delay(300ms));
				if (r == 2)
					continue;
			} while (false);

			// handle read attribute response
			PacketReader attributeResponse(length, packet2);
			auto attribute = attributeResponse.enum16<zb::ZclBasicAttribute>();
			uint8_t status = attributeResponse.int8();
			auto type = attributeResponse.enum8<zb::ZclDataType>();
			auto source = attributeResponse.enum8<zb::ZclPowerSourceType>();
printf("power source attribute status %d value %d\n", int(status), int(source));

			// remove endpoint if the device is not battery powered
			if (source != zb::ZclPowerSourceType::BATTERY) {
				--endpointCount;

				for (int j = i; j < endpointCount; ++j) {
					device.u.zb.endpoints[j] = device.u.zb.endpoints[j + 1];
					device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + j * 2] = device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + j * 2 + 2];
					device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + j * 2 + 1] = device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + j * 2 + 2 + 1];
				}
			}
		}
	}
	
	for (int i = 0; i < endpointCount; ++i) {
		auto endpointType = EndpointType(device.u.zb.endpoints[i]);
		EndpointInfo const &info = endpointInfos[device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + i * 2]];
		uint8_t endpoint = device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + i * 2 + 1];
		
		// check if we have to bind this cluster
		if (info.bind) {

			// bind request
			do {
				// wait until device is ready to receive
				//co_await deviceState->dataRequestBarrier.wait();

				// send bind request
				uint8_t zdpCounter;
				{
					// get global configuration
					Configuration const &configuration = *this->configuration[0].flash;
					
					PacketWriter2 w(packet1);

					// nwk header
					writeNwkHeader(w, device.u.zb.shortAddress, zb::NwkFrameControl::TYPE_DATA
						| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
						| zb::NwkFrameControl::SECURITY);
						
					// zdp header
					zdpCounter = writeZdpHeader(w, zb::ZdpCommand::BIND_REQUEST);

					// source
					w.int64(device.deviceId);
					
					// source endpoint
					w.int8(endpoint);
					
					// cluster
					w.enum16<zb::ZclCluster>(info.cluster);
					
					// address mode: unicast
					w.int8(3);
					
					// destination
					w.int64(configuration.longAddress);
					
					// destination endpoint
					w.int8(endpoint);
					
					writeFooter(w, sendFlags);
				}
				co_await radio::send(RADIO_ZB, packet1, sendResult);

				// try again if send was not successful
				if (!sendResult)
					continue;

				// wait for a response from the device
				int r = co_await select(deviceState->zdpResponseBarrier.wait({zb::ZdpCommand::BIND_RESPONSE, zdpCounter,
					length, packet1}), timer::delay(300ms));
				if (r == 2)
					continue;
			} while (false);
			
			// handle bind response
			PacketReader bindResponse(length, packet1);
			uint8_t status = bindResponse.int8();
			printf("bind response %d\n", int(status));
		}
	}
	
	
	// move pairs of index and endpoint
	for (int i = 0; i < endpointCount * 2; ++i) {
		device.u.zb.endpoints[endpointCount + i] = device.u.zb.endpoints[Device::MAX_ENDPOINT_COUNT + i];
	}
	
	// allocate endpoints
	deviceState->endpoints = new DeviceState::Endpoint[endpointCount];
	
	// write the configured device to flash
	int index = this->devices.size();
	auto ds = &*deviceState;
	this->devices.write(index, &device, &ds);
}


void RadioInterface::handleNwk(PacketReader &r, Device const &device, DeviceState *state) {
	Configuration const &configuration = *this->configuration[0].flash;
	
	// header ("string a" for decryption)
	uint8_t const *header = r.current;

	auto frameControl = r.enum16<zb::NwkFrameControl>();
	uint16_t destination = r.int16();
	uint16_t source = r.int16();
	uint8_t radius = r.int8();
	uint8_t sequenceNumber = r.int8();

	if ((frameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0) {
		uint8_t const *extendedSource = r.current;
		r.skip(8);
	}

	// security header
	// note: does in-place decryption of the payload
	if ((frameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// restore security level according to 4.3.1.2 step 1.
		r.current[0] |= securityLevel;
		
		// security control field (4.5.1.1)
		auto securityControl = r.enum8<zb::SecurityControl>();
				
		if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_NETWORK) {
			// only network key supported
			return;
		}
		if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
			// only extended nonce supported
			return;
		}

		uint32_t securityCounter = r.int32();

		uint8_t const *extendedSource = r.current;
		r.skip(8);
		
		uint8_t keySequenceNumber = r.int8();

		int headerLength = r.current - header;

		// encrypted message followed by 4 byte message integrity code (mic)
		uint8_t *message = r.current;
		int micLength = 4;
		int payloadLength = r.end - micLength - message;

		// nonce (4.5.2.2)
		Nonce nonce(extendedSource, securityCounter, securityControl);

		// in-place decrypt
		bool result = decrypt(message, nonce, header, headerLength, message, payloadLength, micLength, configuration.networkAesKey);
		
		if (!result) {
			// decryption failed
			return;
		}
		
		// remove mic from end
		r.end -= micLength;
	}

	auto frameType = frameControl & zb::NwkFrameControl::TYPE_MASK;
	if (frameType == zb::NwkFrameControl::TYPE_COMMAND) {
	
	} else if (frameType == zb::NwkFrameControl::TYPE_DATA) {
		handleAps(r, device, state);
	}
}

void RadioInterface::handleAps(PacketReader &r, Device const &device, DeviceState *state) {
	// frame control
	auto frameControl = r.enum8<zb::ApsFrameControl>();
	auto frameType = frameControl & zb::ApsFrameControl::TYPE_MASK;
	if (frameType == zb::ApsFrameControl::TYPE_COMMAND) {
		// aps command

	} else if (frameType == zb::ApsFrameControl::TYPE_DATA) {
	   // aps data: zdp or zcl follow
	   uint8_t destinationEndpoint = r.int8();
	   if (destinationEndpoint == 0)
		   handleZdp(r, device, state);
	   else
		   handleZcl(r, device, state, destinationEndpoint);
   } else if (frameType == zb::ApsFrameControl::TYPE_ACK) {
		// aps ack
   }
}

void RadioInterface::handleZdp(PacketReader &r, Device const &device, DeviceState *state) {
	auto command = r.enum16<zb::ZdpCommand>();
	uint16_t profile = r.int16();
	uint8_t sourceEndpoint = r.int8();
	uint8_t apsCounter = r.int8();
	uint8_t zdpCounter = r.int8();

	uint8_t *response = r.current;
	int length = min(r.getRemaining(), DeviceState::RESPONSE_LENGTH);
	
	state->zdpResponseBarrier.resumeFirst([command, zdpCounter, length, response](DeviceState::ZdpResponse r) {
		if (r.command == command && r.zdpCounter == zdpCounter) {
			r.length = length;
			array::copy(length, r.response, response);
			return true;
		}
		return false;
	});
}

void RadioInterface::handleZcl(PacketReader &r, Device const &device, DeviceState *state, uint8_t dstEndpoint) {
	auto cluster = r.enum16<zb::ZclCluster>();
	auto profile = r.enum16<zb::ZclProfile>();
	uint8_t srcEndpoint = r.int8();
	uint8_t apsCounter = r.int8();
	
	// zcl frame starts here
	auto frameControl = r.enum8<zb::ZclFrameControl>();
	uint8_t zclCounter = r.int8();
	auto command = r.enum8<zb::ZclCommand>();
	
	auto frameType = frameControl & zb::ZclFrameControl::TYPE_MASK;
	if (frameType == zb::ZclFrameControl::TYPE_PROFILE_WIDE) {
		// profile wide commands such as "read attribute response"
		uint8_t *response = r.current;
		int length = min(r.getRemaining(), DeviceState::RESPONSE_LENGTH);

		state->zclResponseBarrier.resumeFirst([dstEndpoint, srcEndpoint, profile, cluster, zclCounter, command, length, response]
			(DeviceState::ZclResponse r)
		{
			if (r.dstEndpoint == dstEndpoint && r.srcEndpoint == srcEndpoint && r.profile == profile && r.cluster == cluster
				&& r.zclCounter == zclCounter && r.command == command)
			{
				r.length = length;
				array::copy(length, r.response, response);
				return true;
			}
			return false;
		});
	} else if (frameType == zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC) {
		// cluster specific commands such as "on"
		bool disableDefaultResponse = (frameControl & zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE) != 0;
	
		printf("ZCL command : %d\n", int(command));
		
		// send default response
	}
}


Coroutine RadioInterface::sendBeacon() {
	uint8_t packet[32];
	uint8_t sendResult;
	while (true) {
		// wait until a beacon request was received
		co_await this->beaconBarrier.wait();

		// send beacon
		{
			// get global configuration
			Configuration const &configuration = *this->configuration[0].flash;

			PacketWriter w(packet);

			// ieee 802.15.4 frame control
			w.enum16(ieee::FrameControl::TYPE_BEACON
				| ieee::FrameControl::SOURCE_ADDRESSING_SHORT);

			// sequence number
			w.int8(this->macCounter++);

			// source pan
			w.int16(configuration.zbPanId);

			// source address
			w.int16(0x0000);

			// superframe specification
			w.int16(15 // beacon interval
				| (15 << 4) // superframe interval
				| (15 << 8) // final cap slot
				| (0 << 12) // battery extension
				| 0x4000 // pan coordinator
				| (this->commissioning ? 0x8000 : 0)); // association permit

			// gts
			w.int8(0);

			// pending addresses, short and long
			w.int8(0x00);

			// beacon

			// protocol id
			w.int8(0);

			// stack profile
			w.int16(2 // zb pro
				| (2 << 4) // protocol version
				| (0 << 10) // router capacity
				| (0 << 11) // device depth (4 bit)
				| 0x8000); // end device capacity

			// extended pan id
			w.int64(UINT64_C(0xdddddddddddddddd));

			// tx offset
			w.int16(0xffff);
			w.int8(0xff);

			// update id
			w.int8(0);
	
			w.finish(radio::SendFlags::NONE);
		}
		co_await radio::send(RADIO_ZB, packet, sendResult);
		
		// "cool down" before a new beacon can be sent
		co_await timer::delay(100ms);
	}
}


void RadioInterface::writeNwkHeader(PacketWriter2 &w, uint16_t destinationAddress, zb::NwkFrameControl nwkFrameControl) {
	// get global configuration
	Configuration const &configuration = *this->configuration[0].flash;


	// ieee 802.15.4 frame control
	auto frameControl = ieee::FrameControl::TYPE_DATA
		| ieee::FrameControl::PAN_ID_COMPRESSION
		| ieee::FrameControl::DESTINATION_ADDRESSING_SHORT
		| ieee::FrameControl::SOURCE_ADDRESSING_SHORT;
	if (destinationAddress != 0xffff)
		frameControl |= ieee::FrameControl::ACKNOWLEDGE_REQUEST;
	w.enum16(frameControl);

	// sequence number
	w.int8(this->macCounter++);
	
	// destination pan
	w.int16(configuration.zbPanId);

	// destination address
	w.int16(destinationAddress);

	// source address
	w.int16(0x0000);
	

	// header ("string a" for encryption)
	w.header = w.current;

	// network layer frame control field
	w.enum16(nwkFrameControl | zb::NwkFrameControl::VERSION_2);

	// destination
	w.int16(destinationAddress == 0xffff ? 0xfffc : destinationAddress);
	
	// source
	w.int16(0x0000);
	
	// radius
	w.int8(1);
	
	// sequence number
	w.int8(this->nwkCounter++);
	
	// extended source
	if ((nwkFrameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0)
		w.int64(configuration.longAddress);


	// security header
	if ((nwkFrameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// security control field
		w.securityControlPtr = w.current;
		w.enum8(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_NETWORK
			| zb::SecurityControl::EXTENDED_NONCE);

		// security counter
		w.int32(this->securityCounter);

		// extended source
		w.int64(configuration.longAddress);

		// key sequence number
		w.int8(0);
	} else {
		w.securityControlPtr = nullptr;
	}
	
	// message to encrypt
	w.message = w.current;
}

void RadioInterface::writeApsCommand(PacketWriter2 &w, zb::ApsFrameControl apsFrameControl) {
	// get global configuration
	Configuration const &configuration = *this->configuration[0].flash;


	// header ("string a" for encryption)
	w.header = w.current;

	// application support layer frame control field
	w.enum8(zb::ApsFrameControl::TYPE_COMMAND | apsFrameControl);
	
	// counter
	w.int8(this->apsCounter++);


	// security header
	if ((apsFrameControl & zb::ApsFrameControl::SECURITY) != 0) {
		// security control field
		w.securityControlPtr = w.current;
		w.enum8(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_KEY_TRANSPORT
			| zb::SecurityControl::EXTENDED_NONCE);

		// security counter
		w.int32(this->securityCounter);

		// extended source
		w.int64(configuration.longAddress);
	} else {
		w.securityControlPtr = nullptr;
	}
	
	// message to encrypt
	w.message = w.current;
}
/*
void RadioInterface::writeApsData(PacketWriter2 &w, uint8_t destinationEndpoint, uint16_t clusterId, uint16_t profile,
	uint8_t sourceEndpoint)
{
	// application support layer frame control field
	w.enum8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint
	w.int8(destinationEndpoint);
	
	// cluster id
	w.int16(clusterId);
	
	// profile
	w.int16(profile);
	
	// source endpoint
	w.int8(sourceEndpoint);

	// counter
	w.int8(this->apsCounter++);
}
*/
uint8_t RadioInterface::writeZdpHeader(PacketWriter2 &w, zb::ZdpCommand command) {//, uint16_t addressOfInterest) {
	// application support layer frame control field
	w.enum8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint (device)
	w.int8(0);
	
	// cluster id
	w.enum16<zb::ZdpCommand>(command);
	
	// profile (device profile)
	w.int16(0x0000);
	
	// source endpoint (device)
	w.int8(0);

	// counter
	w.int8(this->apsCounter++);
	
	
	// device profile
	uint8_t zdpCounter = this->zdpCounter++;
	w.int8(zdpCounter);
	//w.int16(addressOfInterest);
	
	return zdpCounter;
}

uint8_t RadioInterface::writeZclProfileWide(PacketWriter2 &w, uint8_t dstEndpoint, uint8_t srcEndpoint,
	zb::ZclProfile profile, zb::ZclCluster cluster, zb::ZclCommand command)
{
	w.enum8<zb::ApsFrameControl>(zb::ApsFrameControl::TYPE_DATA);
	w.int8(dstEndpoint);
	w.enum16<zb::ZclCluster>(cluster);
	w.enum16<zb::ZclProfile>(profile);
	w.int8(srcEndpoint);
	w.int8(this->apsCounter++);
	
	w.enum8<zb::ZclFrameControl>(zb::ZclFrameControl::TYPE_PROFILE_WIDE
		| zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE);
	uint8_t zclCounter = this->zclCounter++;
	w.int8(zclCounter);
	w.enum8<zb::ZclCommand>(command);

	return zclCounter;
}

void RadioInterface::writeFooter(PacketWriter2 &w, radio::SendFlags sendFlags) {
	if (w.securityControlPtr != nullptr) {
		// get global configuration
		Configuration const &configuration = *this->configuration[0].flash;

		auto securityControl = zb::SecurityControl(*w.securityControlPtr);
		
		// encrypt
		int headerLength = w.message - w.header;
		int payloadLength = w.current - w.message;
		int micLength = 4;
		w.skip(micLength);

		// nonce (4.5.2.2)
		Nonce nonce(configuration.longAddress, this->securityCounter, securityControl);

		// encrypt in-place
		AesKey const &key = (securityControl & zb::SecurityControl::KEY_MASK) == zb::SecurityControl::KEY_NETWORK
			? configuration.networkAesKey : hashedTrustCenterLinkAesKey;
		encrypt(w.message, nonce, w.header, headerLength, w.message, payloadLength, micLength, key);

		//bool ok = decrypt(p.message, nonce, p.header, headerLength, p.message, payloadLength, micLength, key);

		// clear security level according to 4.3.1.1 step 8.
		*w.securityControlPtr &= ~zb::SecurityControl::LEVEL_MASK;

		// increment security counter
		++this->securityCounter;
	}
	w.finish(sendFlags);
}

Coroutine RadioInterface::sendLinkStatus() {
	uint8_t packet[48];
	uint8_t sendResult;
	while (true) {
		// send link status every 15 seconds
		co_await timer::delay(15s);

		// send link status
		{
			PacketWriter2 w(packet);
			writeNwkHeader(w, 0xffff, zb::NwkFrameControl::TYPE_COMMAND
				| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
				| zb::NwkFrameControl::SECURITY
				| zb::NwkFrameControl::EXTENDED_SOURCE);

			// command frame
			w.enum8(zb::NwkCommand::LINK_STATUS);
			w.int8(0x40 // last frame
				| 0x20 // first frame
				| 0); // link status count

			writeFooter(w, radio::SendFlags::NONE);
		}
		co_await radio::send(RADIO_ZB, packet, sendResult);
	}
}


// RadioInterface::Device

int RadioInterface::Device::getFlashSize() const {
	switch (this->type) {
	case GP:
		return getOffset(Device, u.gp.endpoints[this->endpointCount]);
	case ZB:
		return getOffset(Device, u.zb.endpoints[this->endpointCount * 3]);
	}
	return 0;
}

int RadioInterface::Device::getRamSize() const {
	return sizeof(DeviceState*);
}
/*
int RadioInterface::Device::getEndpointCount() const {
	int i = 0;
	while (i < array::size(this->endpointTypes) && this->endpointTypes[i] != EndpointType(0))
		++i;
	return i;
}
*/
EndpointType const *RadioInterface::Device::getEndpoints() const {
	switch (this->type) {
	case GP:
		return reinterpret_cast<EndpointType const *>(this->u.gp.endpoints);
	case ZB:
		return reinterpret_cast<EndpointType const *>(this->u.zb.endpoints);
	}
	return nullptr;
}

