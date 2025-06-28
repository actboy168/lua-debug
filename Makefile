
.PHONY: all download_deps build_luamake init_luamake install

all: download_deps build_luamake
	luamake/luamake build --release

build_luamake: init_luamake
	@echo "Building luamake..."
	git submodule update --init --recursive
	cd luamake && compile/build.sh

download_deps: build_luamake
	luamake/luamake lua compile/download_deps.lua

init_luamake:
	@if [ -d "luamake" ]; then \
		if [ -d "luamake/.git" ]; then \
			echo "Updating luamake repository..."; \
			(cd luamake && git pull && git submodule update --remote); \
		else \
			echo "Error: 'luamake' exists but is not a Git repository."; \
		fi; \
	else \
		echo "Cloning luamake repository..."; \
		git clone --recurse-submodules https://github.com/actboy168/luamake.git; \
	fi

install: all
	@echo "Installing Lua Debugger..."
	luamake/luamake lua compile/install.lua
