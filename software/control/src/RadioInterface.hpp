#pragma once

#include <Interface.hpp>
#include <Storage.hpp>
#include <crypt.hpp>
#include <PacketReader.hpp>
#include <Coroutine.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 */
class RadioInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;

	/**
	 * Constructor
	 * @param onSent gets called when send queue is empty, also initialy when all devices on the bus are enumerated
	 * @param onReceived gets called when data was reveived from an endpoint (endpointId, data, length)
	 */
	RadioInterface(AesKey const &networkKey, std::function<void (uint8_t, uint8_t const *, int)> const &onReceived);

	~RadioInterface() override;

	/**
	 * Set long address
	 */
	void setLongAddress(uint64_t longAddress) {this->longAddress = longAddress;}

	/**
	 * Set pan is where the radio interface should operate. Must be called
	 * @param panId pan id
	 */
	void setPan(uint16_t panId);

	/**
	 * Set commissioning mode
	 * @param enabled true to enable commissioning
	 */
	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;
	void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void send(uint8_t endpointId, uint8_t const *data, int length) override;

	
	struct Device {
		enum Type : uint8_t {
			ZB,
			GP
		};
		
		// device id, either 64 bit ieee 802.15.4 long device address or 32 bit green power id
		DeviceId deviceId;

		// radio device type
		Type type;
		
		// device type from commissioning message
		uint8_t deviceType;
	
		// endpoints derived from device type (zero terminated list)
		EndpointType endpointTypes[4];

		union {
			// short device address
			uint16_t shortAddress;
			
			// device key for green power devices
			AesKey key;
		} u;
		
		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
		
		/**
		 * Returns the number of endpoints
		 * @return number of endpoints
		 */
		int getEndpointCount() const;
	};

	struct DeviceState {
		// security counter
		uint32_t counter;
		
		// endpoint subscriptions
		uint8_t firstEndpointId;
		uint8_t referenceCounters[4];
		
		uint8_t state;
	};


	// list of commissioned devices
	Storage::Array<Device, DeviceState> devices;

private:

	Coroutine receive();

	void handleAssociationRequest(uint64_t sourceAddress, PacketReader &d);

	void handleGp(uint8_t const *mac, PacketReader &d);
	void handleCommission(uint32_t deviceId, PacketReader& d);

	
	// ieee beacon
	Coroutine sendBeacon();
	
	// send beacon coroutine waits on this object until a beacon request arrives
	CoList<> beaconWait;
	
	// zb link status
	Coroutine sendLinkStatus();
	
	
	// radio context index
	static int const radioIndex = RADIO_ZB;

	// our ieee long address
	uint64_t longAddress;
	
	// id of our pan
	uint16_t panId;

	// network key
	AesKey const &networkKey;

	// callback
	std::function<void (uint8_t, uint8_t const *, int)> onReceived;

	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	// endpoint reference counters
	uint8_t nextEndpointId = 1;


	// counters
	uint8_t macSequenceNumber = 0;
	uint8_t nwkSequenceNumber = 0;
	uint32_t securityCounter = 0;
};
