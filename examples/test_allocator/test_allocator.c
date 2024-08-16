/**

 */

#include "pico/stdlib.h"
#include "sparkfun_pico/sfe_pico_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// main()

static void memory_stats()
{
    size_t mem_size = sfe_mem_size();
    size_t mem_used = sfe_mem_used();
    printf("\tMemory pool - Total: 0x%X (%u)  Used: 0x%X (%u) - %3.2f%%\n", mem_size, mem_size, mem_used, mem_used,
           (float)mem_used / (float)mem_size * 100.0);

    size_t max_block = sfe_mem_max_free_size();
    printf("\tMax free block size: 0x%X (%u) \n", max_block, max_block);
}
int main()
{
    stdio_init_all();

    // wait a little bit - for startup
    sleep_ms(2000);
    printf("\n-----------------------------------------------------------\n");
    printf("SparkFun - Allocator test - starting\n");
    sleep_ms(2000);

    sfe_pico_alloc_init();
    printf("Startup\n");

    memory_stats();

    // Allocate a Meg
    uint8_t *big_block = (uint8_t *)sfe_mem_malloc(1024 * 1024);
    if (!big_block)
    {
        printf("Big block allocation failed\n");
        return 1;
    }
    printf("\nAllocated a Meg using sfe_alloc\n");
    memory_stats();
    sfe_mem_free(big_block);

    printf("\nFreed a Meg using sfe_free\n");
    memory_stats();

// Wrapping the built in malloc/free
#if defined(SFE_PICO_ALLOC_WRAP)
    // Now with built ins -- did we override ?
    // Allocate a Meg
    big_block = (uint8_t *)malloc(1024 * 1024);
    if (!big_block)
    {
        printf("Big block built in allocation failed\n");
        return 1;
    }
    printf("\nAllocated a Meg using built in alloc\n");
    memory_stats();
    free(big_block);

    printf("\nFreed a Meg using built in free\n");
    memory_stats();
#endif
    printf("DONE\n");
    printf("-----------------------------------------------------------\n");
    while (1)
    {
        sleep_ms(1000);
    }
}
