#include "../rng.hpp"
#include <random>


namespace rng {

std::random_device rd;
std::mt19937 mt(rd());
std::uniform_int_distribution<uint64_t> dist;

void init() {
}

uint8_t int8() {
	return rng::dist(rng::mt);
}

uint64_t int64() {
	return rng::dist(rng::mt);
}

} // namespace rng
