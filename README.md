flax
====

Compiler/parser for simple language. For kicks.


#### Language Goals ####

- This project grew out of my frustration with C/C++. I use C++ mainly for low level development, and its syntax is cumbersome, laden in C-derived K&R cruft... and the include files. Don't get me started on include files.
- Flax targets the LLVM backend, simply because it is widely ported, has an incredible amount of optimisation passes, and can target freestanding platforms.
- This language aims to have as little dependencies against userspace libraries as possible.
- Therefore, only a small subset of libc functions will be required. While the language has not been finalised, I envision only basic functions (printf, malloc/free, memset/memcpy/memmove) being required.
- The builtin standard library (tentatively 'Foundation') will be provided as a source-level library (written in Flax!), making it much easier to build against a freestanding target.
- Why so much talk about freestanding targets? I eventually want to write userspace programs for my hobby OS (https://github.com/requimrar/mx) in Flax, and if possible parts of the kernel or drivers.




#### Current Features ####

- Handwritten parser
- Generates LLVM bitcode.
- User-defined types (structs, mostly)
- Arrays (compile-time fixed size only, dynamic arrays yet to be)
- Pointer syntax (C-like)
- Operator overloading on structs
- Function overloading (C++-ish)



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



#### Future Plans ####

- Rule the world
- Type inference



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







#### Language Guide ####
- Sorry this is a bit ghetto, but a more comprehensive guide will come soon.

#### General & Variables ####

Flax ignores semicolons.
It also does not distinguish between expressions and statements.
There are 11 builtin types:

```swift
Int8, Uint8
Int16, Uint16
Int32, Uint32
Int64, Uint64
Float32, Float64
Void                // (which is not really a type)
```

Note that integer literals are 'Int64' by default. They will be automatically casted if possible.
Variables are declared with the 'var' syntax, followed by an identifier, a colon, then a type name.
Type inference is not yet available, so all variable declarations need explicit type specifiers. (sorry!)

```swift
var x: Int64 = 100
```

when the initialiser is left out, it will be initialised to a zero-value appropriate for the variable.
Pointers are declared like so (both are equally valid):

```swift
var x: Int8 ptr = 0
var y: Int8* = 0

// Pointers to pointers:
var y: Int8 ptr ptr = 0
var z: Int8** = 0
```

Use 'deref' and 'addrof' to dereference a pointer and take the address of a variable, respectively.
C-style syntax is supported for these, except dereferencing uses the pound '#' operator

```swift
var a: Int8 = 100
var b: Int8 ptr = addrof a
var c: Int8 = deref b

var a2: Int8 = 100
var b2: Int8* = &a
var c2: Int8 = #b

```

Pointers can also be indexed into. No bounds-checking will be done, however. (Planned)

```swift
var f: Int8* = malloc(100)
f[2] = 42
```

<br/>
#### Functions ####
Functions are declared like so:

```swift
func Foo(bar: Int64, qux: Int8 ptr) -> Int32
{
	...
}
```

The return type can be omitted if the function returns Void.
Implicit returns are planned, but support is currently iffy and has been disabled.

Functions can prefixed with the 'public' keyword (before 'func) to make them exposed and linker-visible. They are 'private' and module-local by default.


The keyword 'ffi' can be used to declare an external C function.
note that this is the only kind of declaration-without-body supported in Flax.
Unfortunately, you still need to provide an identifier for the parameter. A fix is coming soon.
Varargs are only supported for external functions, you cannot currently write Flax functions taking a varialbe
number of parameters.

```swift
ffi func printf(x: Int8 ptr, ...)
```

Furthermore, functions can be overloaded if they take different parameter types.
The functions' names are mangled like so:
`[basename]#_[param1Type]_[param2Type]_[paramNType]`
Functions marked with 'ffi' or the '@nomangle' attribute (see below) will not be mangled.
As a result, they cannot be overloaded.
Functions taking no arguments will be mangled, taking a single parameter of type 'void'. (aka nothing)

```swift
func printInt(x: Int8) { ... }
func printInt(x: Int16) { ... }
func printInt(x: Int32) { ... }
```


<br/>
#### Structs ####

Structs are declared with the 'struct' keyword.
All functions in a struct have an implicit first parameter called 'self', which is a pointer type.
Flax does not distinguish between pointer access and non-pointer access, both use the '.' operator.
However, pointers-to-pointers-to-structs cannot be accessed this way. Only a single-level of indirection is supported.

```swift
struct GuideToGalaxy
{
	// structs can obviously contain members
	var theAnswer: Int64
	var billionsOfPeopleOnEarth: Int64 = 7			// inline initialisers are allowed.

	// You can declare an init() function yourself
	init()
	{
		// unfortunately, init functions taking parameters are not yet supported.
		self.theAnswer = (100 - 94) * self.billionsOfPeopleOnEarth

		// all variables are always initialised with an appropriate zero-value, regardless of the presence of a user-defined
		// initialiser.
	}

	func calculateAnswerToLifeTheUniverseAndEverything() -> Int64
	{
		// implicit return (TODO: not supported currently)
		self.theAnswer
	}
}
```

Structs can be used just like any other type:

```swift
struct Hitchhiker
{
	var guide: GuideToGalaxy
}

var aHitchhiker: Hitchhiker								// this is initialised with a default initialiser.
var bHitchhiker: Hitchhiker* = &aHitchhiker

// this should theoretically work, but has not been tested extensively. (struct members in structs)
var answer: Int64 = bHitchhiker.guide.calculateAnswerToLifeTheUniverseAndEverything()

```
functions in structs are mangled as well, since LLVM does not support namespaced functions.
They follow this pattern:
`__struct#[structName]_[mangledFuncName]`



<br/>
#### Arrays ####
Arrays are declared as such:

```swift
var intArr: Int64[100]
```

Neither dynamic arrays nor initialiser lists are supported yet.
You can however always use a pointer. Those subscript as well.
speaking of subscripting:

```swift
var someInt: Int64 = intArr[50]
```

Fortunately, since the length of the array is known at compile time, this will fail

```swift
var someOtherInt: Int64 = intArr[1585]
```

Of course, array elements can be assigned to.

```swift
intArr[40] = 42
```



<br/>
#### Attributes ####
This will be short, since there aren't a lot of attributes
Attributes are declared using the '@name' syntax:

```swift
@nomangle func Foo(...) { ... }
```
In fact, 'private', 'internal' and 'public' are attributes with special syntax.
User-defined attributes and some form of reflection (at runtime) are planned.

<br />
#### Control Flow ####
If-else statements are supported, and naturally nestable.

```swift
var x: Int8 = 10
if x > 20
{
	...
}
else if x < 5
{
	...
}
else
{
	...
}
```


As you can see, parentheses around the conditional expression
are optional.
Furthermore, single-line C-style ifs like this
are not supported, and will not be.

```c
if(cond)
	doSomething();

```



#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
- Requested help: Error catching, please! A lot of the time the Flax compiler will fail to catch invalid things, leading to failed assertions or even broken LLVM-IR being generated (on the LLVM level, which gives cryptic messages)
