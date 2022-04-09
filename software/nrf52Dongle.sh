conan install . -if=build-nrf52Dongle -pr nrf52Dongle
cd build-nrf52Dongle
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
cmake --build . --target all
