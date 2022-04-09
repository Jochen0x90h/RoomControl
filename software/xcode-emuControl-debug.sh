conan install . -if=build-emuControl-debug -pr emuControl-debug
cd build-emuControl-debug
cmake -G Xcode -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
