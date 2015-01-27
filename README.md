flax [![Build Status](https://travis-ci.org/requimrar/flax-lang.svg?branch=master)](https://travis-ci.org/requimrar/flax-lang)
============

Compiler/parser for simple language. For kicks.


#### Language Goals ####

- This project grew out of my frustration with C/C++. I use C++ mainly for low level development, and its syntax is cumbersome, laden in C-derived K&R cruft... and the include files. Don't get me started on include files.
- Flax targets the LLVM backend, simply because it is widely ported, has an incredible amount of optimisation passes, and can target freestanding platforms.
- This language aims to have as little dependencies against userspace libraries as possible.
- Therefore, only a small subset of libc functions will be required. While the language has not been finalised, I envision only basic functions (printf, malloc/free, memset/memcpy/memmove) being required.
- The builtin standard library (tentatively 'Foundation') will be provided as a source-level library (written in Flax!), making it much easier to build against a freestanding target.
- Why so much talk about freestanding targets? I eventually want to write userspace programs for my hobby OS (https://github.com/requimrar/mx) in Flax, and if possible parts of the kernel or drivers.


==========================

#### Current Features ####

- Handwritten parser
- Generates LLVM bitcode.
- User-defined types (structs, mostly)
- Arrays (compile-time fixed size only, dynamic arrays yet to be)
- Pointer syntax (C-like)
- Operator overloading on structs
- Function overloading (C++-ish)

=========================

#### Language Syntax ####

- Actually mostly Swift-like
- 'func' to declare/define functions
- Two-pass parser, no need for forward declarations
- var id: type style variable declarations
- 'ffi' (foreign function interface) for calling into C-code (supports varargs a-la printf)
- Strictly defined types, (U?)Int(8|16|32|64) and Float(32|64)
- Arrays declared with Type[length] syntax
- Dynamic arrays not yet supported.
- Pointers!

======================

#### Future Plans ####

- Rule the world
- Type inference


====================================

#### Building the Flax compiler ####

- You will need clang++ (in your $PATH), because only it is capable of compiling LLVM bitcode to object code.
- You must modify the 'projDir' variable in the top-level build.capri script.
- This is due to a limitation of the Capri system, it will be fixed soon according to its developer.
- Screw makefiles. Flax is currently using a third-party build system.
- Binaries and sources are available here: http://91.206.143.88/capri/releases/
- Documentation for the Capri script is available here: http://forum.osdev.org/viewtopic.php?f=2&t=28568&start=0
- Ensure that the 'capri' executable is on your path (or just provide the absolute path to your shell)
- Run 'capri' in the top-level directory.
- Find the 'flaxc' executable in 'build/sysroot/usr/bin'
- Additionally, default libraries will be built from 'build/sysroot/lib' and the shared library files (.dylib or .so) placed in 'build/sysroot/usr/lib'






#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
- Requested help: Error catching, please! A lot of the time the Flax compiler will fail to catch invalid things, leading to failed assertions or even broken LLVM-IR being generated (on the LLVM level, which gives cryptic messages)
