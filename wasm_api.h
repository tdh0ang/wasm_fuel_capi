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


/****************************************************************************
 * Defines
****************************************************************************/
#define NUM_MAX_PARTITIONS 2


/****************************************************************************
 * Error Codes
****************************************************************************/
#define WASM_API_NO_FUEL           -2 
#define WASM_API_ERR               -1
#define WASM_API_OK                 0 


/****************************************************************************
 * Structs
****************************************************************************/
typedef struct wasm_partition wasm_partition;


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
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_inject_fuel(int partition_id, uint64_t fuel_amount);

 
/**
 * @brief Run partition for a fuel slice
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_run_partition(int partition_id, const char* func_name);


/**
 * @brief Check if fuel is exhausted
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

#endif // WASM_API_H