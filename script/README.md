# LLVM Pass Pipeline Automation Script

## Overview
This script automates a binary translation workflow:
1. Binary preprocessing using **EFACT** and **CDMF**
2. Disassembly and LLVM Bitcode generation via **Mcsema**
3. Sequential application of custom LLVM Pass optimizations
4. Final cross-compilation to generate ARM64 architecture executable

---

## Required Variables (for example):
  ```bash
  export CPASS_DIR=/home/EC-SBT/Passes
  export EFACT_DIR=/home/EFACT
  export ARM_TOOLCHAIN_ROOT=/path/to/arm-toolchain (cross compile)
  export LLVM11_CLANG=/usr/bin/clang-11
  export LLVM20_CLANG=/usr/bin/clang-20
  export IDA_PATH=/home/ida/ida-8.3
  export LD_SCRIPT_OVERRIDE=/home/EC-SBT/Passes/ld_script_aarch64_back.ld
  export VA_LIST_HELPER_OVERRIDE=/home/EC-SBT/Passes/va_listHelperFunc_c.so
  ```
## Setup
  ```bash
  chmod +x EC-SBT-Runner-base
  ```
