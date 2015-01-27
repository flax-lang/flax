## Flax Language Guide ##
-------------------------

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
Bool                // (which is not really a type)
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

