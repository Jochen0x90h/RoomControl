import os, shutil
from conans import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake

# linux:
# install conan: pip3 install --user conan
# upgrade conan: pip3 install --upgrade --user conan

# macos:
# install conan: brew install conan

# create default profile: conan profile new default --detect
# create debug profile: copy ~/.conan/profiles/default to Debug, replace Release by Debug

# helper function for deploy()
def copy(src, dst):
    if os.path.islink(src):
        if os.path.lexists(dst):
            os.unlink(dst)
        linkto = os.readlink(src)
        os.symlink(linkto, dst)
    else:
        shutil.copy(src, dst)

class Project(ConanFile):
    name = "roomcontrol"
    description = "Emulator for RoomControl firmware"
    url = "https://github.com/Jochen0x90h/RoomControl"
    license="CC-BY-NC-SA-4.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "platform": "ANY",
        "board": "ANY",
        "mcu": "ANY",
        "cpu": "ANY",
        "fpu": "ANY"}
    default_options = {
        "platform": None,
        "board": None,
        "mcu": None,
        "cpu": None,
        "fpu": None}
    generators = "CMakeDeps"
    exports_sources = [
        "CMakeLists.txt",
        "util/*",
        "network/*",
        "system/*",
        "node/*",
        "control/*",
        "glad/*",
        "tools/*"
    ]

    def requirements(self):
        p = str(self.options.platform if self.options.platform else self.settings.os)
        if str(self.settings.os) in p:
            self.requires("libusb/1.0.24")
            self.requires("boost/1.79.0")
            self.requires("gtest/1.11.0")
        if "emu" in p:
            self.requires("glfw/3.3.7")

    keep_imports = True
    def imports(self):
        # copy dependent libraries into the build folder
        self.copy("*", src="@bindirs", dst="bin")
        self.copy("*", src="@libdirs", dst="lib")

    def generate(self):
        p = str(self.options.platform if self.options.platform else self.settings.os)

        # generate "conan_toolchain.cmake"
        toolchain = CMakeToolchain(self)
        toolchain.variables["PLATFORM"] = p #self.options.platform if self.options.platform else self.settings.os
        toolchain.variables["BOARD"] = self.options.board
        toolchain.variables["MCU"] = self.options.mcu
        toolchain.variables["CPU"] = self.options.cpu
        toolchain.variables["FPU"] = self.options.fpu

        # https://github.com/conan-io/conan/blob/develop/conan/tools/cmake/toolchain.py
        if str(self.settings.os) not in p:
            toolchain.blocks["generic_system"].values["cmake_system_name"] = "Generic"
            toolchain.blocks["generic_system"].values["cmake_system_processor"] = self.settings.arch
            toolchain.variables["CMAKE_TRY_COMPILE_TARGET_TYPE"] = "STATIC_LIBRARY"
            toolchain.variables["CMAKE_FIND_ROOT_PATH_MODE_PROGRAM"] = "NEVER"
            toolchain.variables["CMAKE_FIND_ROOT_PATH_MODE_LIBRARY"] = "ONLY"
            toolchain.variables["CMAKE_FIND_ROOT_PATH_MODE_INCLUDE"] = "ONLY"
            toolchain.variables["GENERATE_HEX"] = self.env.get("GENERATE_HEX", None)

        # bake CC and CXX from profile into toolchain
        toolchain.blocks["generic_system"].values["compiler"] = self.env.get("CC", None)
        toolchain.blocks["generic_system"].values["compiler_cpp"] = self.env.get("CXX", None)

        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # run unit tests if CONAN_RUN_TESTS environment variable is set to 1
        if os.getenv("CONAN_RUN_TESTS") == "1":
            cmake.test()

    def package(self):
        # install from build directory into package directory
        cmake = CMake(self)
        cmake.install()

        # also copy dependent libraries into the package
        self.copy("*.dll", "bin", "bin")
        self.copy("*.dylib*", "lib", "lib", symlinks = True)
        self.copy("*.so*", "lib", "lib", symlinks = True)

    def package_info(self):
        pass

    def deploy(self):
        # install if CONAN_INSTALL_PREFIX env variable is set
        prefix = os.getenv("CONAN_INSTALL_PREFIX")
        if prefix == None:
            print(f"set CONAN_INSTALL_PREFIX env variable to install to local directory, e.g.")
            print(f"export CONAN_INSTALL_PREFIX=$HOME/.local")
        else:
            print(f"Installing {self.name} to {prefix}")

            # create destination directories if necessary
            dstBinPath = os.path.join(prefix, "bin")
            if not os.path.exists(dstBinPath):
                os.mkdir(dstBinPath)
            #print(f"dstBinPath: {dstBinPath}")
            dstLibPath = os.path.join(prefix, "lib")
            if not os.path.exists(dstLibPath):
                os.mkdir(dstLibPath)
            #print(f"dstLibPath: {dstLibPath}")

            # copy executables
            for bindir in self.cpp_info.bindirs:
                srcBinPath = os.path.join(self.cpp_info.rootpath, bindir)
                #print(f"srcBinPath {srcBinPath}")
                if os.path.isdir(srcBinPath):
                    files = os.listdir(srcBinPath)
                    for file in files:
                        print(f"install {file}")
                        src = os.path.join(srcBinPath, file)
                        dst = os.path.join(dstBinPath, file)
                        if os.path.isfile(src):
                            copy(src, dst)

            # copy libraries
            for libdir in self.cpp_info.libdirs:
                srcLibPath = os.path.join(self.cpp_info.rootpath, libdir)
                #print(f"srcLibPath {srcLibPath}")
                if os.path.isdir(srcLibPath):
                    files = os.listdir(srcLibPath)
                    for file in files:
                        print(f"install {file}")
                        src = os.path.join(srcLibPath, file)
                        dst = os.path.join(dstLibPath, file)
                        if os.path.isfile(src):
                            copy(src, dst)
