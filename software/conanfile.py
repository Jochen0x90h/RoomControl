from conans import ConanFile, CMake, tools
import os, re, shutil

# linux:
# install conan: pip3 install --user conan
# upgrade conan: pip3 install --upgrade --user conan

# macos:
# install conan: brew install conan

# create default profile: conan profile new default --detect
# create debug profile: copy ~/.conan/profiles/default to Debug, replace Release by Debug

def get_version():
    git = tools.Git()
    try:
        # get version from git tag
        version = git.get_tag()
        if version is None:
            # not found: use branch name
            version = git.get_branch()
        return version.replace("/", "-").replace(" ", "_").replace("(", "").replace(")", "")
    except:
        None

def get_reference():
    return f"{Project.name}/{Project.version}@"

class Project(ConanFile):
    name = "emulator"
    version = get_version()
    description = "Emulator for RoomControl firmware"
    url = "https://github.com/Jochen0x90h/RoomControl"
    license="CC-BY-NC-SA-4.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "debug": [False, True]}
    default_options = {
        "debug": False,
        "boost:shared": True}
    generators = "cmake"
    exports_sources = "conanfile.py", "CMakeLists.txt", "src/*", "test/*"
    requires = \
        "boost/1.75.0", \
        "glfw/3.3.3", \
        "libusb/1.0.24", \
        "gtest/1.8.1"

    def configure_cmake(self):
        cmake = CMake(self, build_type = "RelWithDebInfo" if self.options.debug and self.settings.build_type == "Release" else None)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self.configure_cmake()
        cmake.build()
         
    def package(self):
        cmake = self.configure_cmake()
        cmake.install()
        
    def deploy(self):
        # libs to install (check with otool -L or readelf -d to find out)
        libs = ["libboost_system", "libboost_filesystem"]
        
        # install if CONAN_INSTALL_PREFIX env variable is set
        prefix = os.getenv("CONAN_INSTALL_PREFIX")
        if prefix == None:
            print(f"set CONAN_INSTALL_PREFIX env variable to install to local directory, e.g.")
            print(f"export CONAN_INSTALL_PREFIX=$HOME/.local")
        else:
            print(f"Installing {self.name} to {prefix}")
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
                files = os.listdir(srcBinPath)
                for file in files:
                    #print(f"file: {file}")
                    print(f"install {file}")
                    src = os.path.join(srcBinPath, file)
                    dst = os.path.join(dstBinPath, file)
                    if os.path.isfile(src):
                        shutil.copy2(src, dst, follow_symlinks=False)
                        if tools.detected_os() == "Macos":
                            # set rpath for macos, check with otool -l <executable>, see LC_RPATH commands
                            os.system(f"install_name_tool -add_rpath '@executable_path/../lib' '{dst}'")
                        else:
                            # set rpath for linux, check with readelf -d <executable>
                            os.system(f"patchelf --set-rpath '$ORIGIN/../lib' '{dst}'")
                            
            # copy libs
            # https://docs.conan.io/en/latest/reference/conanfile/attributes.html#deps-cpp-info
            for depName in self.deps_cpp_info.deps:
                #print(f"depName: {depName}")               
                for srcLibPath in self.deps_cpp_info[depName].lib_paths:
                    #print(f"srcLibPath: {srcLibPath}")
                    files = os.listdir(srcLibPath)
                    for file in files:
                        #print(f"file: {file}")
                        m = re.match("(.+)\.(so|dylib)(\.\d+)*", file)
                        if m:
                            if m.group(1) in libs or tools.detected_os() == "Macos": # -dead_strip_dylibs somehow does not work for boost
                                print(f"install {file}")
                                src = os.path.join(srcLibPath, file)
                                dst = os.path.join(dstLibPath, file)
                                if os.path.isfile(src):
                                    try:
                                        os.remove(dst)
                                    except OSError:
                                        pass
                                    shutil.copy2(src, dst, follow_symlinks=False)
