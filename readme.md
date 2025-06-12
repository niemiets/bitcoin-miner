# Bitcoin Miner

Semi-working bitcoin miner made in C++ written in classic C style (kinda) as it allows more straight forward approach. Project was made for fun and learning purpose. I will rewrite at later time.

## Requirements

- Windows
- C++23 compiler
- CMake 3.30+
- Internet connection

## How to use?

### Build

```bash
mkdir build
cd build
cmake ..
make
```

### Launch

> Sometimes have to launch it more than once cause probably memory not allocated or something like that. After running for some time it crashes.

```bash
./miner.exe
```

## Quirks

Address ip to which miner connects is hard-coded (188.214.129.52), same with my wallet.
