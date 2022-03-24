conan install . -o board=emuSwitch -if=build-emuSwitch-debug -pr emu-debug
cd build-emuSwitch-debug
cmake -G Xcode -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
