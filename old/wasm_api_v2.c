/*
 * wasm_api.c
 *
 *  Created on: Jul 17, 2025
 *      Author: tdhoang
 */

// https://github.com/bytecodealliance/wasmtime/blob/main/examples/gcd.c

/****************************************************************************
 * Includes
****************************************************************************/
#include "wasm_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <wasm.h>
#include <wasmtime.h>


/****************************************************************************
 * Defines
****************************************************************************/
#define ERR            -1
#define TRAP            0
#define FUEL_AMOUNT     10000000
#define NUM_RUNS        100

/****************************************************************************
 * Wasm/Wasmtime related instances
****************************************************************************/
wasm_engine_t *engine;
wasm_config_t *config;

/****************************************************************************
 * Wasm Partition Struct
****************************************************************************/
struct wasm_partition {
    wasmtime_module_t *module;
    wasmtime_instance_t instance;
    wasmtime_context_t *context;
    wasmtime_store_t *store;
    int partition_id;
    bool instantiated;
};

wasm_partition* partitions[NUM_MAX_PARTITIONS] = {NULL};

/****************************************************************************
 * Static Function Prototypes
****************************************************************************/
static int catch_err(int errType, const char* msgPrint, wasmtime_error_t* err, wasm_trap_t* trap);
static int printFuelUsage(int partition_id);
static int partitionIdValid(int id);

// Benchmark related functions
static uint64_t getTimeUs();
static uint64_t runPartitionBenchmark(int partition_id, const char* func_name);

/****************************************************************************
 * Static Function Implementations
****************************************************************************/

/**
 * @brief Catches errors or traps and prints a custom message 
 *
 * @param errType Either ERR or TRAP for identification
 * @param msgPrint String to be printed when error/trap is caught
 * @param err Error or NULL if its a trap
 * @param trap Trap or NULL if its an error
 * @return WASM_API_ERR
 */
static int catch_err(int errType, const char* msgPrint, wasmtime_error_t* err, wasm_trap_t* trap) {

    if((errType != ERR && errType != TRAP) || (errType == ERR && err == NULL) || (errType == TRAP && trap == NULL)) {
        return WASM_API_ERR;
    }

    wasm_byte_vec_t msg;

    switch(errType) {
        case ERR:
                wasmtime_error_message(err, &msg);
                fprintf(stderr, "%s: %.*s\n", msgPrint, (int)msg.size, msg.data);
                wasm_byte_vec_delete(&msg);
                wasmtime_error_delete(err);
                return WASM_API_ERR;
        break;

        case TRAP:
                wasm_trap_message(trap, &msg);
                fprintf(stderr, "%s: %.*s\n", msgPrint, (int)msg.size, msg.data);
                wasm_byte_vec_delete(&msg);
                wasm_trap_delete(trap);
                return WASM_API_ERR;
        break;

        default:
            return WASM_API_ERR;
    }

}

/**
 * @brief Prints fuel remaining and fuel used  
 *
 * @return WASM_API_OK
 */
static int printFuelUsage(int partition_id) {
    uint64_t fuel_remaining = 0;

    wasmtime_context_get_fuel(partitions[partition_id]->context, &fuel_remaining);
    printf("<<<<<<<<<<<<<<<<<<<< Partition %u >>>>>>>>>>>>>>>>>>>>\n", partition_id);
    printf("Fuel remaining: %lu\n", fuel_remaining);
    printf("Fuel used: %lu\n", (FUEL_AMOUNT - fuel_remaining));
        
    return WASM_API_OK;
}


/**
 * @brief Bounds check for partition Id, for array bounds, so max is NUM_MAX_PARTITIONS - 1
 *
 * @return WASM_API_OK, else WASM_API_ERR
 */
static int partitionIdValid(int id) {
    if(id < 0 || id > NUM_MAX_PARTITIONS){
        printf("Invalid partition Id %d\n", id);
        return WASM_API_ERR;
    }else {
        return WASM_API_OK;
    }
}


static uint64_t getTimeUs(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000000 + tv.tv_usec);
}

static uint64_t runPartitionBenchmark(int partition_id, const char* func_name) {
    uint64_t start = getTimeUs();

    if(wasm_api_run_partition(partition_id, func_name) != WASM_API_OK) return 0; 

    uint64_t end = getTimeUs();
    uint64_t duration = end - start;

    printf("Partition %d ran '%s' in %lu µs\n", partition_id, func_name, duration);

    return duration;
}

/****************************************************************************
 * Function Implementations
****************************************************************************/

