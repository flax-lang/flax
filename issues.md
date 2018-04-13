# Issues

Note: this is just a personal log of outstanding issues, shorter rants/ramblings, and a changelog that doesn't need me to scroll through git.


### FEATURES TO IMPLEMENT


3. Optional arguments.


4. Public and private imports (ie. do we re-export our imports (currently the default), or do we keep them to ourselves (the new default)


5. String operators


7. 'cases' member of enums to enable runtime enumeration of... the enumeration.


8. Operator overloading for assignment and subscript/slice


13. Generic functions & types


14. Multi-dimensional arrays, as opposed to our current 'array-of-arrays' approach
	eg. index with `foo[a, b, c]` instead of `foo[a][b][c]`


16. `[[noreturn]]` for functions, so we don't error when no value is returned (eg. when calling `abort()`)


-----


### THINGS TO FIX

2. There are still some instances where we explicitly 'initialise' a class equivalent to `memset(0)` -- see *THINGS TO NOTE* below.

3. Fix the `char`/`i8` stupidity when handling strings. The way I see it, there are 2 options:
	a) make `char` redundant; strings are just `i8` everywhere. if we want unicode, then it'll be a separate (`ustring`?) type.
	b) make `char` distinct; strings would handle unicode in terms of codepoints, maybe utf-32. would be pretty bad
		for most things though.

	Probaby going with option A.


4. Some kind of construct to make a variable immutable beyond a certain point; thus you could have arbitrarily complex initialisation code,
	then have the compiler enforce that your variable is immutable after that point, eg:

	```
		var k = ...
		if(some_cond1)  k = get_value()
		else            k = get_other_value()

		k.mutating_func()

		make_immutable(k)

		k = ...      // will not compile
	```



-----


### THINGS TO NOTE




------


### THINGS TO INVESTIGATE


1. Certain cases we still allow a zeroinitialised class to exist:
	A. When growing a dynamic array, the new elements are zeroinitialised instead of having a value set (for non-class types, they get their default value)
	B. When initialising a field in a struct, if not explicitly assigned to it is left as it is -- which should be the zeroinitialiser
	C. When initialising a field in a class without an inline initialiser, we explicitly set it to 0.

	We either document this properly, or change the behaviour. I don't really want to devolve into the C++ style of forcing super-explicit initialiser
	syntax. However, we can enforce setting a value by forcing an inline initialiser for classes.

	For structs, we have 2 options -- 1: screw it, just make a zeroinit class if they appear in a struct; 2: only allow initialising structs with
	constructers that specify a value for any class types.

	For the former, I'm more inclined to do that, but the latter is more 'correct', as it were. Unfortunately, that would also mean disallowing
	`var foo: SomeStruct` without an initialiser...


3. Foreach loops where you take more than one thing at a time, like this, maybe:
	`for [ first, middle, last, ... ] in list { ... }`


4. Variadic functions should take a slice of `any`.


5. Type inference for single-expr functions? It's a little weird to have two arrows like this:
	`fn foo(a: T) -> T => a * a`

	The type inference would require some re-working though, because to generate the declaration of the function we need the return type, but to
	get the return type in this situation we need to typecheck the function body. Albeit it's a single function, there might still be
	unresolved or unresolvable things inside the body.

	Possibly investigate whether we can do the typechecking in the generateDecl function?? But it's highly likely we separated those for a reason,
	so I don't think it's doable at this point.

	It's not really a high-priority thing anyway.


