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


#### Future Plans ####

- Generic Types
- Subtyping
- Internal compiler optimisations
- Incremental compilation
- Rule the world


-----------------------------------------------


#### Building the Flax compiler ####

- Flax uses a makefile; most likely some form of GNU-compatible `make` will work.
- LLVM needs to be installed. On macOS, `brew install llvm` should work. For Linux people, follow roughly `.travis.yml`, which uses apt-get to install the required libraries.
- For macOS people, simply call `make`.
- Linux people, call `make linux`.
- A C++14-compatible compiler should be used, `g++-6` is good. Due to *reasons*, `clang++` doesn't like `libstdc++`, and `libllvm` from `apt` is linked with `libstdc++`, so you basically need to use `g++` on Linux.
- Find the `flaxc` executable in `build/sysroot/usr/local/bin`
- Additionally, the (admittedly limited) standard library will be copied from `./libs` to `./build/sysroot/usr/local/lib/flaxlibs/`

- The `shakefile` still works, however. (probably. no guarantees.)

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



##### Codegen: Part 1 #####

This is where the bulk of the compiler is, and naturally the messiest part. While there is no explicit typechecking pass, the code generation at the top level is, in itself, split into 6 passes, for technical reasons.

(Most of it is to allow all function declarations to be seen first, before bodies are seen -- allowing order-independent function calling. It's not perfect.)

Each AST node is visited, and code generation into the Flax Intermediate Representation (see: `source/FlaxIR`) is done. Naturally the code generation involves checking types as well.



##### Codegen: Part 2 #####

Once the entire compilation unit (file) has been translated into FlaxIR, it is translated into LLVM IR. Since the design of FlaxIR is mostly LLVM except lacking actual code generation, it's fairly straightforward.

Once all modules are LLVM'ed, everything is linked together, and LLVM optimisations are applied. If the compiler is set to JIT, then the LLVM JIT engine is called on the module. If not, then it is compiled to an object file, upon which `cc` is invoked, to get an executable.