/**
 * @brief Initialize the Wasm engine and context globally 
 *
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_init(void) {
    // Enable fuel consumption
    config = wasm_config_new();
    wasmtime_config_consume_fuel_set(config, true);

    // Async Support
    // wasmtime_config_async_support_set(config, true);

    engine = wasm_engine_new_with_config(config);
    if(!engine) {
        printf("Failed to create Wasmtime engine\n");
        return WASM_API_ERR;
    }

    return WASM_API_OK;    
}


/**
 * @brief Load Wasm module from file and instantiate it
 *
 * @param wasm_file Wasm module to be read
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_load_partition(int partition_id, const char* wasm_file) {

    /* Create Wasm partition struct */

    // partition_id checks
    if(partitionIdValid(partition_id) != WASM_API_OK) {
        return WASM_API_ERR;
    }

    // Check if already loaded
    if(partitions[partition_id] != NULL) {
        printf("Partition %d already loaded\n", partition_id);
        return WASM_API_ERR;
    }

    // Allocate Mem for a partition
    wasm_partition *partition = malloc(sizeof(wasm_partition));
    if(!partition) {
        printf("Memory allocation failed!\n");
        return WASM_API_ERR;
    }

    // Store in global array 
    partitions[partition_id] = partition;

    partition->partition_id = partition_id;

    // Create Wasm related instances and assign
    partition->store = wasmtime_store_new(engine, NULL, NULL);
    if(!partition->store) {
        printf("Failed to create Wasmtime store\n");
        return WASM_API_ERR;
    }
    
    partition->context = wasmtime_store_context(partition->store);

    partitions[partition_id]->module = NULL;
    partitions[partition_id]->instantiated = false;


    /* Read .wasm content */

    // Open Wasm file
    FILE *file = fopen(wasm_file, "rb");
    if(!file) {
        printf("> Error loading file: %s\n", wasm_file);
        return WASM_API_ERR;
    }

    // Move file pointer to end for fle size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocating byte vector with previously determined size
    wasm_byte_vec_t wasm_data;
    wasm_byte_vec_new_uninitialized(&wasm_data, file_size);

    // Reads file content into wasm_data.data and closes file
    size_t read = fread(wasm_data.data, 1, file_size, file);
    fclose(file);
    if(read != file_size) {
        printf("Failed to read full wasm file\n");
        wasm_byte_vec_delete(&wasm_data);
        free(partition);
        return WASM_API_ERR;
    }

    /* Compile Wasm Module */ 

    // Pass bytes to Wasmtime for compilation
    wasmtime_error_t* error = wasmtime_module_new(engine, (const uint8_t*) wasm_data.data, wasm_data.size, &partition->module);
    wasm_byte_vec_delete(&wasm_data);

    if(error != NULL) {
        free(partition);
        return catch_err(ERR, "Failed to compile wasm module", error, NULL);
    }

    /* Instantiate module */
    
    // Instantiate
    wasm_trap_t* trap = NULL;
    error = wasmtime_instance_new(partition->context, partition->module, NULL, 0, &partition->instance, &trap);

    // Error == unsuccessful resource alloc, trap == runtime exception
    if(error != NULL || trap != NULL) {
        if(error != NULL) {
            catch_err(ERR, "Error while instantiating wasm module", error, NULL);
        }else if(trap != NULL) {
            catch_err(TRAP, "Trap during instantiation", NULL, trap);
        }

        wasmtime_module_delete(partition->module);
        wasmtime_store_delete(partition->store);
        free(partition);
        return WASM_API_ERR;// TODO: When to exit with return
    }

    partition->instantiated = true;
    partitions[partition_id] = partition;

    return WASM_API_OK;
}


