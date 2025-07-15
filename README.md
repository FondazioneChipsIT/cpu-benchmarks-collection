# cpu-benchmarks-collection

This repository allows one to compile state-of-the-art CPU benchmarks to be run
on target platforms that are different than the host computer where they are
built. These can be platforms that support bare-metal runtimes, RTOSs, or rich
OSs.

## Tree structure

```
.
├── benchmarks/
│   ├── <benchmark1>.mk
│   ├── <benchmark2>.mk
│   ├── deps/
│   |   └── <benchmark1>
|   |   └── <benchamrk2>
│   ├── patches/
├── build/
│   └── <target1> // e.g. `cheshire`
|   └── <target2>
├── Makefile
├── README.md
├── scripts
│   └── apply_patches.sh
└── target
	└── <target1> // e.g. `cheshire`
```

## Getting started

This repository uses Git submodules for the original benchmarks that have to be
initialized. Either clone the repository recursively using:

```
git clone --recursive <url>
```

or fetch the submodules afterwards in the repository:

```
git submodule update --init --recursive
```

### Building a Benchmark
To compile a benchmark for a given platform, first set the root of the platform
SDK or environment (e.g. containing headers, runtime, etc.) as
`PLATFORM_SDK_ROOT` and its toolchain as `PLATFORM_CROSS_COMPILE`:

Then build the target benchmark:

```
make all BENCHMARK=<benchmark> PLATFORM=<target>
```

An example for the target `cheshire`, which integrates the RISC-V core CVA6,
follows:

```
export PLATFORM_ROOT=<abs_path_platform>/cheshire/sw
export PLATFORM_CROSS_COMPILE=<abs_path_toolchain>/bin/riscv64-unknown-elf
make all BENCHMARK=coremark PLATFORM=cheshire
```

**Note:** if you are at IIS and are targeting `cheshire`, you can use:
```
export PLATFORM_CROSS_COMPILE=/usr/pack/riscv-1.0-kgf/riscv64-gcc-11.2.0/bin/riscv64-unknown-elf
```

### Benchmark Patches
Some benchmarks (e.g. `STREAM`) require patches to be portable. These are stored
under:

```
$ROOT/benchmarks/patches/<benchmark>
```

To apply patches, from the root of the repository, call:

```
./scripts/apply-patches.sh --benchmark <benchmark>
```

The script applies all `.patch` files to the selected benchmark.

**TODO @ale:** improve script to be launched automatically after repo init and to cover
all benchmarks if needed. Right now, only STREAM needs patches

### Target Configuration
Each target platform directory under `$ROOT/target/<platform>/` includes:

* `target.mk`: Defines toolchain, compiler flags, and required libraries

* `ports/<benchmark>/`: Platform-specific source files or headers for the benchmark

### Supported Benchmarks

TODO complete

* `coremark`
* `coremark-pro`: WIP
* `embench-iot`
* `riscv-tests`
* `stream`

## Adding a New Benchmark
* Add the benchmark sources as a submodule under
  `$ROOT/benchmarks/deps/<benchmark>/`

* If needed, add patches under `$ROOT/benchmarks/patches/<benchmark>/`

* Create a fragment `$ROOT/benchmarks/<benchmark>.mk` to collect
  benchmark-specific source files and headers.

* Add platform-specific support for the benchmark under
  `$ROOT/target/<platform>/<benchmark>/`

## License
Unless specified otherwise in the respective file headers, all code checked into
this repository is made available under a permissive license. All software
sources are licensed under Apache 2.0.
