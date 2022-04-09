conan install . -if=build-emuControl-release -pr emuControl-release
cd build-emuControl-release
cmake -G Xcode -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake ../
