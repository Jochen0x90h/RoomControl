conan install . -if=build-nrf52dongle -pr nrf52dongle
cd build-nrf52dongle
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
cmake --build . --target all