/**
 * @brief Inject fuel to the partition
 *
 * @param partition_id Partition identifier
 * @param fuel_amount Amount of fuel for the partition
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_inject_fuel(int partition_id, uint64_t fuel_amount) {

    printf("Injecting %lu units of fuel...\n", fuel_amount);

    wasmtime_error_t* error = wasmtime_context_set_fuel(partitions[partition_id]->context, fuel_amount);

    // Async Yield
    // wasmtime_context_fuel_async_yield_interval(partitions[partition_id]->context, fuel_amount);

    catch_err(ERR, "Error injecting fuel", error, NULL);

    return WASM_API_OK;    
}

 
/**
 * @brief Run partition for a fuel slice
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_run_partition(int partition_id, const char* func_name) {

    if(!partitions[partition_id]->instantiated) {
        printf("Module not instantiated\n");
        return WASM_API_ERR;
    }

    /* Look up exported function */

    // Looks up export by func_name in the current instance, result stored in ext
    wasmtime_extern_t ext;
    bool ok = wasmtime_instance_export_get(partitions[partition_id]->context, &partitions[partition_id]->instance, func_name, strlen(func_name), &ext);

    // Export exist and it's a function
    if(!ok || ext.kind != WASMTIME_EXTERN_FUNC) {
        printf("Function '%s' not found or not a function\n", func_name);
        return WASM_API_ERR;
    }

    /* Call function */

    wasm_trap_t* trap = NULL;

    int32_t fib = 10;
    wasmtime_val_t params[1];
    params[0].kind = WASMTIME_I32;
    params[0].of.i32 = fib;

    wasmtime_val_t results[1]; // TODO: Expect only one result

    // Actual call
    wasmtime_error_t* error = wasmtime_func_call(
        partitions[partition_id]->context, &ext.of.func,
        params, 1,      // Parameters
        results, 1,     // Result
        &trap
    );

    if(error != NULL || trap != NULL) {
        if(error != NULL) {
            catch_err(ERR, "Error calling function", error, NULL);
        }else if(trap != NULL) {
            catch_err(TRAP, "Trap while calling function", NULL, trap);
        }

        return WASM_API_ERR;
    }

    // printFuelUsage(partition_id);
    // wasm_api_fuel_remaining(partition_id);

    // Return result
    if(results[0].kind == WASMTIME_I32) {
        printf("Fibonacci(%d) =  %d\n", fib, results[0].of.i32);
    }

    return WASM_API_OK;   
}


/** // TODO: Probably obsolete
 * @brief Check if fuel is exhausted
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_fuel_remaining(int partition_id) {

    uint64_t fuel_remaining = 0;

    wasmtime_error_t* error = wasmtime_context_get_fuel(partitions[partition_id]->context, &fuel_remaining);

    if(error != NULL) {
        return catch_err(ERR, "Error querying fuel remaining", error, NULL);
    }

    printf("Partition %d: ", partition_id);
    printf("Fuel remaining %ld\n", fuel_remaining);

    return WASM_API_OK;    
}


/**
 * @brief Cleanup resources
 *
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
void wasm_api_cleanup(void) {
    for (int i = 0; i < NUM_MAX_PARTITIONS; i++) {
        if (partitions[i]) {
            if (partitions[i]->instantiated) {
                wasmtime_module_delete(partitions[i]->module);
                wasmtime_store_delete(partitions[i]->store);
            }
            free(partitions[i]);
            partitions[i] = NULL;
        }
    }

    if (engine) {
        wasm_engine_delete(engine);
        engine = NULL;
    }

    printf("\nWasm API cleaned up!\n");
}


/****************************************************************************
 * Main for Testing
****************************************************************************/

static void printInfo(int partition_id, int run) {
    uint64_t fuel_remaining = 0;
    wasmtime_context_get_fuel(partitions[partition_id]->context, &fuel_remaining);
    printf("<<<<<<<<<<<<<<<<<<<< Partition %u >>>>>>>>>>>>>>>>>>>>\n", 0);
    printf("Fuel remaining: %lu\n", fuel_remaining);
    printf("Fuel used (total): %lu\n", (FUEL_AMOUNT - fuel_remaining));
    printf("Fuel used (this run): %lu\n", (FUEL_AMOUNT - fuel_remaining)/(run + 1));
}

int main(int argc, char** argv) {

    int benchmark_mode = (argc > 1 && strcmp(argv[1], "--benchmark") == 0);

    if(wasm_api_init() != WASM_API_OK) return 1;

    if(wasm_api_load_partition(0, "fib.wasm") != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_inject_fuel(0, FUEL_AMOUNT) != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_load_partition(1, "fib.wasm") != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_inject_fuel(1, FUEL_AMOUNT) != WASM_API_OK) return WASM_API_ERR;

    if(benchmark_mode) {
        printf("Running in benchmark mode\n");

        uint64_t total_time[2] = {0, 0};

        for(int run = 0; run < NUM_RUNS; run++) {
            printf("\n##################### Run %d: #####################\n", run + 1);
            uint64_t t0 = runPartitionBenchmark(0, "main");
            uint64_t t1 = runPartitionBenchmark(1, "main");

            printInfo(0, run);            
            printInfo(1, run);

            total_time[0] += t0;
            total_time[1] += t1;

            // if(run == 1998 && wasm_api_inject_fuel(0, FUEL_AMOUNT) != WASM_API_OK) return WASM_API_ERR;
        }

        printf("\nAverage time Partition 0: %lu µs\n", total_time[0] / NUM_RUNS);
        printf("Average time Partition 1: %lu µs\n", total_time[1] / NUM_RUNS);

    }else {
        if(runPartitionBenchmark(0, "main") == (uint64_t) WASM_API_ERR) return 1;
        if(runPartitionBenchmark(1, "main") == (uint64_t) WASM_API_ERR) return 1;
    }

    wasm_api_cleanup();

    return 0;
}