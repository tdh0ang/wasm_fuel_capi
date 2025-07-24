# WebAssembly (Wasm) fuel example with the Wasmtime C API

## Dependencies
- wasmtime-v34.0.1-x86_64-linux-c-api found in https://github.com/bytecodealliance/wasmtime/releases

## Sources
- https://docs.wasmtime.dev/
- https://docs.wasmtime.dev/c-api/

## Motivation
There aren't a lot (not at all afaik) of implementations of fuel metering with Wasmtime in C. There are plenty of examples providede by ByteCodeAlliance here: https://github.com/bytecodealliance/wasmtime/tree/main/examples. The thing of interest lies in the usage of fuel. Ultimately leveraging the fuel concept of Wasm to implement a scheduler. 

A starting point is found under here: https://github.com/bytecodealliance/wasmtime/blob/main/examples/fuel.c. But this is not sufficient, as it is a synchronous implementation, while an asynchronous one is required. 

With this repo I try to provide an implementation of a very basic scheduler, scheduling pure Wasm modules with a C host, as there are no implementations of this thus far. The example utilises Wasmtime with asynchronous instantiation. 

## Background

### Quick Info
Fuel is a mechanism to interrupt a Wasm module, to not let it block the host indefinitely. Thisalready sounds like a baseline for scheduling, by leveraging the fuel concept, assigning fuel instead of time, to restrict module runtimes. Potential of fuel is discussed briefly in this paper, where potential can be seen: https://elib.dlr.de/201323/1/2023158068.pdf. 

**How to imagine fuel?**: Fuel is essentially instructions. 100 Fuel units equal roughly 100 instructions of Wasm. So instead of assigning time slices, instructions are assigned, what makes execution deterministic. 


### Goal 
There are two ways how fuel is handled, or more precisely, the interruption with it. In the example fuel.c mentioned above, when a Wasm module runs out of fuel, a trap is raised. This means though, that the module's execution is essentially canceled. It may run again with more/enough fuel to finish, but for scheduling this is not feasible. 

This is where async execution of Wasm modules is of interest. Async execution allows to pause the module execution and then resume. This is done with "wasmtime_context_fuel_async_yield_interval". For instance, when passing 1000, after 1000 fuel the Wasm module yields back to the C host, opening up the possibility of scheduling, instead of trapping and canceling the execution. For Rust this is rather easy, as only an async environment is needed (tokio) and of course the repsective async API calls. For C, since no examples are provided, this was a little more tricky. 

## How to use

### Repo structure

```
wasm_fuel_capi
├── capi                    # Wasmtime C API v34.0.1-x86_64
│   ├── include
│   ├── lib
│   ├── LICENSE
│   ├── min
│   └── README.md
├── main.c                  # Main calling API functions
├── Makefile
├── old                     # Deprecated files, old versions
│   ├── main.wat
│   ├── wasm_api_v1.c
│   └── wasm_api_v2.c
├── README.md
├── src
│   ├── wasm_api.c          # Main file implementing the logic
│   └── wasm_api.h
└── wasm
    ├── fib.wat             # Fibonacci 
    └── main.wat            # Loop incrementing a number
```

### Running
Simply run:

```bash
make
```

Clean up artifacts:

```bash
make clean
```

Run the executable:

```bash
./sched         # or ./sched --benchmark
```

Running with the --benchmark flag runs the modules several times, depending on NUM_RUNS macro in wasm_api.c

