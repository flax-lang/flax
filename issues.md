# Issues

Note: this is just a personal log of outstanding issues, shorter rants/ramblings, and a changelog that doesn't need me to scroll through git.


### FEATURES TO IMPLEMENT


2. Type solver/unifier for generic function calls


3. Optional arguments.


4. Public and private imports (ie. do we re-export our imports (currently the default), or do we keep them to ourselves (the new default)


5. String operators


6. Location information for type specifiers in definitions


7. 'cases' member of enums to enable runtime enumeration of... the enumeration.


8. Operator overloading for assignment and subscript/slice


9. Figure out how to reconcile generic functions/methods with structs/classes.


14. Multi-dimensional arrays, as opposed to our current 'array-of-arrays' approach
	eg. index with `foo[a, b, c]` instead of `foo[a][b][c]`


16. `[[noreturn]]` for functions, so we don't error when no value is returned (eg. when calling `abort()`)


17. `i8.min` prints `128` instead of `-128`     UPDATE: only on windows????

-----


### THINGS TO FIX

2. There are still some instances where we explicitly 'initialise' a class equivalent to `memset(0)` -- see *THINGS TO NOTE* below.


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


### POLYMORPHIC PIPELINE DOCUMENTATION

So, this thing serves as the shitty documentation for how the generic pipeline works for future implementations.

1. When AST nodes are typechecked at the top level, if they are polymorphic things they will refuse to be checked,
	and won't return a result (`ast::Block` does the same check), but instead add themselves to a list of pending,
	uninstantiated generic types in the `sst::StateTree`.

2. When resolving a reference (function call or identifier), we check the pending generic list if we can't find any
	normal things that match (or if we already have some explicit mappings). If there is a match, we call into
	`attemptToDisambiguateGenericReference()` with whatever information we have (eg. partial solutions)

3. We call `inferTypesForGenericEntity` which is the main solver function that infers types for the type arguments
	that are missing. This just acts like a black box for the most part.

4. If we find that we're actually a reference to a function (ie. we act like a function pointer instead of a call) then
	we do a thing called `fillGenericTypeWithPlaceholders` which puts in fake 'solutions' for each type argument (aka
	`fir::PolyPlaceholderType`), and proceeds to return it.

5. We will then allow that to typecheck. For now, only functions can have placeholder types, so we special-case that in
	function typechecking by just skipping the body typecheck when we have placeholders.

6. Once we manage to solve everything -- ie. get rid of all the placeholder types, we need to re-typecheck the original definition
	with proper filled in types. This happens in `resolveFunctionCall`.




------


### THINGS TO INVESTIGATE


0. Errors need to propagate better
	Right now with the newly-implemented `PrettyError` system, we can propagate a lot more information upwards, and with the new thing of throwing an
	error when we unwrap a `TCResult`, there's less need to be explicit when handling errors during typechecking.

	Unfortunately, for a simple failed generic function instantiation, we get a mess like this:
	```
	(supertiny.flx:159:37) Error: No such function named 'foo' (in scope 'supertiny.main().__anon_scope_0')
		let k = foo<T: int, G: str>(10, 20)
								^
	(supertiny.flx:159:37) Error: No viable candidates in attempted instantiation of parametric entity 'foo'; candidates are:
		let k = foo<T: int, G: str>(10, 20)
								^
	(supertiny.flx:159:37) Error: Parametric entity 'foo' does not have an argument 'G'
		let k = foo<T: int, G: str>(10, 20)
								^
	(supertiny.flx:157:13) Note: 'foo' was defined here:
		fn foo<T>(a: T, b: T) -> T { return a + b }
	```

	Which is counter to our goal of having readable error messages even in the face of failed template instantiations (looking at you, C++)

	Possibly we need more 'kinds' of errors, where we can have boilerplate prefixes like `Candidate not suitable: <bla bla>`, rather than cascading
	multiple errors (most of them should be `info` anyway, and not errors), and also the possiblity of just posting text without the `Error`/`Warning` etc
	prefix. Also, we should be able to control the context-printing of each of those as well.



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


5. Type inference for single-expr functions? It's a little weird to have two arrows like this:
	`fn foo(a: T) -> T => a * a`

	The type inference would require some re-working though, because to generate the declaration of the function we need the return type, but to
	get the return type in this situation we need to typecheck the function body. Albeit it's a single function, there might still be
	unresolved or unresolvable things inside the body.

	Possibly investigate whether we can do the typechecking in the generateDecl function?? But it's highly likely we separated those for a reason,
	so I don't think it's doable at this point.

	It's not really a high-priority thing anyway.


6. wrt. optional arguments, you *must* refer to it by name to specify a value. For example:

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


14. Flesh out the builtin methods for arrays/strings. In particular, do we want to keep allowing `+` and `+=` to work on arrays? Seems a bit dubious. We
	already encounter the issue where we do `[string] + [char:]`, which can be seen as trying to append a string literal `[char:]` to an array of strings,
	which doesn't work.

-----






