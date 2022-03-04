conan install . -if=build-emu-debug -pr emu-debug
cd build-emu-debug
cmake -G Xcode -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
