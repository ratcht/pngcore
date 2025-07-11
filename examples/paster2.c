/**
 * @file paster2.c
 * @brief Example program using libpngcore for concurrent PNG fetching
 * 
 * Usage: ./paster2 <b> <p> <c> <x> <n>
 *   b: buffer size (1-50)
 *   p: number of producers (1-20)
 *   c: number of consumers (1-20)
 *   x: consumer delay in ms (0-1000)
 *   n: image number (1-3)
 */

#include <pngcore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 6) {
        printf("Usage: %s <b> <p> <c> <x> <n>\n", argv[0]);
        printf("  b: buffer size (1-50)\n");
        printf("  p: number of producers (1-20)\n");
        printf("  c: number of consumers (1-20)\n");
        printf("  x: consumer delay in ms (0-1000)\n");
        printf("  n: image number (1-3)\n");
        return 1;
    }

    /* Parse command line arguments */
    int b = atoi(argv[1]);
    int p = atoi(argv[2]);
    int c = atoi(argv[3]);
    int x = atoi(argv[4]);
    int n = atoi(argv[5]);

    /* Validate arguments */
    if (b < 1 || b > 50) {
        fprintf(stderr, "Error: buffer size must be between 1 and 50\n");
        return 1;
    }
    if (p < 1 || p > 20) {
        fprintf(stderr, "Error: number of producers must be between 1 and 20\n");
        return 1;
    }
    if (c < 1 || c > 20) {
        fprintf(stderr, "Error: number of consumers must be between 1 and 20\n");
        return 1;
    }
    if (x < 0 || x > 1000) {
        fprintf(stderr, "Error: consumer delay must be between 0 and 1000 ms\n");
        return 1;
    }
    if (n < 1 || n > 3) {
        fprintf(stderr, "Error: image number must be between 1 and 3\n");
        return 1;
    }

    printf("Configuration:\n");
    printf("  Buffer size: %d\n", b);
    printf("  Producers: %d\n", p);
    printf("  Consumers: %d\n", c);
    printf("  Consumer delay: %d ms\n", x);
    printf("  Image number: %d\n", n);
    printf("\n");

    /* Create concurrent processor configuration */
    pngcore_concurrent_config_t config = {
        .buffer_size = b,
        .num_producers = p,
        .num_consumers = c,
        .consumer_delay = x,
        .image_num = n
    };

    /* Create concurrent processor */
    printf("Creating concurrent processor...\n");
    pngcore_concurrent_t *proc = pngcore_concurrent_create(&config);
    if (!proc) {
        fprintf(stderr, "Error: Failed to create concurrent processor\n");
        return 1;
    }

    /* Run the concurrent processing */
    printf("Starting concurrent PNG fetching...\n");
    if (pngcore_concurrent_run(proc) != 0) {
        fprintf(stderr, "Error: Failed to run concurrent processing\n");
        pngcore_concurrent_destroy(proc);
        return 1;
    }

    /* Get the assembled result */
    printf("Assembling final PNG...\n");
    pngcore_png_t *result = pngcore_concurrent_get_result(proc);
    if (!result) {
        fprintf(stderr, "Error: Failed to get assembled PNG\n");
        pngcore_concurrent_destroy(proc);
        return 1;
    }

    /* Save the result */
    printf("Saving result to all.png...\n");
    pngcore_error_t error;
    if (pngcore_save_file(result, "all.png", &error) != 0) {
        fprintf(stderr, "Error saving PNG: %s\n", error.message);
        pngcore_free(result);
        pngcore_concurrent_destroy(proc);
        return 1;
    }

    /* Get execution time */
    double exec_time = pngcore_concurrent_get_time(proc);
    printf("\npaster2 execution time: %.2f seconds\n", exec_time);

    /* Clean up */
    pngcore_free(result);
    pngcore_concurrent_destroy(proc);

    return 0;
}