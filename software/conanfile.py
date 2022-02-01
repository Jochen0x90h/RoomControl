import os, shutil
from conans import ConanFile, CMake

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
        "debug": [False, True]}
    default_options = {
        "debug": False}
    generators = "cmake"
    exports_sources =\
        "CMakeLists.txt",\
        "util/*",\
        "network/*",\
        "system/*",\
        "node/*",\
        "control/*",\
        "glad/*",\
        "tools/*"
    requires = \
        "boost/1.77.0", \
        "glfw/3.3.5", \
        "libusb/1.0.24", \
        "gtest/1.11.0"


    keep_imports = True
    def imports(self):
        # copy dependent libraries into the build folder
        self.copy("*", src="@bindirs", dst="bin")
        self.copy("*", src="@libdirs", dst="lib")

    def configure_cmake(self):
        cmake = CMake(self, build_type = "RelWithDebInfo" if self.options.debug and self.settings.build_type == "Release" else None)
        cmake.configure()
        return cmake

    def build(self):
        # build
        cmake = self.configure_cmake()
        cmake.build()

        # run unit tests if CONAN_RUN_TESTS environment variable is set
        if os.getenv("CONAN_RUN_TESTS"):
            cmake.test()

    def package(self):
        # install from build directory into package directory
        cmake = self.configure_cmake()
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
