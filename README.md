[Flax](https://flax-lang.github.io) [![Build Status](https://travis-ci.org/flax-lang/flax.svg?branch=master)](https://travis-ci.org/flax-lang/flax)
============

A low level language with high level syntax and expressibility, aimed at OSDev work.

#### Language Goals ####

- This project grew out of my frustration with C/C++. I use C++ mainly for low level development, and its syntax is cumbersome, laden in C-derived K&R cruft... and the include files. Don't get me started on include files.
- Flax targets the LLVM backend, simply because it is widely ported, has an incredible amount of optimisation passes, and can target freestanding platforms.
- This language aims to have as little dependencies against userspace libraries as possible.
- Therefore, only a small subset of libc functions will be required. While the language has not been finalised, I envision only basic functions (printf, malloc/free, memset/memcpy/memmove) being required.
- The builtin standard library (tentatively 'Foundation') will be provided as a source-level library (written in Flax!), making it much easier to build against a freestanding target.
- Why so much talk about freestanding targets? I eventually want to write userspace programs for my hobby OS (https://github.com/requimrar/mx) in Flax, and if possible parts of the kernel or drivers.


--------------------------

#### Current Features ####

- User-defined types (structs, enums and typealiases)
- Arrays (probably regressed and broken)
- Pointer syntax (C-like)
- Operator overloading on structs
- Function overloading (C++-ish)
- Type inference
- Nested types
- Static functions
- Deferred statements (executes at end of current block scope)
- Swift-like extensions to existing types

-------------------------

#### Language Syntax ####
- See https://flax-lang.github.io#syntax

----------------------

#### Future Plans ####

- Rule the world
- Generic Types
- Type inheritance of some kind

------------------------------------

#### Building the Flax compiler ####

- You will need clang++ (in your $PATH), because only it is capable of compiling LLVM bitcode to object code.
- Flax uses Shake, available at http://shakebuild.com
- To summarise: install `ghc`, install `cabal`, `cabal update; cabal install shake`, `runhaskell shakefile.hs` to build Flax.
- Find the 'flaxc' executable in 'build/sysroot/usr/local/bin'
- Additionally, default libraries will be copied from './libs' to './build/sysroot/usr/local/lib'

-----------------------------------------------

#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
- Requested help: Error catching, please! A lot of the time the Flax compiler will fail to catch invalid things, leading to fired assertions or even broken LLVM-IR being generated (on the LLVM level, which gives cryptic messages)
