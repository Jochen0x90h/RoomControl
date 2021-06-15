.PHONY: release debug install upload

# name of conan package
NAME := $(shell python3 -c 'from conanfile import Project; print(Project.name)')

# get version from git branch or tag
left := (
right := )
BRANCH := $(shell git tag -l --points-at HEAD)
ifeq ($(BRANCH),)
	BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
endif
VERSION := $(subst /,-,$(subst $(left),_,$(subst $(right),_,$(BRANCH))))

# default target
all: release debug

# build release and run unit tests
release: export CONAN_RUN_TESTS=1
release:
	conan create . $(VERSION)@

# build debug and run unit tests
debug: export CONAN_RUN_TESTS=1
debug:
	conan create --profile Debug . $(VERSION)@

# install to ~/.local
install: export CONAN_INSTALL_PREFIX=${HOME}/.local
install:
	conan install $(NAME)/$(VERSION)@

# upload to artifact repository
upload:
	conan upload $(NAME)/$(VERSION)@
	conan upload --profile Debug $(NAME)/$(VERSION)@
