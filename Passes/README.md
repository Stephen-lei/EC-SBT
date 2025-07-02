The LLVM passes in this dir is the LLVM passes that solve some translation accurate problem or imporve the performance of the translated program.

## 1. System Requirements
- LLVM 11 and LLVM 20 development libraries
- Corresponding Clang compilers (clang-11, clang-20)

## 2. Environment Variables
Set these variables before compilation:
```bash
export LLVM11_CLANG=/path/to/clang-11   # Example: /usr/bin/clang-11
export LLVM20_CLANG=/path/to/clang-20   # Example: /usr/bin/clang-20
```
## 2. build passed
```bash
chmod +x build_passes.sh
./build_passes.sh
