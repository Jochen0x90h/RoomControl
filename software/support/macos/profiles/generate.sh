# This script creates the necessary conan profiles in ~/.conan/profiles 

DEBUG="s/build_type=/build_type=Debug/g"
RELEASE="s/build_type=/build_type=Release/g"
MACOS_EMU="s/platform=/platform=Macos;emu/g"
PROFILES=$HOME/.conan/profiles

# macos-debug
sed -e $DEBUG -e "/platform=/d" -e "/board=/d" template > $PROFILES/macos-debug

# macos-release
sed -e $RELEASE -e "/platform=/d" -e "/board=/d" template > $PROFILES/macos-release

# emuControl-debug
sed -e $DEBUG -e $MACOS_EMU -e "s/board=/board=emuControl/g" template > $PROFILES/emuControl-debug

# emuControl-release
sed -e $RELEASE -e $MACOS_EMU -e "s/board=/board=emuControl/g" template > $PROFILES/emuControl-release

# emuSwitch-debug
sed -e $DEBUG -e $MACOS_EMU -e "s/board=/board=emuSwitch/g" template > $PROFILES/emuSwitch-debug

# emuSwitch-release
sed -e $RELEASE -e $MACOS_EMU -e "s/board=/board=emuSwitch/g" template > $PROFILES/emuSwitch-release
