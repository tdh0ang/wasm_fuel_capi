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


/****************************************************************************
 * Defines
****************************************************************************/
#define ERR            -1
#define TRAP            0

/****************************************************************************
 * Wasm/Wasmtime related instances
****************************************************************************/
wasm_engine_t *engine;
wasm_config_t *config;

/****************************************************************************
 * Wasm Partition Array
****************************************************************************/
wasm_partition *partitions[NUM_MAX_PARTITIONS] = {NULL};

/****************************************************************************
 * Static Function Prototypes
****************************************************************************/
static int catch_err(int errType, const char* msgPrint, wasmtime_error_t* err, wasm_trap_t* trap);
static int printFuelUsage(int partition_id);
static int partitionIdValid(int partition_id);


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
static int partitionIdValid(int partition_id) {
    if(partition_id < 0 || partition_id > NUM_MAX_PARTITIONS){
        printf("Invalid partition Id %d\n", partition_id);
        return WASM_API_ERR;
    }else {
        return WASM_API_OK;
    }
}


/****************************************************************************
 * Function Implementations
****************************************************************************/

wasm_partition *get_wasm_partition(int partition_id) {
    partitionIdValid(partition_id);
    return partitions[partition_id];
}


/**
 * @brief Initialize the Wasm engine and context globally 
 *
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_init(void) {
    
    // Config to enable fuel usage
    config = wasm_config_new();

    // Enable fuel consumption
    wasmtime_config_consume_fuel_set(config, true);

    // Async Support
    wasmtime_config_async_support_set(config, true);

    // Engine creation
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

    partition->linker = wasmtime_linker_new(engine);

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

    // Create Async instance, pre instance required for wasmtime_instance_pre_instantiate_async
    wasmtime_instance_pre_t *instance_pre = NULL;
    error = wasmtime_linker_instantiate_pre(partition->linker, partition->module, &instance_pre);
    if(error != NULL) {
        return catch_err(ERR, "Error preaparing async instantiation", error, NULL);
    }

    /* Instantiate module */
    
    // Instantiate
    wasm_trap_t* trap = NULL;
    wasmtime_call_future_t *future = wasmtime_instance_pre_instantiate_async(instance_pre, partition->context, &partition->instance, &trap, &error);
    if (error || !future) {
        return catch_err(ERR, "Error during async instantiation\n", error, NULL);
    }


    while(!wasmtime_call_future_poll(future)) {
        printf("instantiation yielded...\n");
    }

    wasmtime_call_future_delete(future);

    // Finalise partition attributes
    partition->future = NULL;
    partition->instantiated = true;
    partitions[partition_id] = partition;
    

    return WASM_API_OK;
}


/**
 * @brief Inject fuel to the partition
 *
 * @param partition_id Partition identifier
 * @param fuel_amount Amount of fuel for the partition
 * @param yield True when yielding should be activated
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_inject_fuel(int partition_id, uint64_t fuel_amount, bool yield) {

    wasm_partition *partition = partitions[partition_id];

    printf("Injecting %lu units of fuel...\n", fuel_amount);

    wasmtime_error_t* error = wasmtime_context_set_fuel(partition->context, fuel_amount);

    catch_err(ERR, "Error injecting fuel", error, NULL);

    // Async Yield
    if(yield) {
        printf("Yielding set for partition %d\n", partition_id);
        wasmtime_context_fuel_async_yield_interval(partition->context, YIELD_AFTER);
    } else {
        printf("No yielding set for partition %d\n", partition_id);
        wasmtime_context_fuel_async_yield_interval(partition->context, 0);
    }

    return WASM_API_OK;    
}

 
/**
 * @brief Run partition for a fuel slice
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_run_partition(int partition_id, const char* func_name) {

    wasm_partition *partition = partitions[partition_id];
    
    int32_t fib = 10;
    wasmtime_val_t params[1];
    // wasmtime_val_t results[1]; // TODO: Expect only one result
    if (partition->results == NULL) {
        partition->results = malloc(sizeof(wasmtime_val_t));  // Or handle allocation elsewhere
        if (!partition->results) {
            fprintf(stderr, "Failed to allocate results buffer\n");
            return WASM_API_ERR;
        }
    }

    if(!partition->instantiated) {
        printf("Module not instantiated\n");
        return WASM_API_ERR;
    }

    // First time call, skips function export in runs after that
    if(partition->future == NULL) {    
        /* Look up exported function */

        // Looks up export by func_name in the current instance, result stored in ext
        wasmtime_extern_t ext;
        bool ok = wasmtime_instance_export_get(partition->context, &partition->instance, func_name, strlen(func_name), &ext);

        // Export exist and it's a function
        if(!ok || ext.kind != WASMTIME_EXTERN_FUNC) {
            printf("Function '%s' not found or not a function\n", func_name);
            return WASM_API_ERR;
        }

        partition->exported_func = ext.of.func;

        /* Call function */

        wasm_trap_t* trap = NULL;
        wasmtime_error_t *error = NULL;

        params[0].kind = WASMTIME_I32;
        params[0].of.i32 = fib;

        partition->future = wasmtime_func_call_async(
            partition->context, &partition->exported_func,
            params, 1,
            partition->results, 1,
            &trap, &error
        );

        if(error != NULL || trap != NULL) {
            if(error != NULL) {
                catch_err(ERR, "Error calling function", error, NULL);
            }else if(trap != NULL) {
                catch_err(TRAP, "Trap while calling function", NULL, trap);
            }

            return WASM_API_ERR;
        }

    }
    // wasmtime_call_future_poll returns false when yielded
    if(wasmtime_call_future_poll(partition->future)) {
        if(partition->results[0].kind == WASMTIME_I32) {
            printf("Fibonacci(%d) =  %d\n", 10, partition->results[0].of.i32);
        }
        printf("Partition %d completed\n", partition_id);
        wasmtime_call_future_delete(partition->future);
        partition->future = NULL;
        return PARTITION_DONE;
    }else {
        printf("Partition %d yielded\n", partition_id);
        return PARTITION_YIELDED;
    }

    // while(!wasmtime_call_future_poll(partition->future)) {
    //     printf("Wasm yielded (Partition %d)\n", partition_id);
    // }

    // printFuelUsage(partition_id);

    // // Return result
    // if(results[0].kind == WASMTIME_I32) {
    //     printf("Fibonacci(%d) =  %d\n", fib, results[0].of.i32);
    // }

    // wasmtime_call_future_delete(partition->future);

    // return WASM_API_OK;   

    
}


/** // TODO: Probably obsolete
 * @brief Check if fuel remaining
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_fuel_remaining(int partition_id) {

    wasm_partition *partition = partitions[partition_id];

    uint64_t fuel_remaining = 0;

    wasmtime_error_t* error = wasmtime_context_get_fuel(partition->context, &fuel_remaining);

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
