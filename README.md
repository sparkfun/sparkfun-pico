# SparkFun Examples/Utilities for the Raspberry Pi rp2* Processors

The repository contains a set of utilities, structured for use with the Raspberry Pico SDK. This are target a new or unique functionality not fully supported by the standard ```pico-sdk``` yet, or that are unique to SparkFun Boards.

## The sparkfun_pico CMake library

A majority of the functionality outlined in this repository is contained in the folder ```sparkfun_pico```. This folder is structured as a ```cmake``` library, ready for use by the ```pico-sdk``` build environment.

### General Use

For a majority of the examples provided in this repository - especially those related to the use of PSRAM on the RP2350 - the following steps are used to build the examples:

* Clone this repository
* CD into the cloned repository
* Checkout the submodules in the repository (```git submodule update --init --recursive```)
* For an example that uses the ```sparkfun_pico``` library, copy ```sparkfun_pico``` folder in this repository to the example folder. (```cd examples/myexample; cp -R ../../sparkfun_pico .```)

At this point, the examples are built following the standard ```pico-sdk``` build process.

>>[!NOTE]
>> Specify the target SparkFun Board when calling cmake.

```sh
export PICO_SDK_PATH=<the path to the pico-sdk>
cd examples/has_psram
mkdir build
cmake .. -DPICO_BOARD=sparkfun_promicro_rp2350
make
```

### Examples

This repository contains the following examples:

#### [has_psram](examples/has_psram)

One of the more simple examples, this example shows how to detect if PSRAM is on the rp2350 board, and the size of the PSRAM available. It also walks through the available PSRAM, manually setting and verifying values.

#### [psram_allocator](examples/psram_allocator)

This example detects the PSRAM available on the board, and adds it to an allocator, which manages the *allocation* of the PSRAM. PSRAM is accessed (allocated) using a provided API, which mimics the standard malloc/free functionality.  

A "Two-Level Segregated Fit" (flsf) allocator is used from [here](https://github.com/espressif/tlsf).

#### [all_allocator](examples/all_allocator)

This example detects the PSRAM available on the board, and adds it as well as the built in SRAM based heap to an allocator to provide an unified access to available memory. The allocator manages the *allocation* of the PSRAM and heap SRAM via a single API. The example also ```wraps``` the built in ```malloc``` and ```free``` suite of commands to integrate with existing examples and uses.

A "Two-Level Segregated Fit" (flsf) allocator is used from [here](https://github.com/espressif/tlsf).

#### [set_qflash](examples/set_qflash)

This is a simple example that is used to verify the Quad SPI bit is set in the flash IC used on the attached board. It doesn't require the use of the ```sparkfun_pico``` library, but is helpful during board development and fits with the goals of this repository.
