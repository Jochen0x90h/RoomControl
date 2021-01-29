#include "SystemTimer.hpp"
#include <iostream>
#include <chrono>


SystemTimer::~SystemTimer() {
}

SystemTime SystemTimer::getSystemTime() {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	return {uint32_t(us.count() * 128 / 125000)};
}

static std::chrono::steady_clock::time_point toChronoTime(SystemTime time) {
	auto now = std::chrono::steady_clock::now();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
	SystemDuration d = time - SystemTime{uint32_t(us.count() * 128 / 125000)};
	return now + std::chrono::microseconds((long long)d.value * 125000 / 128);
}

void SystemTimer::setSystemTimeout1(SystemTime time) {
	// https://www.boost.org/doc/libs/1_67_0/doc/html/boost_asio/reference/steady_timer.html
	this->emulatorTimer1.expires_at(toChronoTime(time));
	this->emulatorTimer1.async_wait([this, time] (boost::system::error_code error) {
		if (!error) {
			onSystemTimeout1(time);
		}
	});
}

void SystemTimer::stopSystemTimeout1() {
	this->emulatorTimer1.cancel();
}


void SystemTimer::setSystemTimeout2(SystemTime time) {
	this->emulatorTimer2.expires_at(toChronoTime(time));
	this->emulatorTimer2.async_wait([this, time] (boost::system::error_code error) {
		if (!error) {
			onSystemTimeout2(time);
		}
	});
}

void SystemTimer::stopSystemTimeout2() {
	this->emulatorTimer2.cancel();
}


void SystemTimer::setSystemTimeout3(SystemTime time) {
	this->emulatorTimer3.expires_at(toChronoTime(time));
	this->emulatorTimer3.async_wait([this, time] (boost::system::error_code error) {
		if (!error) {
			onSystemTimeout3(time);
		}
	});
}

void SystemTimer::stopSystemTimeout3() {
	this->emulatorTimer3.cancel();
}
