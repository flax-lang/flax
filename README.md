# [Flax](https://flax-lang.github.io)

A low level language with high level syntax and expressibility, aimed at OSDev work.


[![forthebadge](http://forthebadge.com/images/badges/powered-by-electricity.svg)](http://forthebadge.com)
[![forthebadge](http://forthebadge.com/images/badges/fuck-it-ship-it.svg)](http://forthebadge.com)

--------------------------

#### Disclaimer ####
I work on Flax in my spare time, and as the lone developer I cannot guarantee continuous development. I am still a student, so the limited time I have will be shared between this project and [mx](github.com/zhiayang/mx). Expect stretches of active development, followed by stretches of inactivity.


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
- Arrays (small regressions, mostly complete)
- Pointer manipulation (addrof/dereference, pointer arithmetic)
- Operator overloading on structs
- Function overloading (therefore name mangling)
- Basic type inference (C++ auto-esque, not Haskell-esque)
- Namespaces
- Nested types
- Type-static functions
- Deferred expressions (executes at end of current block scope)
- Swift-like extensions to existing types
- Generic Functions

-------------------------

#### Language Syntax ####
- See https://flax-lang.github.io#syntax (somewhat out of date)

----------------------

#### Future Plans ####

- Rule the world
- Generic Types
- Subtyping
- Internal compiler optimisations

------------------------------------

#### Building the Flax compiler ####

- You will need clang++ (in your $PATH), because only it is capable of compiling LLVM bitcode to object code.
- Flax uses Shake, available at http://shakebuild.com
- To summarise: install `ghc`, install `cabal`, `cabal update; cabal install shake`, `runhaskell shakefile.hs` to build Flax.
- If you don't want to recompile `shakefile.hs` every time, you can also run `ghc shakefile.hs` and call `./shakefile` instead.
- Find the 'flaxc' executable in 'build/sysroot/usr/local/bin'
- Additionally, default libraries will be copied from './libs' to './build/sysroot/usr/local/lib'

-----------------------------------------------

#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
- Alternatively, join the discussion at #flax-lang on Freenode IRC.
- Requested help: Error catching, please! Sometimes the Flax compiler will fail to catch invalid things, leading to fired assertions or even broken LLVM-IR being generated (on the LLVM level, which gives cryptic messages)
