# [Flax](https://flax-lang.github.io)

A low level, general-purpose language with high level syntax and expressibility.


[![forthebadge](https://forthebadge.com/images/badges/made-with-crayons.svg)](http://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/built-with-resentment.svg)](http://forthebadge.com)

[![Build Status](https://semaphoreci.com/api/v1/zhiayang/flax/branches/develop/badge.svg)](https://semaphoreci.com/zhiayang/flax)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Build status](https://ci.appveyor.com/api/projects/status/c9cmm08t27ef1hji/branch/develop?svg=true)](https://ci.appveyor.com/project/zhiayang/flax/branch/develop)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Build Status](https://travis-ci.org/flax-lang/flax.svg?branch=develop)](https://travis-ci.org/flax-lang/flax)



-----------------------------------------------


<!-- <p align="center">
  <img src="https://raw.githubusercontent.com/flax-lang/flax/develop/build/d20.gif" />
</p>
----------------------------------------------- -->

#### Disclaimer ####

I work on Flax in my spare time, and as the lone developer I cannot guarantee continuous development.
I'm no famous artist but this is my magnum opus, so it'll not be abandoned anytime soon.

### Language Goals

- No header files.
- Minimal runtime
- Minimal stupidty
- Clean, expressive syntax


-----------------------------------------------


### Current Features

- Structs, classes, unions, enums
- Arrays (fixed and dynamic), slices
- Pointer manipulation/arithmetic
- Operator overloading
- Generic functions and types
- Type inference (including for generics)
- Full compile-time execution (of arbitrary code)

-----------------------------------------------


### Language Syntax
- See https://flax-lang.github.io (incomplete, outdated, obsolete, etc. etc.)

-----------------------------------------------



### Code Sample

```rust
do {
	fn prints<T, U>(m: T, a: [U: ...])
	{
		for x in a => printf(" %.2d", m * x)
	}

	printf("set 6:")
	let xs = [ 1, 2, 3, 4, 5 ]
	prints(3, ...xs)

	printf("\n")
}

do {
	union option<T>
	{
		some: T
		none
	}

	let x = option::some("foobar")
	let y = option::some(456)

	println("x = %, y = %", x as some, y as some)
}
```

-----------------------------------------------


### Building the Flax compiler

#### Dependencies ####
- LLVM 7, mostly due to their obsession with changing the IR interface every damn version
- GMP/MPIR
- MPFR
- libffi


#### macOS / Linux

- The `makefile` is the preferred way to build on UNIX systems.
- LLVM needs to be installed. On macOS, `brew install llvm@7` should work, and you might need to do some PPA fiddling for Debian-based distros.
- A *C++17*-compatible compiler should be used.
- Find the `flaxc` executable in `build/sysroot/usr/local/bin`
- Additionally, the (admittedly limited) standard library will be copied from `./libs` to `./build/sysroot/usr/local/lib/flaxlibs/`


#### Windows

- Install [meson](https://mesonbuild.com/).
- Run `env MPIR_ROOT_DIR=... LLVM_ROOT_DIR=... meson meson-build-dir` to set the locations for the dependencies (see `meson.build` for the names, there are 4) and configure the build.
- Optionally, pass `--buildtype=release` to build a release-mode compiler (highly recommended)
- Run `ninja -C meson-build-dir`.
- `flaxc.exe` will be found in the build directory.

##### Libraries
- Download the [prebuilt binaries](https://github.com/flax-lang/flax/releases/tag/win-build-deps) for the 4 dependencies, and place them somewhere.
- Note: the folder structure of the libraries should be `(llvm|mpir|mpfr|libffi)/(Release|Debug)/(include|lib)/...`


-----------------------------------------------


### Building Flax Programs

- Some form of compiler (`cc` is called via `execvp()`) should be in the `$PATH` to produce object/executable files; not necessary if using JIT
- Since nobody in their right mind is *actually* using this, please pass `-sysroot build/sysroot` to invocations of the compiler -- else the compiler will default to looking somewhere in `/usr/local/lib` for libraries.
- Speaking of which, standard libraries are looked for in `<sysroot>/<prefix>/lib/flaxlibs/`. Prefix is set to `/usr/local/` by default.


-----------------------------------------------

### Contributors

Special thanks to the Unofficial Official Language Consultants:
[darkf](https://github.com/darkf), [adrian17](https://github.com/adrian17),
[sim642](https://github.com/sim642), and [ryu0](https://github.com/ryu0).




### Contributing

Feel free to open an issue if you feel that there's any missing feature, or if you found a bug in the compiler. Pull requests are also
welcome.














