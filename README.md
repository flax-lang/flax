# [Flax](https://flax-lang.github.io)

A low level language with high level syntax and expressibility, aimed at OSDev work.


[![forthebadge](http://forthebadge.com/images/badges/powered-by-electricity.svg)](http://forthebadge.com)
[![forthebadge](http://forthebadge.com/images/badges/fuck-it-ship-it.svg)](http://forthebadge.com)

[![Build Status](https://semaphoreci.com/api/v1/zhiayang/flax/branches/develop/shields_badge.svg)](https://semaphoreci.com/zhiayang/flax)
&nbsp;&nbsp;&nbsp;&nbsp;[![Build Status](https://travis-ci.org/flax-lang/flax.svg?branch=develop)](https://travis-ci.org/flax-lang/flax)



-----------------------------------------------


<p align="center">
  <img src="https://raw.githubusercontent.com/flax-lang/flax/develop/build/d20.gif" />
</p>



-----------------------------------------------

#### Disclaimer ####

Feb 2017: I'm being dragged off to dig trenches or something, Flax will thus experience slow development for... basically 2 years.



I work on Flax in my spare time, and as the lone developer I cannot guarantee continuous development. I am still a student, so the limited time I have will be shared between this project and [mx](https://github.com/zhiayang/mx). Expect stretches of active development, followed by stretches of inactivity.


#### Language Goals ####

- No header files.
- Minimal runtime (at most dependent on libc)
- Clean, expressive syntax
- Minimal stupidty
- No magic behind your back


-----------------------------------------------


#### Current Features ####

- User-defined types (structs, enums and typealiases)
- Arrays (fixed and dynamic), slices
- Pointer manipulation/arithmetic
- Operator overloading
- Basic type inference (C++ auto-esque, not Haskell-esque)
- Deferred expressions (executes at end of current block scope)
- Swift-like extensions to existing types
- Generic Functions
- C-style for loops
- Range-based for loops
- Basically almost everything that C can do (plus some more), with the exception of `typedef`.

-----------------------------------------------


#### Language Syntax ####
- See https://flax-lang.github.io (incomplete)

-----------------------------------------------



#### Code Sample ####

```swift
ffi fn malloc(size: i64) -> i8*
ffi fn printf(fmt: i8*, ...) -> i32

namespace NS
{
	class Array<T>
	{
		var data: T*

		init(ptr: T*)
		{
			self.data = ptr
		}

		operator [] (index: i64) -> T
		{
			get { self.data[index] }
			set { self.data[index] = value }
		}
	}
}

fn add<T>(a: T, b: T) -> T
{
	a + b
}

public fn main() -> int
{
	do {
		let ptr = (alloc[60] i64).data
		defer dealloc ptr

		var list = NS.Array<T: i64>(ptr)

		ptr[0] = 37

		printf("x = %d\n", list[0])
		list[1] = 401
	}

	do {
		let ptr = (alloc[60] string).data
		defer dealloc ptr


		var list = NS.Array<T: string>(ptr)
		list[3] = "foobar"

		printf("0 = '%s', 3 = '%s'\n", list[0], list[3])
	}

	let c = add<T: i64>(30, 40)
	let d = add(100, 200)
	printf("c = %d, d = %d\n", c, d)

	return 0
}
```

-----------------------------------------------


#### Building the Flax compiler ####


##### macOS / Linux

- Flax uses a makefile; most likely some form of GNU-compatible `make` will work.
- LLVM needs to be installed. On macOS, `brew install llvm` should work. For Linux people, follow roughly `.travis.yml`, which uses apt-get to install the required libraries.
- For macOS people, simply call `make`.
- Linux people, call `make linux`.
- A C++14-compatible compiler should be used, `g++-6` is good. Due to *reasons*, `clang++` doesn't like `libstdc++`, and `libllvm` from `apt` is linked with `libstdc++`, so you basically need to use `g++` on Linux.
- Find the `flaxc` executable in `build/sysroot/usr/local/bin`
- Additionally, the (admittedly limited) standard library will be copied from `./libs` to `./build/sysroot/usr/local/lib/flaxlibs/`


##### Windows

- Open `flax.vcxproj`
- Edit the configuration variables to tell `msvc` where to find the libraries -- notably, these are needed: [`libmpir`](http://mpir.org), [`libmpfr`](http://mpfr.org), and most importantly, [`libllvm`](http://llvm.org). Follow the build instructions for each library, preferably generating both Debug and Release *static* libraries.
- Point Visual Studio to the appropriate include and lib directories (as appropriate for each configuration if you built separate Release and Debug libraries)
- Build and profit, hopefully.
- The compiler executable is placed in `build/sysroot/windows/(Release|Debug)/`

-----------------------------------------------


#### Building Flax Programs ####

- Some form of compiler (`cc` is called via `execvp()`) should be in the `$PATH` to produce object/executable files; not necessary if using JIT
- Since nobody in their right mind is *actually* using this, please pass `-sysroot build/sysroot` to invocations of the compiler -- else the compiler will default to looking somewhere in `/usr/local/lib` for libraries.
- Speaking of which, standard libraries (Foundation, etc.) are looked for in `<sysroot>/<prefix>/lib/flaxlibs/`. Prefix is set to `/usr/local/` by default.


-----------------------------------------------


#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
- Alternatively, join the discussion at #flax-lang on Freenode IRC.
- Requested help: Improved edge-case detection! With FIR now, cryptic LLVM assertions are much rarer, but might still happen.


-----------------------------------------------


#### Compiler Architecture ####

Some stuff about the compiler itself, now. As any noob looking to build a compiler would do, I used the [LLVM Kaleidoscope tutorial](http://llvm.org/docs/tutorial/) as a starting point. Naturally, it being a tutorial did no favours for the code cleanliness and organisation of the Flax compiler.

Flax itself has 3 main passes -- Tokenising and Lexing, Parsing, and finally Codegen. Yes -- there is no separate typechecking phase. Strictly speaking this is a bad practice, but as with any large structure, replacing the foundation in one fell swoop is nigh impossible.


##### Tokenising and Lexing #####

This isn't terribly complicated. Each file is naturally only looked at once, and a list of tokens and raw lines are stored somewhere. There's always a nagging feeling that token location reporting is flawed, and it probably is.

EDIT: It is.


##### Parsing #####

Broadly speaking this is a recursive descent parser, handwritten. Not terribly efficient, but whatever. This step creates the AST nodes for the next step, although there are some hacks to enable custom operators, which should probably be reimplemented sometime soon.



##### Typechecking #####

At this stage, each AST node that the parser produced is traversed, at the file-level. AST nodes are transformed through a typechecking phase into SST nodes (the
original meaning of this initialism has been lost). This typechecking consists of solidifying `pts::Type`s into `fir::Type`s; given that the former is simply
a stripped-down version of the latter, this is natural.

SST nodes are just AST nodes with refinements; identifiers are resolved to their targets, and function calls also find their intended target here.

Before the rewrite, this used to happen in an intertwined fashion with code generation, which definitely wasn't pretty.



##### Code Generation #####

After each file is typechecked, the collector forcibly squishes them together into a single unit, combining the necessary definitions from other files that
were imported -- code generation happens at the program level.

During code generation, we output 'Flax Intermediate Representation', or 'FIR'. It's basically a layer on top of LLVM that preserves much of its interface,
with some light abstractions built on top. This is where AST nodes actually get transformed into instructions.

Part of the purpose for FIR was to decouple from LLVM, and partly to allow compile-time execution in the future, which would definitely be easier with our
own IR (mainly to send and retrieve values across the compiler <> IR boundary).



##### Translation #####

After a `fir::Module` is produced by the code generator, we 'translate' this into LLVM code. The entire process can be seen in `source/backend/llvm/translator.cpp`,
and clearly we basically cloned the LLVM interface here, which makes it easy to translate into LLVM.














