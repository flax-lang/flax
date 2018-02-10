# Issues

Note: this is just a personal log of outstanding issues, shorter rants/ramblings, and a changelog that doesn't need me to scroll through git.


### FEATURES TO IMPLEMENT


1. Class inheritance and virtual methods


2. Using namepace


3. Optional arguments.


4. Public and private imports (ie. do we re-export our imports (currently the default), or do we keep them to ourselves (the new default)


5. String operators


7. 'cases' member of enums to enable runtime enumeration... of the enumeration.


8. Operator overloading for assignment and subscript/slice


13. Generic functions & types


14. Multi-dimensional arrays, as opposed to our current 'array-of-arrays' approach
	eg. index with `foo[a, b, c]` instead of `foo[a][b][c]`


16. `[[noreturn]]` for functions, so we don't error when no value is returned (eg. when calling `abort()`)


-----


### THINGS TO FIX

1. The 'default-value' thing when we create temporaries or other stuff needs to be fixed; classes that do not have an initialiser taking 0 arguments
	cannot be 'default-value'-ed. This is basically like any other langauge eg. C++ when it comes to this.

	Also, we need to enforce classes having at least one initialiser. Not entirely sure how we're going to be dealing with private/protected/public
	visibility on these yet. If we do do anything, it's basically just going to be enforced by the compiler, just like immutability...

	Oh well.


2. We need to be calling the inline-initialiser glue function from class constructors. Right now, I think it's a better idea to just insert a manual
	call to it when we *create* the value, rather than insert a call to it from within user-defined constructors itself. It'll be slightly less messy,
	but we need to remember to call the inline-init glue function whenever we create a class from anywhere. Probably should put that in a helper function.


3. Fix the `char`/`i8` stupidity when handling strings. The way I see it, there are 2 options:
	a) make `char` redundant; strings are just `i8` everywhere. if we want unicode, then it'll be a separate (`ustring`?) type.
	b) make `char` distinct; strings would handle unicode in terms of codepoints, maybe utf-32. would be pretty bad
		for most things though.

	Probaby going with option A.


-----


### THINGS TO INVESTIGATE

1. Should slices be a 'weak' reference to the elements?
	ie. should making a slice of a dynamic array increase the refcount of the elements of the dynamic array?
	Right now, we don't increment the reference count -- ie. we've implemented weak slices.

	Do we want strong slices?


2. Foreach loops where you take more than one thing at a time, like this, maybe:
	`for [ first, middle, last, ... ] in list { ... }`


3. Variadic functions should take a slice of `any`.


4. Type inference for single-expr functions? It's a little weird to have two arrows like this:
	`fn foo(a: T) -> T => a * a`

	The type inference would require some re-working though, because to generate the declaration of the function we need the return type, but to
	get the return type in this situation we need to typecheck the function body. Albeit it's a single function, there might still be
	unresolved or unresolvable things inside the body.

	Possibly investigate whether we can do the typechecking in the generateDecl function?? But it's highly likely we separated those for a reason,
	so I don't think it's doable at this point.

	It's not really a high-priority thing anyway.


5. wrt. named parameters:

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


6. wrt. tuples:
	well that's all done and over with. Tuples can be splatted in arbitrary locations at function callsites, but are treated as a positional argument.
	So, you cannot have named arguments before the splatted tuple, and any named arguments after the fact must not conflict with the positionally-
	-specified arguments that came from the splatted tuple.

	You can splat more than one tuple per callsite, and there really isn't much of an implementation issue because we expand the splat op in-place when
	typechecking parameters, so that we get a bunch of sst::TupleDotOps in the typechecking, and the actual overload-resolution-thingy doesn't know
	the difference.


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


-----



### CHANGELOG (FIXED / IMPLEMENTED THINGS)

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






