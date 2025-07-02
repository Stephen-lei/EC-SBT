The LLVM passes in this dir is the LLVM passes that solve some translation accurate problem or imporve the performance of the translated program.

To use these passed ,you should compile the passes into .so file:
```bash
$ clang `llvm-config --cxxflags` -Wl,-znodelete -fno-rtti -fPIC -shared rtdscPass.cpp -o rtdscPass.so `llvm-config --ldflags`
```

you can also run the compile.sh directly.