6. wrt. named parameters:

	there are two cases of named parameters being used; first in a type constructor, and second in a regular function call.

	When calling a type constructor of a struct, all arguments are optional, ie. you can specify all, some or none of the fields
	as named arguments in your constructor call. Those not provided will be given the default value for their type, or, if the
	struct definition itself has initialisers at the field declaration site, that particular init value.

	On the other hand, the constructor for a class acts like a normal function, in that only defined 'init' functions can be called,
	and they can only be called with their corresponding arguments.

	For these regular function calls, named parameters are must come after any positional arguments, and positional arguments cannot come after
	named arguments. Thus, once you start naming arguments in a call, you must name all subsequent arguments.

	Note that functions cannot be overloaded solely on the names of the arguments, just the types.

	Finally, for optional arguments, it behaves much the same, except that you *must* refer to it by name to specify a value. For example:

	`fn foo(a: int, b: int = 3) => ...`

	```
	// valid combinations:
	foo(30)
	foo(a: 30)
	foo(30, b: 5)
	foo(a: 30, b: 1)

	// invalid combinations:
	foo(a: 30, 1)		<-- cannot have positional arguments after named ones
	foo(30, 7)			<-- must name the optional argument 'b'

	```


	Yep, that's about it for named args. I don't plan on supporting the whole 'internal/external name' thing that Swift has going on.
	It's probably just an objective-c fetishism thing, and muddles up function declarations. If you want the internal name to be different,
	just create a new variable, it's not going to kill the program.


8. https://proandroiddev.com/understanding-generics-and-variance-in-kotlin-714c14564c47
	https://en.wikipedia.org/wiki/Covariance_and_contravariance_(computer_science)


9. Some kind of metaprogramming system, but on more than one level. To have some level of useful metaprogramming, the metaprogram must be able to inspect,
	and to some extent modify, the internal state of the compiler. It must also integrate well with the existing system, and our Compile-Time-Execution
	engine must be robust enough to seamlessly handle passing values between the compiler itself, and any compile-time program and/or metaprogram. This is
	paramount to anything working properly.

	If we allow the user-level metaprogram to create new constructs in the language, then we must be able to call user-defined typecheck functions, not
	to mention allowing the user to access the IRBuilder to facilitate code generation. This might be far too complex already.

	For the first, entry-level kind of metaprogramming (if you can call it that, can you?), we should start with some kind of macro system, but instead of
	textual manipulation we should be aiming for AST manipulation. For instance transforming one kind of AST node into another kind, etc. There's not an
	immediately obvious use-case I can think of for this right now, but hopefully it'll come eventually.


