#!/bin/bash

# Check required environment variables
if [ -z "$LLVM11_CLANG" ] || [ -z "$LLVM20_CLANG" ]; then
    echo "Please set the following environment variables:"
    echo "  LLVM11_CLANG   Path to LLVM 11 clang (e.g., /usr/bin/clang-11)"
    echo "  LLVM20_CLANG   Path to LLVM 20 clang (e.g., /usr/bin/clang-20)"
    exit 1
fi

# Define source files
LLVM11_SOURCES=(
    EXTInline.cpp
    ExternalEntryPointCheck.cpp
    FlagRegisterOPT.cpp
    FunctionNameDirectChange.cpp
    changCallingConvension.cpp
    changeBugDateOffset.cpp
    checkLinkerType.cpp
    fsRegisterPass.cpp
    fstatPass.cpp
    printfPass.cpp
    rdtscPass.cpp
    statPass.cpp
    va_listPass.cpp
)

LLVM20_SOURCES=(TLS.cpp)

# Build LLVM 11 Passes
for src in "${LLVM11_SOURCES[@]}"; do
    base=$(basename "$src" .cpp)
    echo "Compiling $src (LLVM 11)..."
    $LLVM11_CLANG `llvm-config --cxxflags` -Wl,-znodelete -fno-rtti -fPIC -shared \
        "$src" -o "${base}_llvm11.so" `llvm-config --ldflags`
done

# Build LLVM 20 Passes
for src in "${LLVM20_SOURCES[@]}"; do
    base=$(basename "$src" .cpp)
    echo "Compiling $src (LLVM 20)..."
    $LLVM20_CLANG `llvm-config-20 --cxxflags` -Wl,-znodelete -fno-rtti -fPIC -shared \
        "$src" -o "${base}_llvm20.so" `llvm-config-20 --ldflags`
done

echo "Build completed! Output files:"
ls -1 *.so
