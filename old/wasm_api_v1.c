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
#include <wasm.h>
#include <wasmtime.h>


/****************************************************************************
 * Defines
****************************************************************************/
#define ERR    -1
#define TRAP    0


/****************************************************************************
 * Wasm/Wasmtime related instances
****************************************************************************/
wasm_engine_t *engine;
wasmtime_store_t *store;
wasmtime_context_t *context;
wasm_config_t *config;

static wasmtime_module_t* module = NULL;
static wasmtime_instance_t instance;
static bool instantiated = false;

/****************************************************************************
 * Static Function Prototypes
****************************************************************************/
static int catch_err(int errType, const char* msgPrint, wasmtime_error_t* err, wasm_trap_t* trap);

/****************************************************************************
 * Static Function Implementations
****************************************************************************/

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

    engine = wasm_engine_new_with_config(config);
    if(!engine) {
        printf("Failed to create Wasmtime engine\n");
        return WASM_API_ERR;
    }

    store = wasmtime_store_new(engine, NULL, NULL);
    if(!store) {
        printf("Failed to create Wasmtime store\n");
        return WASM_API_ERR;
    }
    
    context = wasmtime_store_context(store);

    return WASM_API_OK;    
}


/**
 * @brief Load Wasm module from file and instantiate it
 *
 * @param wasm_file Wasm module to be read
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_load_partition(const char* wasm_file, int partition_id) {

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
        return WASM_API_ERR;
    }

    /* Compile Wasm Module */ 

    // Pass bytes to Wasmtime for compilation
    wasmtime_error_t* error = wasmtime_module_new(engine, wasm_data.data, wasm_data.size, &module);
    wasm_byte_vec_delete(&wasm_data);

    if(error != NULL) {
        catch_err(ERR, "Failed to compile wasm module", error, NULL);
    }

    /* Instantiate module */
    
    // Instantiate
    wasm_trap_t* trap = NULL;
    error = wasmtime_instance_new(context, module, NULL, 0, &instance, &trap);

    // Error == unsuccessful resource alloc, trap == runtime exception
    if(error != NULL || trap != NULL) {
        if(error != NULL) {
            catch_err(ERR, "Error while instantiating wasm module", error, NULL);
        }else if(trap != NULL) {
            catch_err(TRAP, "Trap during instantiation", NULL, trap);
        }

        wasmtime_module_delete(module);

        return WASM_API_ERR;
    }

    instantiated = true;

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

    wasmtime_error_t* error = wasmtime_context_set_fuel(context, fuel_amount);

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

    if(!instantiated) {
        printf("Module not instantiated\n");
        return WASM_API_ERR;
    }

    /* Look up exported function */

    // Looks up export by func_name in the current instance, result stored in ext
    wasmtime_extern_t ext;
    bool ok = wasmtime_instance_export_get(context, &instance, func_name, strlen(func_name), &ext);

    // Export exist and it's a function
    if(!ok || ext.kind != WASMTIME_EXTERN_FUNC) {
        printf("Function '%s' not found or not a function\n", func_name);
        return WASM_API_ERR;
    }

    /* Call function */

    wasm_trap_t* trap = NULL;
    wasmtime_val_t results[1]; // TODO: Expect only one result

    // Actual call
    wasmtime_error_t* error = wasmtime_func_call(
        context, &ext.of.func,
        NULL, 0,     // No parameters
        results, 1,  // Result expected
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

    // Return result
    if(results[0].kind == WASMTIME_I32) {
        printf("Wasm returned: %d\n", results[0].of.i32);
    }

    return WASM_API_OK;   
}


/**
 * @brief Check if fuel is exhausted
 *
 * @param partition_id Partition identifier
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
int wasm_api_fuel_exhausted(int partition_id) {

    return WASM_API_FUEL_AVAILABLE;    
}


/**
 * @brief Cleanup resources
 *
 * @return WASM_API_OK, when successful, else WASM_API_ERR
 */
void wasm_api_cleanup(void) {
    // TODO: free() issue
    if(module) wasmtime_module_delete(module);
    if(store) wasmtime_store_delete(store);
    if(engine) wasm_engine_delete(engine);
    if(config) wasm_config_delete(config);
}



/****************************************************************************
 * Main for Testing
****************************************************************************/

int main() {
    if(wasm_api_init() != WASM_API_OK) return 1;

    if(wasm_api_load_partition("main.wasm", 0) != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_inject_fuel(0, 1000) != WASM_API_OK) return WASM_API_ERR;

    if(wasm_api_run_partition(0, "main") != WASM_API_OK) return WASM_API_ERR;
    
    // wasm_api_cleanup();

    return 0;
}