10. Arguments-after-varargs (see https://youtu.be/mGe5d6dPHAU?t=1379)
	Basically, allow passing named parameters after var-args.


11. Optimisation: use interned strings for comparison (and for use as keys in all the hashmaps we have), hopefully allowing a large-ish speedup since
	(according to historical profiles) `std::string`-related things are the cause of a lot of slowdowns.


12. Constructor-style syntax for builtin types, eg. `string("foo")` since string literals are now of type `[mut char:]`


13. Make string refcounting not obscene, and move to the `refcount-pointer-is-a-separate-thing` scheme that we switched to for dynamic arrays. The current
	system seems way too fragile.


14. Flesh out the builtin methods for arrays/strings. In particular, do we want to keep allowing `+` and `+=` to work on arrays? Seems a bit dubious. We
	already encounter the issue where we do `[string] + [char:]`, which can be seen as trying to append a string literal `[char:]` to an array of strings,
	which doesn't work.

-----



### CHANGELOG (FIXED / IMPLEMENTED THINGS)

`(??)`
- make init methods always mutable, for obvious reasons. Also, virtual stuff still works and didn't break, which is a plus.

`(7107d5e)`
- remove all code with side effects (mostly `eat()` stuff in the parser) from within asserts.

`(e9ebbb0)`
- fix type-printing for the new array syntax (finally)
- string literals now have a type of `[char:]`, with the appropriate implicit casts in place from `string` and to `&i8`
- add implicit casting for tuple types if their elements can also be implicitly casted (trivial)
- fix an issue re: constant slices in the llvm translator backend (everything was null)
- distinguish appending and construct-from-two-ing for strings in the runtime-glue-code; will probably use `realloc` to implement mutating via append for
	strings once we get the shitty refcount madness sorted out.

`(81a0eb7)`
- add `[mut T:]` syntax for specifying mutable slices; otherwise they will be immutable. If using type inference, then they'll be inferred depending on
	what is sliced.

`(f824a97)`
- overhaul the mutability system to be similar to Rust; now, pointers can indicate whether the memory they point to is mutable, and `let` vs `var` only
	determines whether the variable itself can be modified. Use `&mut T` for the new thing.
- allow `mut` to be used with methods; if it is present, then a mutable `self` is passed in, otherwise an immutable `self` is passed.
- add casting to `mut`: `foo as mut` or `foo as !mut` to cast an immutable pointer/slice to a mutable one, and vice versa.
- distinguish mutable and non-mutable slices, some rules about which things become mutable when sliced and which don't.

`(ec9adb2)`
- add generic types for structs -- presumably works for classes and stuff as well.
- fix bug where we couldn't do methods in structs.
- fix bug where we treated variable declarations inside method bodies as field declarations
- fix bug where we were infinite-looping on method/field stuff on structs

`(d9133a8)`
- change type syntax to be `[T]` for dynamic arrays, `[T:]` for slices, and `[T: N]` for fixed arrays
- change strings to return `[char:]` instead of making a copy of the string. This allows mutation... at your own risk (for literal strings??)
- add `str` as an alias for the aforementioned `[char:]`

`(b48e10f)`
- change `alloc` syntax to be like this: `alloc TYPE (ARGS...) [N, M, ...] { BODY }`, where, importantly, `BODY` is code that will be run on each element in
	the allocated array, with bindings `it` (mutable, for obvious reasons), and `i` (immutable, again obviously) representing the current element and the index
	respectively.
- unfortunately what we said about how `&T[]` parses was completely wrong; it parses as `&(T[])` instead.

`(b89aa2c)`
- fix how we did refcounts for arrays; instead of being 8 bytes behind the data pointer like we were doing for strings, they're now just stored in a separate
	pointer in the dynamic array struct itself. added code in appropriate places to detect null-ness of this pointer, as well as allocating + freeing it
	appropriately.
- add for loops with tuple destructuring (in theory arbitrary destructuring, since we use the existing framework for such things).
- add iteration count binding for for-loops; `for (a, b), it in foo { it == 1, 2, ... }`

`(3eb36eb)`
- fix the completely uncaught disaster of mismatched comparison ops in binary arithmetic
- fix bug where we were double-offsetting the indices in insertvalue and extractvalue (for classes)
- fix certain cases in codegen where our `TypeDefn` wasn't code-generated yet, leading to a whole host of failures. // ! STILL NOT ROBUST
- fix typo in operator for division, causing it not to work properly (typoed `-` instead of `/`)
- change pointer syntax to use `&T` vs `T*`. fyi, `&T[]` parses intuitively as `(&T)[]`; use `&(T[])` to get `T[]*` of old
- change syntax of `alloc` (some time ago actually) to allow passing arguments to constructors; new syntax is `alloc(T, arg1, arg2, ...) [N1, N2, ...]`

`(6dc5ed5)`
- fix the behaviour of foreach loops such that they don't unnecessarily make values (and in the case of classes, call the constructor) for the loop variable

`(f7568e9)`
- add dynamic dispatch for virtual methods (WOO)
- probably fix some low-key bugs somewhere

`(7268a2c)`
- enforce calling superclass constructor (via `init(...) : super(...)`) in class constructor definitions
- fix semantics, by calling superclass inline-init function in derived-class inline-init function
- refactor code internally to pull stuff out more.

`(ba4de52)`
- re-worked method detection (whether we're in a method or a normal function) to handle the edge case of nested function defs (specifically in a method)
- make base-class declarations visible in derived classes, including via implicit-self
- method hiding detection -- it is illegal to have a method in a derived class with the same signature as a method in the base class (without virtual)

`(e885c8f)`
- fix regression wrt. scoping and telporting in dot-ops (ref `rants.md` dated 30/11/17)

`(c94a6c1)`
- add `using ENUM as X`, where `X` can also be `_`.

`(8123b13)`
- add `using X as _` that works like import -- it copies the entities in `X` to the current scope.

`(1830146)`
- add `using X as Y` (but where `Y` currently cannot be `_`, and `X` must be a namespace of some kind)

`(23b51a5)`
- fix edge cases in dot operator, where `Some_Namespace.1234` would just typecheck 'correctly' and return `1234` as the value; we now report an error.

`(00586be)`
- add barebones inheritance on classes. Barebones-ness is explained in `rants.md`

`(f7a72b6)`
- fix variable decompositions
- enable the decomposition test we had.
- disable searching beyond the current scope when resolving definitions, if we already found at least one thing here. Previous behaviour was wrong, and
	screwed up shadowing things (would complain about ambiguous references, since we searched further up than we should've)

`(1be1271)`
- fix emitting `bool` in IR that was never caught because we never had a function taking `bool` args, thus we never mangled it.
- add class constructors; all arguments must be named, and can only call declared init functions.

`(c6a204a)`
- add order-independent type usage, a-la functions. This allows `A` to refer to `B` and `A` to simultaneously refer to `B`.
- fix detection (rather add it) of recursive definitions, eg. `struct A { var x: A }`, or `struct A { var x: B }; struct B { var x: A }`
- add `sizeof` operator

`(b7fb307)`
- add static fields in classes, with working initialisers

`(81faedb)`
- add error backtrace, but at a great cost...

`(b4dabf6)`
- add splatting for single values, to fill up single-level tuple destructures, eg. `let (a, b) = ...10; a == b == 10`

`(597b1f2)`
- add array and tuple decomposition, and allow them to nest to arbitrarily ridiculous levels.

`(f8d983c)`
- allow assignment to tuples containing lvalues, to enable the tuple-swap idiom eg. `(a, b) = (b, a)`

`(e91b4a2)`
- add splatting of tuples in function calls; can have multiple tuples

`(9e3356d)`
- improve robustness by making range literals `(X...Y)` parse as binary operators instead of hacky postfix unary ops.

`(7cb117f)`
- fix a bug that prevented parsing of function types taking 0 parameters, ie. `() -> T`

`(4eaae34)`
- fix custom unary operators for certain cases (`@` was not being done properly, amongst other things)

`(d06e235)`
- add named arguments for all function calls, including methods, but excluding fn-pointer calls

`(d0f8c93)`
- fix some bugs re: the previous commit.

`(ec728cd)`
- add constructor syntax for types (as previously discussed), where you can do some fields with names, or all fields positionally.

`(ec7b2f3)`
- fix false assertion (assert index > 0) for InsertValue by index in FIR. Index can obviously be 0 for the first element.

`(3add15b)`
- fix implicit casting arguments to overloaded operators
- add ability to overload unicode symbols as operators

`(d2f8dbd)`
- add ability to overload prefix operators
- internally, move Operator to be a string type to make our lives easier with overloading.

`(dcc28ba)`
- fix member access on structs that were passed as arguments (ie. 'did not have pointer' -- solved by using ExtractValue in such cases)
- fix method calling (same thing as above) -- but this time we need to use ImmutAlloc, because we need a `this` pointer
- add basic operator overloading for binary, non-assigment operators.

`(45e818e)`
- check for, and error on, duplicate module imports.

`(5a9aa9e)`
- add deferred statements and blocks

`(80a6619)`
- add single-expression functions with `fn foo(a: T) -> T => a * 2`

`(3b438c2)`
- add support for reverse ranges (negative steps, start > end)

`(f3f8dbb)`
- add ranges
- add foreach loops on ranges, arrays, and strings
- add '=>' syntax for single-statement blocks, eg. `if x == 0 => x += 4`

`(dacc809)`
- fix lexing/parsing of negative numerical literals

`(ca3ae4b)`
- better type inference/support for empty array literals
- implicit conversion from pointers to booleans for null checking

`(5be4db1)`
- fix runtime glue code for arrays wrt. reference counting

`(e3a2b55)`
- add alloc and dealloc
- add dynamic array operators (`pop()`, `back()`)

`(b7c6f74)`
- add enums

`(408260c)`
- add classes






