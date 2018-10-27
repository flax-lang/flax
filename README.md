# [Flax](https://flax-lang.github.io)

A low level language with high level syntax and expressibility, aimed at OSDev work.


[![forthebadge](https://forthebadge.com/images/badges/made-with-crayons.svg)](http://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/built-with-resentment.svg)](http://forthebadge.com)

[![Build Status](https://semaphoreci.com/api/v1/zhiayang/flax/branches/develop/badge.svg)](https://semaphoreci.com/zhiayang/flax)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Build status](https://ci.appveyor.com/api/projects/status/c9cmm08t27ef1hji/branch/develop?svg=true)](https://ci.appveyor.com/project/zhiayang/flax/branch/develop)
&nbsp;&nbsp;&nbsp;&nbsp;
[![Build Status](https://travis-ci.org/flax-lang/flax.svg?branch=develop)](https://travis-ci.org/flax-lang/flax)



-----------------------------------------------


<p align="center">
  <img src="https://raw.githubusercontent.com/flax-lang/flax/develop/build/d20.gif" />
</p>



-----------------------------------------------

#### Disclaimer ####

I work on Flax in my spare time, and as the lone developer I cannot guarantee continuous development.
I'm no famous artist but this is my magnum opus, so it'll not be abandoned anytime soon.

### Language Goals

- No header files.
- Minimal runtime
- Clean, expressive syntax
- Minimal stupidty
- No magic behind your back


-----------------------------------------------


### Current Features

- User-defined types (currently only product types)
- Arrays (fixed and dynamic), slices
- Pointer manipulation/arithmetic
- Operator overloading
- C++ auto-style type inference
- Generic functions and types
- C-style for loops
- Range-based for loops

-----------------------------------------------


### Language Syntax
- See https://flax-lang.github.io (incomplete)

-----------------------------------------------



### Code Sample

```rust
do {
	fn gincr<A>(x: A) -> A => x + 1
	fn apply<B, C>(x: B, f: fn(B) -> C) -> C => f(x)

	fn mapstupid<D, E, F>(arr: [D:], f: fn(D) -> E, fa: fn(D, fn(D) -> E) -> F) -> [F]
	{
		var i = 0
		var ret: [F]
		while i < arr.length
		{
			ret.append(fa(arr[i], f))
			i += 1
		}

		return ret
	}

	printf("set 4:")
	let new = mapstupid([ 5, 6, 7, 8, 9 ], gincr, apply)
	for it in new { printf(" %d", it) }

	printf("\n")
}
```

-----------------------------------------------


### Building the Flax compiler


#### macOS / Linux

- Flax uses a makefile; most likely some form of GNU-compatible `make` will work.
- LLVM needs to be installed. On macOS, `brew install llvm@6` should work. (note: llvm 7 and up seems to have changed some JIT-related things)
- For macOS people, simply call `make`.
- Linux people, call `make linux`.
- A *C++17*-compatible compiler should be used.
- Find the `flaxc` executable in `build/sysroot/usr/local/bin`
- Additionally, the (admittedly limited) standard library will be copied from `./libs` to `./build/sysroot/usr/local/lib/flaxlibs/`


#### Windows

##### Option 1
- Install [meson](https://mesonbuild.com/)
- Edit `meson.build` variables to tell it where to find the libraries -- notably, these are needed: [`libmpir`](http://mpir.org), [`libmpfr`](http://mpfr.org), and most importantly, [`libllvm`](http://llvm.org). Follow the build instructions for each library, preferably generating both Debug and Release *static* libraries.
- Run `meson build\meson-dbg` (where ever you want, really), followed by `ninja -C build\meson-dbg`.
- `flaxc.exe` will be in `build\meson-dbg`.
- Build and profit, hopefully.

##### Option 2
- Download the [prebuilt binaries](https://github.com/flax-lang/flax/releases/download/win-build-deps/libraries.zip) for LLVM, MPIR, and MPFR.
- Set the following environment variables: `DEPS_DBG_INCLUDES_DIR`, `DEPS_DBG_LIBS_DIR`, `DEPS_REL_INCLUDES_DIR`, and `DEPS_REL_LIBS_DIR`. Point them to the location of the libraries you just downloaded. (Look in `appveyor.yml` to see what they should contain -- essentially the include and library paths for `msbuild` to find)
- Note: the folder structure of the libraries should be `(llvm|mpir|mpfr)/Release/(include|lib)/...`
- Run `msbuild /p:Configuration=Release`

-----------------------------------------------


### Building Flax Programs

- Some form of compiler (`cc` is called via `execvp()`) should be in the `$PATH` to produce object/executable files; not necessary if using JIT
- Since nobody in their right mind is *actually* using this, please pass `-sysroot build/sysroot` to invocations of the compiler -- else the compiler will default to looking somewhere in `/usr/local/lib` for libraries.
- Speaking of which, standard libraries are looked for in `<sysroot>/<prefix>/lib/flaxlibs/`. Prefix is set to `/usr/local/` by default.


-----------------------------------------------


### Contributing

- Found a bug? Want a feature? Just submit a pull request!


-----------------------------------------------


### Compiler Architecture

Some stuff about the compiler itself, now. As any noob looking to build a compiler would do, I used the [LLVM Kaleidoscope tutorial](http://llvm.org/docs/tutorial/) as a starting point. Naturally, it being a tutorial did no favours for the code cleanliness and organisation of the Flax compiler.

Flax itself has 4 main passes -- Lexing, Parsing, Typechecking, and finally Codegen. Yes that's right, the shitty typecheck-and-codegen-at-the-same-time architecture of old has been completely replaced!


#### Tokenising and Lexing

This isn't terribly complicated. Each file is naturally only looked at once, and a list of tokens and raw lines are stored somewhere. There's always a nagging feeling that token location reporting is flawed, and it probably is.

EDIT: It is.


#### Parsing

Broadly speaking this is a recursive descent parser, handwritten. Not terribly efficient, but whatever. This step creates the AST nodes for the next step, although there are some hacks to enable custom operators, which should probably be reimplemented sometime soon.



#### Typechecking

At this stage, each AST node that the parser produced is traversed, at the file-level. AST nodes are transformed through a typechecking phase into SST nodes (the original meaning of this initialism has been lost). This typechecking consists of solidifying `pts::Type`s into `fir::Type`s; given that the former is simply a stripped-down version of the latter, this is natural.

SST nodes are just AST nodes with refinements; identifiers are resolved to their targets, and function calls also find their intended target here.

Before the rewrite, this used to happen in an intertwined fashion with code generation, which definitely wasn't pretty.



#### Code Generation

After each file is typechecked, the collector forcibly squishes them together into a single unit, combining the necessary definitions from other files that were imported -- code generation happens at the program level.

During code generation, we output 'Flax Intermediate Representation', or 'FIR'. It's basically a layer on top of LLVM that preserves much of its interface, with some light abstractions built on top. This is where AST nodes actually get transformed into instructions.

Part of the purpose for FIR was to decouple from LLVM, and partly to allow compile-time execution in the future, which would definitely be easier with our own IR (mainly to send and retrieve values across the compiler <> IR boundary).



#### Translation

After a `fir::Module` is produced by the code generator, we 'translate' this into LLVM code. The entire process can be seen in `source/backend/llvm/translator.cpp`, and clearly we basically cloned the LLVM interface here, which makes it easy to translate into LLVM.














