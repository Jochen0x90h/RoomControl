conan install . -if=build-emu-release -pr emu-release
cd build-emu-release
cmake -G Xcode -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
