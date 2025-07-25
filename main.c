

#include "src/wasm_api.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

// Benchmark related functions
static uint64_t getTimeUs();
static uint64_t runPartitionBenchmark_I(int partition_id, const char* func_name);
static void sched_cycle();
static void printInfo(int partition_id, int run);


int main(int argc, char** argv) {

    int benchmark_mode = (argc > 1 && strcmp(argv[1], "--benchmark") == 0);

    if(wasm_api_init() != WASM_API_OK) return 1;

    if(wasm_api_load_partition(0, "wasm/fib.wasm") != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_inject_fuel(0, FUEL_AMOUNT, true) != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_load_partition(1, "wasm/fib.wasm") != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_inject_fuel(1, FUEL_AMOUNT, true) != WASM_API_OK) return WASM_API_ERR;

    sched_cycle();

    // if(benchmark_mode) {
    //     printf("Running in benchmark mode\n");

    //     uint64_t total_time[2] = {0, 0};

    //     for(int run = 0; run < NUM_RUNS; run++) {
    //         printf("\n##################### Run %d: #####################\n", run + 1);
    //         uint64_t t0 = runPartitionBenchmark_I(0, "main");
    //         uint64_t t1 = runPartitionBenchmark_I(1, "main");

    //         printInfo(0, run);            
    //         printInfo(1, run);

    //         total_time[0] += t0;
    //         total_time[1] += t1;

    //     }

    //     printf("\nAverage time Partition 0: %lu µs\n", total_time[0] / NUM_RUNS);
    //     printf("Average time Partition 1: %lu µs\n", total_time[1] / NUM_RUNS);

    // }else {
    //     if(runPartitionBenchmark_I(0, "main") == (uint64_t) WASM_API_ERR) return 1;
    //     if(runPartitionBenchmark_I(1, "main") == (uint64_t) WASM_API_ERR) return 1;
    // }

    wasm_api_cleanup();

    return 0;
}


static uint64_t getTimeUs(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000000 + tv.tv_usec);
}

static uint64_t runPartitionBenchmark_I(int partition_id, const char* func_name) {
    uint64_t start = getTimeUs();

    if(wasm_api_run_partition(partition_id, func_name) != WASM_API_OK) return 0; 

    uint64_t end = getTimeUs();
    uint64_t duration = end - start;

    printf("Partition %d ran '%s' in %lu µs\n", partition_id, func_name, duration);

    return duration;
}

static void sched_cycle() {
    int num_partition = 0;

    while(1) {
        int current_partition = num_partition;
        partition_status status = wasm_api_run_partition(current_partition, "main");

        switch (status) {
            case PARTITION_DONE:
                printf("Partition %d finished execution\n", current_partition);
            break;

            case PARTITION_YIELDED:
                num_partition = (num_partition + 1) % NUM_MAX_PARTITIONS;
                printf("Partition %d yielded, executing Partition %d next\n", current_partition, num_partition);
            break;

            case PARTITION_ERROR:
                printf("Partition %d encountered an error\n", current_partition);
            break;

            default:
                printf("Unknown status from Partition %d\n", current_partition);
            break;
        }
    }
}


static void printInfo(int partition_id, int run) {
    uint64_t fuel_remaining = 0;
    wasmtime_context_get_fuel(get_wasm_partition(partition_id)->context, &fuel_remaining);
    printf("<<<<<<<<<<<<<<<<<<<< Partition %u >>>>>>>>>>>>>>>>>>>>\n", 0);
    printf("Fuel remaining: %lu\n", fuel_remaining);
    printf("Fuel used (total): %lu\n", (FUEL_AMOUNT - fuel_remaining));
    printf("Fuel used (this run): %lu\n", (FUEL_AMOUNT - fuel_remaining)/(run + 1));
}