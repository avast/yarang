# yarang - YARA New Generation

`yarang` is an experiment focused on new scanning engine for YARA, which is based on compiling YARA ruleset into native code and exposing C API in order to use it anywhere.
It uses HyperScan under the hood as a pattern matcher instead of custom Aho-Corasick implementation which YARA rolls.

## Installation

Requirements:

* Linux
* CMake 3.11.0+
* C++ compiler with C++20 support
* HyperScan 5.3.0+ (with `hyperscan_pic.patch` which can be found in the root of this repository)
* Yaramod 3.8.1+

Compilation:

1. Clone [HyperScan](https://github.com/intel/hyperscan) and apply `hyperscan_pic.patch` and build it into static libraries.
2. Clone [yaramod](https://github.com/avast/yaramod.git) and build it into static library.
3. Build `yarang`
    ```
    mkdir build && cd build
    cmake -DHYPERSCAN_ROOT_DIR=<hyperscan install dir> -DYARAMOD_ROOT_DIR=<yaramod install dir> -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . -- -j
    ```
4. Make sure that `yarangc` (compiler) and `yarang` (scanner) are available in your `PATH` environment variable.
5. Set `HYPERSCAN_ROOT_DIR` environment variable point to your HyperScan installation. This is required because the ruleset needs to be compiled with HyperScan runtime.
6. Run `scripts/yarangc.sh <YARA_RULES_FILE>`. Your ruleset will be compiled to shared library `<YARA_RULES_FILE>.bin`.
7. You can now run `yarang <YARA_RULES_FILE>.bin [-c <CUCKOO_FILE>] <FILE>`.

## How it works

`yarangc` uses yaramod to parse out rules into ASTs. After that, the process is divided into these 2 stages:

1. Pattern extraction
2. Code generation
3. Ruleset compilation

### Pattern extraction

During the pattern extraction, strings are extracted out of rules and are divided into 2 categories, which are:

1. Literal patterns - pattern consisting only of characters which can be compared directly with string equality
2. Regex patterns - pattern which contains metacharacters and can't be matched directly through string comparison

All plain strings are implicitly literals, all regexes are implicitly regexes. Hex strings are analyzed and if they do not contain
any metacharacters then they are placed into literals, otherwise regexes. The product of this stage are 2 compiled HyperScan databases -
one for literals, one for regexes. The reason for this distinction is that literals do not need any complicated matching and can
use more optimized version of matching which works only on literals. Resulting database is also smaller for literals.

This stage also gives all strings their unique numeric identifier which will be later used during code generation and
also during the runtime.

### Code generation

AST of the whole ruleset is transformed into C code which will do the matching during runtime. As a common runtime, `scripts/ruleset.yar.cpp`
is used. It contains a lot of helper structures, functions and necessary runtime for rule evaluation. `yarangc` actually only generates
`ruleset.def` file which is included somewhere in the middle of `scripts/ruleset.yar.cpp`. `rules.def` file contains:

1. Functions implementing the logic of an individual rules
2. `rules` array containing all the rules
3. `ScanContext` structure which is basis for matching a individual buffer of data

There is a huge emphasis put on using expressions which can be evaluated during the compile time. We want to make runtime as fast as possible and therefore
we are willing to spend more time during code generation and actual ruleset compilation (next stage). That's why you can see a lot of templates,
tuples, arrays and other structures which compiler can evaluate during the compilation and not vectors, hash tables and other structures with dynamic allocation.

Instead of string identifiers, unique numeric indetifiers are used. Same for rules. This makes the runtime as compact as possible and allows compiler to pick up
several optimizations which will make it fast.

### Ruleset compilation

After the code generation is done, the ruleset is compiled into single shared library. Since the ruleset consists not only of the compiled code but
also HyperScan databases (literal, regex and mutex), they are bundled into the final shared library so that we don't have to distribute archive and multiple
files at the same time.

The ruleset itself is of `ET_DYN` type and therefore can be used in conjunction with [`dlopen`](https://man7.org/linux/man-pages/man3/dlopen.3.html).
It exports these symbols:

* `void yng_initialize()` - Needs to be called before any other `yarang` functions are called.
* `void yng_finalize()` - Needs to be called after all `yarang` calls had been made. All further calls result in undefined behavior.
* `Scanner* yng_new_scanner(void(*match_callback)(const char*, void*))` - Create a new scanner for this particular ruleset. This function returns pointer to unspecified type. You just need to keep it and pass it to functions where necessary. Provided callback is called each time a match is found. Callback arguments are rule name and user defind data of any type which come from `yng_scan_data`.
* `void yng_free_scanner(Scanner*)`- Used to release resources created by `yng_new_scanner`.
* `void yng_scan_data(Scanner*, char* data, size_t size, const char* cuckoo_file_path, void* user_data)` - Scan particular data using scanner created by `yng_new_scanner`.

You can use [`dlsym`](https://man7.org/linux/man-pages/man3/dlsym.3.html) to obtain pointer to these functions and call them as necessary.

You can also not deal with any of this and instead use `yarang` binary which accepts ruleset shared library as an argument and is able to do all of this for you and print you results. However, if you want to use this scan engine as `libyara` then you'll have to do all of this manually.

## Results

`yarang` is far from complete. As of now it only supports very limited set of YARA language constructs which mostly revolve around strings themselves. The only thing from modules that is supported is `cuckoo.sync.mutex` function.

These runs were ran against 10TB of data. These are the numbers of old YARA scanning engine:

```
User time (seconds): 240630.85
System time (seconds): 2190.54
Percent of CPU this job got: 19912%
Elapsed (wall clock) time (h:mm:ss or m:ss): 20:19.46
Average shared text size (kbytes): 0
Average unshared data size (kbytes): 0
Average stack size (kbytes): 0
Average total size (kbytes): 0
Maximum resident set size (kbytes): 72145628
Average resident set size (kbytes): 0
Major (requiring I/O) page faults: 0
Minor (reclaiming a frame) page faults: 68756823
Voluntary context switches: 7622864
Involuntary context switches: 30649465
Swaps: 0
File system inputs: 21153692136
File system outputs: 0
Socket messages sent: 0
Socket messages received: 0
Signals delivered: 0
Page size (bytes): 4096
Exit status: 0
```

These are the numbers with new YARA scanning engine:

```
User time (seconds): 120528.40
System time (seconds): 1936.75
Percent of CPU this job got: 15453%
Elapsed (wall clock) time (h:mm:ss or m:ss): 13:12.45
Average shared text size (kbytes): 0
Average unshared data size (kbytes): 0
Average stack size (kbytes): 0
Average total size (kbytes): 0
Maximum resident set size (kbytes): 72774452
Average resident set size (kbytes): 0
Major (requiring I/O) page faults: 0
Minor (reclaiming a frame) page faults: 2675622
Voluntary context switches: 11065504
Involuntary context switches: 13723064
Swaps: 0
File system inputs: 21153692136
File system outputs: 0
Socket messages sent: 0
Socket messages received: 0
Signals delivered: 0
Page size (bytes): 4096
Exit status: 0
```

So instead of scanning the data for 20 minutes and 19 seconds (speed aprox. 8.4GB/s) we finished the scan in 13 minutes and 12 seconds (speed aprox. 12.9GB/s).
