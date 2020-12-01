# [Flax](https://flax-lang.github.io)

A low level, general-purpose language with high level syntax and expressibility.


[![forthebadge](https://forthebadge.com/images/badges/made-with-crayons.svg)](http://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/built-with-resentment.svg)](http://forthebadge.com)

[![build status](https://ci.appveyor.com/api/projects/status/c9cmm08t27ef1hji/branch/master?svg=true)](https://ci.appveyor.com/project/zhiayang/flax/branch/master)
&nbsp;&nbsp;&nbsp;&nbsp;
[![build status](https://github.com/flax-lang/flax/workflows/CI/badge.svg)](https://github.com/flax-lang/flax/actions)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Build Status](https://semaphoreci.com/api/v1/zhiayang/flax/branches/master/badge.svg)](https://semaphoreci.com/zhiayang/flax)


-----------------------------------------------

#### Disclaimer ####

I work on Flax in my spare time, and as the lone developer I cannot guarantee continuous development.
I'm no famous artist but this is my magnum opus, so it'll not be abandoned anytime soon.

### Language Goals

- No header files.
- Minimal runtime
- Minimal stupidity
- Clean, expressive syntax


-----------------------------------------------


### Current Features

- Structs, unions, enums
- Arrays (fixed and dynamic), slices
- Pointer manipulation/arithmetic
- Operator overloading
- Generic functions and types
- Type inference (including for generics)
- Full compile-time execution (of arbitrary code)
- Classes, including virtual dispatch and (single) inheritance

-----------------------------------------------


### Language Syntax

- We don't have a proper place that documents everything yet, but most of the basic stuff is probably not gonna change much.
  The testing code in `build/tests/` (most of them, anyway — check `tester.flx` to see which ones we call) tests basically 90% of the
  language, so that's the syntax reference for now.
- Yes, the syntax is not "officially" defined by a grammar. The reference parser implementation is the One True Definition, for now.

-----------------------------------------------



### Code Sample

```rust
import std::io as _

@entry fn main()
{
	println("hello, world!")
}

```

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
- LLVM 11, mostly due to their obsession with changing the IR interface every damn version
- GMP/MPIR
- MPFR
- libffi


#### macOS / Linux

- The `makefile` is the preferred way to build on UNIX systems.
- LLVM needs to be installed. On macOS, `brew install llvm` should work, and you might need to do some PPA fiddling for Debian-based distros.
- A C++17-compatible compiler should be used.
- Find the `flaxc` executable in `build/sysroot/usr/local/bin`.
- Additionally, the (admittedly limited) standard library will be copied from `./libs` to `./build/sysroot/usr/local/lib/flaxlibs/`.


#### Windows

- Install [meson](https://mesonbuild.com/).
- Run `env MPIR_ROOT_DIR=... LLVM_ROOT_DIR=... meson meson-build-dir` to set the locations for the dependencies (see `meson.build` for the names, there are 4) and configure the build.
- Optionally, pass `--buildtype=release` to build a release-mode compiler (highly recommended).
- Run `ninja -C meson-build-dir`.
- `flaxc.exe` will be found in the build directory.

##### Libraries
- Download the [prebuilt binaries](https://github.com/flax-lang/flax/releases/tag/win-build-deps) for the 4 dependencies, and place them somewhere.
- Note: the folder structure of the libraries should be `(llvm|mpir|mpfr|libffi)/(Release|Debug)/(include|lib)/...`.


-----------------------------------------------


### Building Flax Programs

Since nobody in their right mind is *actually* using this, please pass `-sysroot build/sysroot` to invocations of the compiler -- else the compiler will default to looking somewhere in `/usr/local/lib` for libraries. Speaking of which, standard libraries are looked for in `<sysroot>/<prefix>/lib/flaxlibs/`. Prefix is set to `/usr/local/` by default.

Since version 0.41.2, executables can be generated on all 3 of our supported platforms! For Linux and macOS, all that is required is a working C compiler in the `$PATH`; we call `cc` to link object files.

For Windows, even if you are not building the compiler from source (eg. you are using a released binary), Visual Studio 2017 or newer must still be installed, with the "Desktop development with C++", "MSVC v142 (or whatever)", and "Windows 10 SDK" components. However, we currently find the toolchain through [established means](https://gist.github.com/machinamentum/a2b587a68a49094257da0c39a6c4405f), so that `link.exe` does not have to be in the `%PATH%`, ie. you do not have to call `vcvarsall.bat` before running the compiler.

-----------------------------------------------

### Contributors

Special thanks to the Unofficial Official Language Consultants:
[darkf](https://github.com/darkf), [adrian17](https://github.com/adrian17),
[sim642](https://github.com/sim642), and [ryu0](https://github.com/ryu0).




### Contributing

Feel free to open an issue if you feel that there's any missing feature, or if you found a bug in the compiler. Pull requests are also
welcome.



### Licensing

The Flax compiler itself (this repository) is licensed under the Apache 2.0 license (see `LICENSE` file). For ease of building, some dependencies
are included in the repository itself (under the `external` folder) and compiled together, instead of as a separate library (shared or otherwise).
These are:

1. [mpreal](https://bitbucket.org/advanpix/mpreal), GPL
2. [utf8rewind](https://bitbucket.org/knight666/utf8rewind), MIT
3. [tinyprocesslib](https://gitlab.com/eidheim/tiny-process-library), MIT












