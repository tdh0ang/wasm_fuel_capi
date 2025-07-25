/*
 * wasm_api.h
 *
 *  Created on: Jul 17, 2025
 *      Author: tdhoang
 */

#ifndef WASM_API_H
#define WASM_API_H

/****************************************************************************
 * Includes
****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <wasm.h>
#include <wasmtime.h>


/****************************************************************************
 * Defines
****************************************************************************/
#define NUM_MAX_PARTITIONS 2
#define FUEL_AMOUNT     10000000
#define YIELD_AFTER     100
#define NUM_RUNS        100

/****************************************************************************
 * Error Codes
****************************************************************************/
#define WASM_API_NO_FUEL           -2 
#define WASM_API_ERR               -1
#define WASM_API_OK                 0 


/****************************************************************************
 * Structs
****************************************************************************/

typedef struct wasm_partition {
    wasmtime_module_t *module;
    wasmtime_instance_t instance;
    wasmtime_context_t *context;
    wasmtime_store_t *store;
    wasmtime_linker_t *linker;
    wasmtime_call_future_t *future;
    int partition_id;
    bool instantiated;
    wasmtime_val_t *results;
    wasmtime_func_t exported_func;
}wasm_partition;


typedef enum {
    PARTITION_DONE,
    PARTITION_YIELDED,
    PARTITION_ERROR
} partition_status;

/****************************************************************************
 * Function Prototypes
****************************************************************************/

/**
 * @brief Initialize the Wasm engine and context globally 
 *
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_init(void);


/**
 * @brief Load Wasm module from file and instantiate it
 *
 * @param wasm_file Wasm module to be read
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_load_partition(int partition_id, const char* wasm_file);


/**
 * @brief Inject fuel to the partition
 *
 * @param partition_id Partition identifier
 * @param fuel_amount Amount of fuel for the partition
 * @param yield True when yielding should be activated
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_inject_fuel(int partition_id, uint64_t fuel_amount, bool yield);

 
/**
 * @brief Run partition for a fuel slice
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_run_partition(int partition_id, const char* func_name);


/**
 * @brief Check if fuel remaining
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_fuel_remaining(int partition_id);


/**
 * @brief Cleanup resources
 *
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
void wasm_api_cleanup(void);

/**
 * @brief Returns partition array
 *
 * @param partition_id
 * @return Returns partition array
 */
wasm_partition *get_wasm_partition(int partition_id);


#endif // WASM_API_H