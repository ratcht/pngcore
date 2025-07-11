# PNGCore

A systems-oriented PNG processing library written in C, featuring concurrent processing, network capabilities, and shared memory support.

**This library was adapted/inspired from the systems programming labs I did while taking ECE 252.**

## Overview

PNGCore is designed to demonstrate systems programming techniques while providing a fully functional PNG processing library. It showcases:

- **Multi-process concurrency** with producer-consumer patterns
- **Shared memory IPC** for zero-copy data sharing
- **Network programming** with libcurl for distributed PNG fetching
- **Low-level PNG format handling** with manual parsing and CRC validation
- **Memory-efficient processing** with circular buffers and semaphore synchronization

## Features

### Core PNG Operations
- Parse and validate PNG files
- Create PNG files from raw pixel data
- Extract and manipulate PNG chunks
- CRC validation and generation
- zlib compression/decompression of image data

### Concurrent Processing
- Producer-consumer architecture for parallel processing
- Shared memory circular buffers
- POSIX semaphore synchronization
- Multi-process PNG fragment assembly
- Configurable worker counts and buffer sizes

### Network Capabilities
- HTTP-based PNG fragment fetching
- Custom header parsing for sequence numbers
- Automatic fragment reassembly
- Built on libcurl for reliability

### Memory Efficiency
- Zero-copy operations using shared memory
- Configurable buffer sizes
- Efficient circular buffer implementation
- Minimal memory allocation during processing

## Installation

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential libcurl4-openssl-dev zlib1g-dev

# RHEL/CentOS
sudo yum install gcc make libcurl-devel zlib-devel

# macOS (with Homebrew)
brew install curl zlib
```

### Building from Source

```bash
git clone https://github.com/ratcht/pngcore.git
cd pngcore
make
sudo make install
```

### Verifying Installation

```bash
# Build and run examples
make examples
./bin/simple_read test.png
```

## Quick Start

### Reading a PNG File

```c
#include <pngcore.h>

int main() {
  pngcore_error_t error;
  
  // Load PNG file
  pngcore_png_t *png = pngcore_load_file("image.png", &error);
  if (!png) {
      fprintf(stderr, "Error: %s\n", error.message);
      return 1;
  }
  
  // Get properties
  printf("Width: %u\n", pngcore_get_width(png));
  printf("Height: %u\n", pngcore_get_height(png));
  
  // Clean up
  pngcore_free(png);
  return 0;
}
```

### Creating a PNG File

```c
// Create 256x256 RGBA PNG
pngcore_png_t *png = pngcore_create(256, 256, 8, PNGSYS_COLOR_RGBA);

// Generate pixel data (with filter bytes)
size_t size = 256 * (256 * 4 + 1);
uint8_t *pixels = malloc(size);
// ... fill pixel data ...

// Set pixels and save
pngcore_set_raw_data(png, pixels, size);
pngcore_save_file(png, "output.png", NULL);

// Clean up
free(pixels);
pngcore_free(png);
```

### Concurrent PNG Processing

```c
// Configure concurrent processor
pngcore_concurrent_config_t config = {
  .buffer_size = 10,
  .num_producers = 4,
  .num_consumers = 2,
  .consumer_delay = 0,
  .image_num = 1
};

// Create and run processor
pngcore_concurrent_t *proc = pngcore_concurrent_create(&config);
pngcore_concurrent_run(proc);

// Get assembled result
pngcore_png_t *result = pngcore_concurrent_get_result(proc);
pngcore_save_file(result, "assembled.png", NULL);

// Clean up
pngcore_free(result);
pngcore_concurrent_destroy(proc);
```

## API Reference

### Core Functions

| Function | Description |
|----------|-------------|
| `pngcore_load_file()` | Load PNG from file |
| `pngcore_load_buffer()` | Load PNG from memory buffer |
| `pngcore_save_file()` | Save PNG to file |
| `pngcore_create()` | Create new PNG structure |
| `pngcore_free()` | Free PNG structure |

### Property Access

| Function | Description |
|----------|-------------|
| `pngcore_get_width()` | Get image width |
| `pngcore_get_height()` | Get image height |
| `pngcore_get_bit_depth()` | Get bit depth |
| `pngcore_get_color_type()` | Get color type |

### Data Manipulation

| Function | Description |
|----------|-------------|
| `pngcore_get_raw_data()` | Extract uncompressed pixel data |
| `pngcore_set_raw_data()` | Set pixel data |
| `pngcore_validate()` | Validate PNG structure |

### Concurrent Processing

| Function | Description |
|----------|-------------|
| `pngcore_concurrent_create()` | Create concurrent processor |
| `pngcore_concurrent_run()` | Run processing |
| `pngcore_concurrent_get_result()` | Get assembled PNG |
| `pngcore_concurrent_destroy()` | Clean up processor |

## Architecture

### Library Structure

```
libpngcore/
├── Core PNG Module
│   ├── Raw PNG parsing
│   ├── Chunk validation
│   └── CRC calculation
├── Compression Module
│   ├── zlib integration
│   └── IDAT processing
├── Network Module
│   ├── HTTP fetching
│   └── Fragment handling
└── Concurrent Module
    ├── Shared memory management
    ├── Circular buffer
    └── Process coordination
```

### Memory Layout

The concurrent processor uses three shared memory segments:

1. **Circular Buffer** - Stores PNG fragments awaiting processing
2. **IDAT Buffer** - Accumulates decompressed image data
3. **Semaphores** - Synchronization primitives

### Circular Buffer Process Model

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  Producer 1 │────▶│              │────▶│ Consumer 1  │
├─────────────┤     │   Circular   │     ├─────────────┤
│  Producer 2 │────▶│    Buffer    │────▶│ Consumer 2  │
├─────────────┤     │              │     └─────────────┘
│  Producer N │────▶│  (Shared Mem)│
└─────────────┘     └──────────────┘
```

## Examples

The `examples/` directory contains several demonstration programs:

- `simple_read.c` - Basic PNG reading and property inspection
- `paster2.c` - Concurrent PNG fragment fetching and assembly
- `validate_png.c` - PNG validation and chunk inspection

Build all examples:
```bash
make examples
```