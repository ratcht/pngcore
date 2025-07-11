# Makefile for libpngcore
# PNG System Library with concurrent processing support

# Compiler and flags
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -fPIC -I./include
LDFLAGS = -shared
LIBS = -lcurl -lz -lpthread -lrt

# Directories
SRCDIR = src
INCDIR = include/pngcore
OBJDIR = obj
LIBDIR = lib
BINDIR = bin
TESTDIR = tests
EXAMPLEDIR = examples

# Library name
LIBNAME = pngcore
STATIC_LIB = $(LIBDIR)/lib$(LIBNAME).a
SHARED_LIB = $(LIBDIR)/lib$(LIBNAME).so
VERSION = 1.0.0

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Header files
HEADERS = $(wildcard $(INCDIR)/*.h)
PUBLIC_HEADER = include/pngcore.h

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.c)
TEST_BINS = $(TEST_SOURCES:$(TESTDIR)/%.c=$(BINDIR)/test_%)

# Example files
EXAMPLE_SOURCES = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLE_BINS = $(EXAMPLE_SOURCES:$(EXAMPLEDIR)/%.c=$(BINDIR)/%)

# Default target
all: directories $(STATIC_LIB) $(SHARED_LIB)

# Create necessary directories
directories:
	@mkdir -p $(OBJDIR) $(LIBDIR) $(BINDIR)

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Build static library
$(STATIC_LIB): $(OBJECTS)
	@echo "Creating static library..."
	@$(AR) rcs $@ $^
	@echo "Static library created: $@"

# Build shared library
$(SHARED_LIB): $(OBJECTS)
	@echo "Creating shared library..."
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Shared library created: $@"

# Build tests
tests: all $(TEST_BINS)

$(BINDIR)/test_%: $(TESTDIR)/%.c $(STATIC_LIB)
	@echo "Building test $@..."
	@$(CC) $(CFLAGS) -o $@ $< -L$(LIBDIR) -l$(LIBNAME) $(LIBS)

# Build examples
examples: all $(EXAMPLE_BINS)

$(BINDIR)/%: $(EXAMPLEDIR)/%.c $(STATIC_LIB)
	@echo "Building example $@..."
	@$(CC) $(CFLAGS) -o $@ $< -L$(LIBDIR) -l$(LIBNAME) $(LIBS)

# Install library
install: all
	@echo "Installing libpngcore..."
	@mkdir -p /usr/local/lib /usr/local/include/pngcore
	@cp $(STATIC_LIB) $(SHARED_LIB) /usr/local/lib/
	@cp $(PUBLIC_HEADER) /usr/local/include/
	@cp $(HEADERS) /usr/local/include/pngcore/
	@ldconfig
	@echo "Installation complete!"

# Uninstall library
uninstall:
	@echo "Uninstalling libpngcore..."
	@rm -f /usr/local/lib/lib$(LIBNAME).*
	@rm -rf /usr/local/include/pngcore
	@rm -f /usr/local/include/pngcore.h
	@ldconfig
	@echo "Uninstallation complete!"

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR)
	@echo "Clean complete!"

# Clean everything including generated documentation
distclean: clean
	@rm -rf docs/html docs/latex

# Generate documentation
docs:
	@echo "Generating documentation..."
	@doxygen Doxyfile

# Run tests
test: tests
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

# Build specific example programs
paster2: examples
	@echo "paster2 binary is at: $(BINDIR)/paster2"

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Help
help:
	@echo "libpngcore build system"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build static and shared libraries (default)"
	@echo "  tests      - Build test programs"
	@echo "  examples   - Build example programs"
	@echo "  install    - Install library to /usr/local"
	@echo "  uninstall  - Remove library from /usr/local"
	@echo "  clean      - Remove build artifacts"
	@echo "  distclean  - Remove all generated files"
	@echo "  docs       - Generate documentation"
	@echo "  test       - Run tests"
	@echo "  debug      - Build with debug symbols"
	@echo "  help       - Show this message"
	@echo ""
	@echo "Examples:"
	@echo "  make              - Build libraries"
	@echo "  make examples     - Build example programs"
	@echo "  make paster2      - Build paster2 example"
	@echo "  make test         - Build and run tests"

.PHONY: all directories tests examples install uninstall clean distclean docs test debug help