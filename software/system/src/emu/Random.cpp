#include "../Random.hpp"
#include <random>


namespace Random {

std::random_device rd;
std::mt19937 mt(rd());
std::uniform_int_distribution<uint64_t> dist;

void init() {
}

uint8_t int8() {
	return Random::dist(Random::mt);
}

uint64_t int64() {
	return Random::dist(Random::mt);
}

} // namespace Random
