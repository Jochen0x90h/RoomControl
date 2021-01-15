mkdir -p build-xcode
cd build-xcode
cmake -DCMAKE_PREFIX_PATH=$HOME/.local -G Xcode ../
