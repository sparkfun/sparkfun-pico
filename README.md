# SparkFun Examples/Utilities for the Raspberry Pi rp2* Processors

The repository contains a set of utilities, structured for use with the Raspberry Pico SDK. This are target a new or unique functionality not fully supported by the standard ```pico-sdk``` yet, or that are unique to SparkFun Boards.

## Using The sparkfun_pico CMake library

A majority of the functionality outlined in this repository is contained in the folder ```sparkfun_pico```. This folder is structured as a ```cmake``` library, ready for use by the ```pico-sdk``` build environment.

### Add sparkfun_pico Library to a pico-sdk Project

To use the library as part of the cmake build process of the ```pico-sdk```, perform the following:

* Clone this repository to some directory ($SRC_DIR)
* CD into the cloned repository
* Checkout the submodules in the repository

```sh
cd $SRC_DIR
git clone https://github.com/sparkfun/sparkfun-pico
cd sparkfun-pico
git submodule update --init --recursive
```

* Copy the ```sparkfun_pico``` folder in this repository to the project folder.

```sh
cd myproject
cp -R $SRC_DIR/sparkfun-pico/sparkfun_pico .
```

* Add the sparkfun_pico library to your projects ```CMakeLists.txt``` file using the following line:

```cmake
add_subdirectory(sparkfun_pico)
```

### Replacing the Integrated Memory Allocator Routines

One option the sparkfun_pico library provides is replacing the default allocator in the pico-sdk and it's associated members (```malloc, free, ...```) with a version that access both the build heap SRAM and PSRAM. This leverages the ***wrap*** functionality provided by the ```pico-sdk```.

To enable this functionality, the following lines are added to your projects CMakeLists.txt file.

```cmake
# use our own allocator

# the following enables the system malloc/free to be wrapped
set(SKIP_PICO_MALLOC 1)

# the following enables wrapping in sparkfun_pico builds
set(SFE_PICO_ALLOC_WRAP 1)

# the following enables the system malloc/free to be wrapped during compilation
add_definitions(-DSFE_PICO_ALLOC_WRAP)
```

> [!NOTE]
> These lines must be added before the include of the pico-sdk file ```pico_sdk_import.cmake```

Once setup in the CMakeFiles.txt file, the resulting firmware will use all available PSRAM and SRAM via the standard allocator API (malloc, free, calloc, realloc ).

The example [all_allocator](examples/all_allocator) shows how to use this functionality.

### Board File

The sparkfun_pico library also includes a board file that includes board specific defines that are not part of the standard pico-sdk board files.

Currently this is just the PIN used as CS for the PSRAM on sparkfun RP2350 boards.

Example:

```c
// For the pro micro rp2350
#define SFE_RP2350_XIP_CSI_PIN 19
```

The file settings key off the board #defined set during the build process.

Mostly the use of this board file is automatic, not requiring direct access, but when using the raw PSRAM detection functionality, the required value of the PSRAM CS pin is obtained by including the file ```sparkfun_pico/sfe_pico_boards.h```, and defining a supported board in your cmake command.

```sh
cmake .. -DPICO_BOARD=sparkfun_promicro_rp2350
```

### Examples - General Use

For a majority of the examples provided in this repository - especially those related to the use of PSRAM on the RP2350 - the following steps are used to build the examples:

* Clone and setup this repo as noted above
* Copy in the ```sparkfun_pico``` library as noted above

At this point, the examples are built following the standard ```pico-sdk``` build process.

>[!NOTE]
> Specify the target SparkFun Board when calling cmake.

```sh
export PICO_SDK_PATH=<the path to the pico-sdk>
cd examples/has_psram
mkdir build
cmake .. -DPICO_BOARD=sparkfun_promicro_rp2350
make
```

## The sparkfun_pico API

The sparkfun_pico library supports the following functions:

### PSRAM detection

> [!NOTE]
> SparkFun RP2350 boards the PSRAM IC detailed [here](https://cdn.sparkfun.com/assets/0/a/3/d/e/APS6404L_3SQR_Datasheet.pdf)

These functions are exported from the file [sfe_psram.h](sparkfun_pico/sfe_psram.h)

```C
size_t sfe_setup_psram(uint32_t psram_cs_pin);
```

This function is used to detect the presence of PSRAM on the board and return the size of the PSRAM available. If no PSRAM is detected, a value of ```0``` is returned.

|Parameter|Description|
|---|---|
|psram_cs_pin| The CS pin used by the PSRAM IC on the board|

Additionally, if PSRAM is detected, it is setup correctly for use by the RP2350.

## The Examples

This repository contains the following examples:

### [has_psram](examples/has_psram)

One of the more simple examples, this example shows how to detect if PSRAM is on the rp2350 board, and the size of the PSRAM available. It also walks through the available PSRAM, manually setting and verifying values.

### [psram_allocator](examples/psram_allocator)

This example detects the PSRAM available on the board, and adds it to an allocator, which manages the *allocation* of the PSRAM. PSRAM is accessed (allocated) using a provided API, which mimics the standard malloc/free functionality.  

A "Two-Level Segregated Fit" (flsf) allocator is used from [here](https://github.com/espressif/tlsf).

### [all_allocator](examples/all_allocator)

This example detects the PSRAM available on the board, and adds it as well as the built in SRAM based heap to an allocator to provide an unified access to available memory. The allocator manages the *allocation* of the PSRAM and heap SRAM via a single API. The example also ```wraps``` the built in ```malloc``` and ```free``` suite of commands to integrate with existing examples and uses.

A "Two-Level Segregated Fit" (flsf) allocator is used from [here](https://github.com/espressif/tlsf).

### [set_qflash](examples/set_qflash)

This is a simple example that is used to verify the Quad SPI bit is set in the flash IC used on the attached board. It doesn't require the use of the ```sparkfun_pico``` library, but is helpful during board development and fits with the goals of this repository.
