core
====

Compiler/parser for simple language. For kicks.






#### Current Features ####

- Handwritten parser
- Generates LLVM bitcode.
- User-defined structs
- Arrays
- That's it ):



#### Language Syntax ####

- Actually mostly Swift-like
- 'func' to declare/define functions
- Two-pass parser, no need for forward declarations
- var id: type style variable declarations
- 'ffi' (foreign function interface) for calling into C-code
- Strictly defined types, (U?)Int(8|16|32|64) and Float(32|64)
- Arrays declared with Type[length] syntax
- Dynamic arrays not yet supported.



#### Future Plans ####

- Rule the world
- Type inference



#### Building the corescript compiler ####

- You will need clang (in your $PATH), because only it is capable of compiling LLVM bitcode to object code.
- Screw makefiles. corescript is currently using a third-party build system.
- Binaries and sources are available here: http://91.206.143.88/capri/releases/
- Ensure that the 'capri' executable is on your path (or just provide the absolute path to your shell)
- Run 'capri' in the top-level directory.
- Find the 'corec' executable in 'build/sysroot/usr/bin'
- Additionally, default libraries will be built from 'build/sysroot/lib' and the shared library files (.dylib or .so) placed in 'build/sysroot/usr/lib'







#### Language Guide ####
- Sorry this is a bit ghetto, but a more comprehensive guide will come soon.

##### General & Variables #####

```swift
// CoreScript ignores semicolons.
// It also does not distinguish between expressions and statements.

// There are 11 builtin types:
Int8, Uint8
Int16, Uint16
Int32, Uint32
Int64, Uint64
Float32, Float64
Void (which is not really a type)

// Note that integer literals are 'Int64' by default. They will be automatically casted if possible.


// Variables are declared with the 'var' syntax, followed by an identifier, a colon, then a type name.
// Type inference is not yet available, so all variable declarations need explicit type specifiers. (sorry!)
var x: Int64 = 100

// when the initialiser is left out, it will be initialised to a zero-value appropriate for the variable.

// Pointers are declared like so:
var x: Int8 ptr = 0

// Pointers to pointers are allowed:
var y: Int8 ptr ptr = 0

// Use 'deref' and 'addrof' to dereference a pointer and take the address of a variable, respectively.
var a: Int8 = 100
var b: Int8 ptr = addrof a
var c: Int8 = deref b
```

<br/>
##### Functions #####
```swift
// Functions are declared like so:
func Foo(bar: Int64, qux: Int8 ptr) -> Int32
{
    ...
}

// The return type can be omitted if the function returns Void.
// If an explicit 'return' statement is not present, the compiler will use the value of the last expression in the function as the return value, if it is of the correct type.

// Functions can prefixed with the 'public' keyword (before 'func) to make them exposed and linker-visible. They are 'private' and module-local by default.


// the keyword 'ffi' can be used to declare an external C function.
// note that this is the only kind of declaration-without-body supported in Corescript.
// Unfortunately, you still need to provide an identifier for the parameter. A fix is coming soon.
// Varargs are only supported for external functions, you cannot currently write Corescript functions taking a varialbe
// number of parameters.
ffi func printf(x: Int8 ptr, ...)

// Functions can be overloaded if they take different parameter types:
func printInt(x: Int8) { ... }
func printInt(x: Int16) { ... }
func printInt(x: Int32) { ... }

// and so on ...
// The functions' names are mangled like so:
// [basename]#_[param1Type]_[param2Type]_[paramNType]
// Functions marked with 'ffi' or the '@nomangle' attribute (see below) will not be mangled.
// Functions taking no arguments are also not mangled.


```
<br/>
##### Structs #####
```swift
// structs are obviously declared with the 'struct' keyword.
// all functions in a struct have an implicit first parameter called 'self', which is a pointer type.
// Corescript does not distinguish between pointer access and non-pointer access, both use the '.' operator.
// However, pointers-to-pointers-to-structs cannot be accessed this way. Only a single-level of indirection is supported.
struct GuideToGalaxy
{
    // structs can obviously contain members
    var theAnswer: Int64
    var billionsOfPeopleOnEarth: Int64 = 7

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
    	// implicit return
    	self.theAnswer
    }
}

struct Hitchhiker
{
	var guide: GuideToGalaxy
}

// structs are just like any other type.
var aHitchhiker: Hitchhiker								// this is initialised with a default initialiser.
var bHitchhiker: Hitchhiker ptr = addrof aHitchhiker

// this should theoretically work, but has not been tested extensively.
var answer: Int64 = bHitchhiker.guide.calculateAnswerToLifeTheUniverseAndEverything()


// functions in structs are mangled as well, since LLVM does not support namespaced functions.
// They follow this pattern:
__struct#[structName]_[mangledFuncName]


```
<br/>
##### Arrays #####
```swift
// Arrays are declared as such:
var intArr: Int64[100]

// neither dynamic arrays nor initialiser lists are supported yet.
// you can however always use a pointer. Those subscript as well.

// speaking of subscripting:
var someInt: Int64 = intArr[50]

// Fortunately, since the length of the array is known at compile time, this will fail:
var someOtherInt: Int64 = intArr[1585]

// Naturally you can assign to elements:
intArr[40] = 42
```
<br/>
##### Attributes #####
```swift
// this will be short, since there aren't a lot of attributes
// Attributes are declared using the '@name' syntax:
@nomangle func Foo(...) { ... }

// In fact, 'private', 'internal' and 'public' are attributes with special syntax.
```







#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
- Requested help: Error catching, please! A lot of the time the Corescript compiler will fail to catch invalid things, leading to failed assertions or even broken LLVM-IR being generated (on the LLVM level, which gives cryptic messages)
