# Developer convenience targets for code quality tooling.
# Usage: make <target> from the MuMain/ directory.

SOURCES := $(shell find src/source -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | grep -v ThirdParty/)

.PHONY: format format-check lint tidy hooks test

# Format all C++ source files in-place
format:
	@echo "Formatting $(words $(SOURCES)) files..."
	@echo $(SOURCES) | xargs clang-format -i
	@echo "Done."

# Check formatting without modifying files (exits non-zero on diff)
format-check:
	@echo "Checking formatting..."
	@echo $(SOURCES) | xargs clang-format --dry-run --Werror

# Run cppcheck static analysis
lint:
	@echo "Running cppcheck..."
	@echo $(SOURCES) | xargs cppcheck \
		--error-exitcode=1 \
		--enable=warning,performance,portability \
		--std=c++20 \
		--suppress=missingInclude \
		--suppress=unmatchedSuppression \
		--suppress=unusedFunction \
		--inline-suppr

# Run clang-tidy (requires a build directory with compile_commands.json)
tidy:
	@if [ ! -f build-mingw/compile_commands.json ]; then \
		echo "Error: build-mingw/compile_commands.json not found."; \
		echo "Run cmake configure first to generate it."; \
		exit 1; \
	fi
	@echo "Running clang-tidy..."
	@echo $(SOURCES) | xargs clang-tidy -p build-mingw

# Install git hooks
hooks:
	@bash scripts/install-hooks.sh

# Build and run unit tests
test:
	cmake -S . -B build-test -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
	cmake --build build-test --target MuTests -j$$(nproc)
	ctest --test-dir build-test --output-on-failure
