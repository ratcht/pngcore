/**
 * @file simple_read.c
 * @brief Example of basic PNG reading using libpngcore
 */

#include <pngcore.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <png_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    pngcore_error_t error;
    
    /* Clear error structure */
    pngcore_error_clear(&error);
    
    printf("Loading PNG file: %s\n", filename);
    
    /* Load PNG file */
    pngcore_png_t *png = pngcore_load_file(filename, &error);
    if (!png) {
        fprintf(stderr, "Error loading PNG: %s (code: %d)\n", 
                error.message, error.code);
        return 1;
    }
    
    /* Get and display PNG properties */
    uint32_t width = pngcore_get_width(png);
    uint32_t height = pngcore_get_height(png);
    uint8_t bit_depth = pngcore_get_bit_depth(png);
    uint8_t color_type = pngcore_get_color_type(png);
    
    printf("\nPNG Properties:\n");
    printf("  Width: %u pixels\n", width);
    printf("  Height: %u pixels\n", height);
    printf("  Bit depth: %u\n", bit_depth);
    printf("  Color type: ");
    
    switch (color_type) {
        case PNGCORE_COLOR_GRAYSCALE:
            printf("Grayscale\n");
            break;
        case PNGCORE_COLOR_RGB:
            printf("RGB\n");
            break;
        case PNGCORE_COLOR_INDEXED:
            printf("Indexed color\n");
            break;
        case PNGCORE_COLOR_GRAYSCALE_ALPHA:
            printf("Grayscale with alpha\n");
            break;
        case PNGCORE_COLOR_RGBA:
            printf("RGBA\n");
            break;
        default:
            printf("Unknown (%u)\n", color_type);
    }
    
    /* Validate PNG */
    if (pngcore_validate(png)) {
        printf("\nPNG validation: PASSED\n");
    } else {
        printf("\nPNG validation: FAILED\n");
    }
    
    /* Get raw pixel data */
    uint8_t *pixels;
    size_t pixel_size;
    
    printf("\nExtracting pixel data...\n");
    if (pngcore_get_raw_data(png, &pixels, &pixel_size) == 0) {
        printf("Successfully extracted %zu bytes of pixel data\n", pixel_size);
        
        /* Calculate expected size */
        int channels = 1;
        switch (color_type) {
            case PNGCORE_COLOR_RGB: channels = 3; break;
            case PNGCORE_COLOR_RGBA: channels = 4; break;
            case PNGCORE_COLOR_GRAYSCALE_ALPHA: channels = 2; break;
        }
        size_t row_bytes = width * channels * (bit_depth / 8);
        size_t expected_size = height * (row_bytes + 1); /* +1 for filter byte */
        
        printf("Expected size: %zu bytes (matches: %s)\n", 
               expected_size, (pixel_size == expected_size) ? "yes" : "no");
        
        /* Show first few bytes of pixel data */
        printf("\nFirst 16 bytes of pixel data:\n");
        for (int i = 0; i < 16 && i < pixel_size; i++) {
            printf("%02X ", pixels[i]);
        }
        printf("\n");
        
        free(pixels);
    } else {
        fprintf(stderr, "Failed to extract pixel data\n");
    }
    
    /* Check specific chunks */
    printf("\nChecking chunks:\n");
    
    pngcore_chunk_t *ihdr = pngcore_get_chunk(png, "IHDR");
    if (ihdr) {
        size_t ihdr_size;
        const uint8_t *ihdr_data = pngcore_chunk_get_data(ihdr, &ihdr_size);
        uint32_t ihdr_crc = pngcore_chunk_get_crc(ihdr);
        printf("  IHDR: %zu bytes, CRC: 0x%08X\n", ihdr_size, ihdr_crc);
        free(ihdr);
    }
    
    pngcore_chunk_t *idat = pngcore_get_chunk(png, "IDAT");
    if (idat) {
        size_t idat_size;
        const uint8_t *idat_data = pngcore_chunk_get_data(idat, &idat_size);
        uint32_t idat_crc = pngcore_chunk_get_crc(idat);
        printf("  IDAT: %zu bytes, CRC: 0x%08X\n", idat_size, idat_crc);
        free(idat);
    }
    
    /* Save a copy of the PNG */
    printf("\nSaving copy as 'copy.png'...\n");
    if (pngcore_save_file(png, "copy.png", &error) == 0) {
        printf("Successfully saved copy\n");
    } else {
        fprintf(stderr, "Error saving copy: %s\n", error.message);
    }
    
    /* Clean up */
    pngcore_free(png);
    
    printf("\nDone!\n");
    return 0;
}