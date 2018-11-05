## CHANGELOG (FIXED / IMPLEMENTED THINGS)

`(??)`
- static access now uses `::` instead of `.`, along the veins of C++ and Rust.
- polymorph instantiations now take the form of `Foo!<...>` (the exclamation mark) -- making it less likely to be ambiguous.
- polymorph arguments can now be positional, meaning `Foo!<int>` instead of `Foo!<T: int>`. The rules are similar to that of funtion calls
	-- no positional arguments after named arguments.

`(0b937a5)`
- add chained comparison operators, eg. `10 < x < 30` would *succinctly* check for `x` between 10 and 30.
- consequently, changed the precedence for all six comparison operators (`==`, `!=`, `<`, `<=`, `>`, and `>=`) to be the same (500)

`(f7fd4e6)`
- using unions -- including both generic and instantiated unions
- vastly improved (i'd say) inference for variants of unions (when accessing them implicitly)

`(e365997)`
- infer polymorphs with union variants (except singleton variants)
- clean up some of the `TypecheckState` god object
- fixed a number of related bugs regarding namespaced generics
- give polymorphic types a `TypeParamMap_t` if there was a `type_infer`

`(9de42cb)`
- major revamp of the generic solver -- the algorithm is mostly unchanged, but the interface and stuff is reworked.
- finally implemented the iterative solver
- variadic generics work
- took out tuple splatting for now.

`(efc961e)`
- generic unions, but they're quite verbose.

`(e475e99)`
- add tagged union types, including support for `is` and `as` to check and unwrap respectively
- infer type parameters for a type from a constructor call

`(c89c809)`
- fix the recursive instantiation of generic functions.

`(ff2a45a)`
- no longer stack allocate for arguments
- new lvalue/rvalue system in the IRBuilder, to make our lives slightly easier; we no longer need to pass a value/pointer pair,
	and we handle the thing in the translator -- not very complicated.
- add `$` as an alias for `.length` when in a subscript or slicing context, similar to how D does it.
- we currently have a bug where we cannot recursively instantiate a generic function.

`(fdc65d7)`
- factor the any-making/getting stuff into functions for less IR mess.
- fix a massive bug in `alloc` where we never called the user-code, or set the length.
- add stack-allocs for arguments so we can take their address. might be in poor taste, we'll see.
- enable more tests in `tester.flx`.

`(70d78b4)`
- fix string-literal-backslash bug.
- clean up some of the old cruft lying around.
- we can just make our own function called `char` that takes a slice and returns the first character. no compile-time guarantees,
	but until we figure out something better it's better than nothing.

`(0e53fce)`
- fix the bug where we were dealing incorrectly with non-generic types nested inside generic types.
- fix calling variadic functions with no variadic args (by inserting an empty thing varslice in the generated arg list)
- fix 0-cost casting between signed/unsigned ints causing overload failure.

`(7e8322d)`
- add `is` to streamline checking the `typeid` of `any` types.

`(d1284a9)`
- fix a bug wrt. scopes; refcount decrementing now happens at every block scope (not just loops) -- added a new `BlockPoint` thing to convey this.
- fix a massive bug where `else-if` cases were never even being evaluated.
- `stdio.flx`!

`(1aade5f)`
- replace the individual IR stuff for strings and dynamic arrays with their SAA equivalents.
- add an `any` type -- kinda works like pre-rewrite. contains 32-byte internal buffer to store SAA types without additional heap allocations,
	types larger than 32-bytes get heap-allocated.
- add `typeid()` to deal with the `any` types.

`(8ab9dad)`
- finally add the check for accessing `refcount` when the pointer is `null`; now we return `0` instead of crashing.

`(8d9c0d2)`
- fix a bug where we added generic versions for operators twice. (this caused an assertion to fire)

`(aa8f9a9)`
- fixed a bug wrt. passing generic stuff to generic functions -- now we check only for a subset of the generic stack instead of the whole thing
	when checking for existing stuff.

`(e530c81)`
- remove mpfr from our code

`(ef6326c)`
- actually fix the bug. we were previously *moving out* of arrays in a destructuring binding, which is definitely not what we want.
	now we increment the refcount before passing it off to the binding, so we don't prematurely free.

`(4d0aa96)`
- fix a massive design flaw wrt. arrays -- they now no longer modify the refcount of their elements. only when they are completely freed,
	then we run a decrement loop (once!) over the elements.
- this should fix the linux crash as well. note: classes test fails, so we've turned that off for now.

`(c060a50)`
- fix a recursion bug with checking for placeholders
- fix a bug where we would try to do implicit field access on non-field stuff -- set a flag during typechecking so we know.
- add more tests.

`(a86608b)`
- fix a parsing bug involving closing braces and namespaces.

`(e35c883)`
- still no iterative solver, but made the error output slightly better.
- also, if we're assigning the result to something with a concrete type, allow the inference thing to work with that
	information as well.

`(dbc7cd2)`
- pretty much complete implementation of the generic type solver for arbitrary levels of nesting.
- one final detail is the lack of the iterative solver; that's trivial though and i'll do that on the weekend.

`(1e41f88)`
- fix a couple of bugs relating to SAA types and their refouncting pointers
- fix a thing where single-expr functions weren't handling `doBlockEndThings` properly.
- start drilling holes for the generic inference stuff

`(337a6a5)`
- eliminate `exitless_error`, and made everything that used to use it use our new error system.

`(e479ba2)`
- overhaul the error output system for non-trivial cases (it's awesome), remove `HighlightOptions` because we now have `SpanError`.
- make osx travis use `tester.flx` so we stop having the CI say failed.

`(6385652)`
- sort the candidates by line number, and don't print too many of the fake margin/gutter things.
- change typecache (thanks adrian) to be better.

`(ce9f113)`
- magnificently beautiful and informative errors for function call overload failure.

`(80d5297)`
- fix a couple of unrelated bugs
- varidic arrays are now slice-based instead of dynarray-based
- variadic functions work, mostly. not thoroughly tested (nothing ever is)

`(65b25b3)`
- parse the variadic type as `[T: ...]`
- allow `[as T: x, y, ... z]` syntax for specifying explicitly that the element type should be `T`.

`(25dadf0)`
- remove `char` type; everything is now `i8`, and `isCharType()` just checks if its an `i8`
- constructor syntax for builtin types, and strings from slices and/or ptr+data
- fix a couple of mutability bugs here and there with the new gluecode.

`(f3a06c3)`
- actually add the instructions and stuff, fix a couple of bugs
- `fir::ConstantString` is now actually a slice.
- move as many of the dynamic array stuff to the new `SAA` functions as possible.
- increase length when appending

`(0ceb391)`
- strings now behave like dynamic arrays
- new concept of `SAA`, string-array-analogues (for now just strings and arrays lol) that have the `{ ptr, len, cap, refptr }` pattern.

`(312b94a)`
- check generic things when looking for duplicate definitions (to the best of our abilities)
- actually make generic constructors work properly instead of by random chance

`(1734444)`
- generic functions work
- made error reporting slightly better, though now it becames a little messy.

`(c911408)`
- actually make generic types work, because we never tested them properly last time.
- fixed a bug in `pts::NamedType` that didn't take the generic mapping into account -- also fixed related issue in the parser

`(860b61e)`
- move to a `TCResult` thing for typechecking returns, cleans up generic types a bunch
- fix a bug where we couldn't define generic types inside of a scope (eg. in a function)

`(1b85906)`
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


