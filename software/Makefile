.PHONY: release test debug reldeb install upload clean

REFERENCE := $(shell python3 -c 'from conanfile import get_reference; print(get_reference())' 2>/dev/null)

all: debug release

# build release and export conan package
release:
	conan install conanfile.py --install-folder build --update
	conan export-pkg . $(REFERENCE) --build-folder build -f

# run tests
test:
	cd build && make test

# build debug and export conan package
debug:
	conan install conanfile.py --install-folder build-debug --profile Debug --update
	conan export-pkg . $(REFERENCE) --build-folder build-debug --profile Debug -f

# build release with debug info
reldeb:
	conan install conanfile.py --install-folder build-reldeb --options debug=True --update
	conan export-pkg . $(REFERENCE) --build-folder build-reldeb --options debug=True -f

# install to ~/.local
install:
	conan install conanfile.py --install-folder build --update
	conan package . --build-folder build --package-folder "${HOME}/.local"

# upload package to conan repository
upload:
	conan upload $(REFERENCE) --all --force

clean:
	rm -rf build build-debug build-reldeb
