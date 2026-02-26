# Developer convenience targets for code quality tooling.
# Usage: make <target> from the MuMain/ directory.

SOURCES := $(shell find src/source -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | grep -v ThirdParty/ | grep -v 'src/source/Dotnet/')

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
#
# Suppression policy (see also: docs/cppcheck-guidance.md)
#
#   GLOBALLY SUPPRESSED (style-only, never indicate bugs):
#     missingInclude        — cppcheck can't resolve Win32/DirectX/third-party paths
#     unmatchedSuppression  — avoids noise across cppcheck versions
#     unusedFunction        — too many false positives without whole-program analysis
#     useInitializationList — style preference, not a correctness issue
#     passedByValue         — performance style (pass-by-const-ref)
#     postfixOperator       — trivial perf (++i vs i++)
#     returnByReference     — performance style
#     *:*/ThirdParty/*      — don't lint third-party code
#
#   ENFORCED (catch real bugs — legacy violations use inline cppcheck-suppress):
#     uninitMemberVar / uninitMemberVarPrivate — uninitialized members
#     dangerousTypeCast    — unsafe C-style casts
#     noOperatorEq / noCopyConstructor — Rule of Three violations
#     duplInheritedMember  — accidental base class member shadowing
#
#   When adding new code, do NOT add inline suppressions for these enforced
#   checks. Fix the issue instead. Existing suppressions are legacy baseline.
#
lint:
	@echo "Running cppcheck..."
	@echo $(SOURCES) | xargs cppcheck \
		-j$$(nproc) \
		--error-exitcode=1 \
		--enable=warning,performance,portability \
		--std=c++20 \
		--language=c++ \
		--suppress=missingInclude \
		--suppress=unmatchedSuppression \
		--suppress=unusedFunction \
		--suppress=*:*/ThirdParty/* \
		--suppress=useInitializationList \
		--suppress=passedByValue \
		--suppress=postfixOperator \
		--suppress=returnByReference \
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
