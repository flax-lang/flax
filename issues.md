# Issues

Note: this is just a personal log of outstanding issues, shorter rants/ramblings, and a changelog that doesn't need me to scroll through git.


## FEATURES TO IMPLEMENT

2. Destructors for classes


5. String operators


6. Location information for type specifiers in definitions


7. 'cases' member of enums to enable runtime enumeration of... the enumeration.


8. Operator overloading for assignment and subscript/slice


14. Multi-dimensional arrays, as opposed to our current 'array-of-arrays' approach
	eg. index with `foo[a, b, c]` instead of `foo[a][b][c]`


16. `[[noreturn]]` for functions, so we don't error when no value is returned (eg. when calling `abort()`)


18. Some way of handling both types and expressions in `sizeof`/`typeid`/`typeof`. Might work if we just check for identifiers, but then
	what about polymorphic types? Those won't even parse as expressions.


-----


## THINGS TO FIX

2. There are still some instances where we explicitly 'initialise' a class equivalent to `memset(0)` -- see *THINGS TO NOTE* below.



------


## THINGS TO INVESTIGATE

* ### Possibly allow struct-constructors to init transparent fields


* ### Errors need to propagate better
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



* ### Certain cases we still allow a zeroinitialised class to exist:
	A. When growing a dynamic array, the new elements are zeroinitialised instead of having a value set (for non-class types, they get their default value)
	B. When initialising a field in a struct, if not explicitly assigned to it is left as it is -- which should be the zeroinitialiser
	C. When initialising a field in a class without an inline initialiser, we explicitly set it to 0.

	We either document this properly, or change the behaviour. I don't really want to devolve into the C++ style of forcing super-explicit initialiser
	syntax. However, we can enforce setting a value by forcing an inline initialiser for classes.

	For structs, we have 2 options -- 1: screw it, just make a zeroinit class if they appear in a struct; 2: only allow initialising structs with
	constructers that specify a value for any class types.

	For the former, I'm more inclined to do that, but the latter is more 'correct', as it were. Unfortunately, that would also mean disallowing
	`var foo: SomeStruct` without an initialiser...



* ### Some kind of construct to make a variable immutable beyond a certain point
	thus you could have arbitrarily complex initialisation code,
	then have the compiler enforce that your variable is immutable after that point, eg:

	```
		var k = ...
		if(some_cond1)  k = get_value()
		else            k = get_other_value()

		k.mutating_func()

		make_immutable(k)

		k = ...      // will not compile
	```


* ### Type inference for single-expr functions? It's a little weird to have two arrows like this:
	`fn foo(a: T) -> T => a * a`

	The type inference would require some re-working though, because to generate the declaration of the function we need the return type, but to
	get the return type in this situation we need to typecheck the function body. Albeit it's a single function, there might still be
	unresolved or unresolvable things inside the body.

	Possibly investigate whether we can do the typechecking in the generateDecl function?? But it's highly likely we separated those for a reason,
	so I don't think it's doable at this point.

	It's not really a high-priority thing anyway.


* ### wrt. optional arguments
	you *must* refer to it by name to specify a value. For example:

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


* https://proandroiddev.com/understanding-generics-and-variance-in-kotlin-714c14564c47
	https://en.wikipedia.org/wiki/Covariance_and_contravariance_(computer_science)



* Arguments-after-varargs (see https://youtu.be/mGe5d6dPHAU?t=1379)
	Basically, allow passing named parameters after var-args.


* Optimisation: use interned strings for comparison (and for use as keys in all the hashmaps we have), hopefully allowing a large-ish speedup since
	(according to historical profiles) `std::string`-related things are the cause of a lot of slowdowns.



* Right now we design the `any` type to not handle refcounted types at all -- meaning it doesn't touch the refcount of those, and thus
	the `any` can outlive the referred-to value.

	To change this, `any` itself would need to be refcounted, but then we'd need to be able to check whether something was refcounted
	** AT RUNTIME **, which is a whole bucket of shit I don't want to deal with yet.

	So for now, we are documenting that `any` does not care about reference counting, and deal with it later.



* A similar issue with refcounting for casting, though i'm not entirely sure if this is a real issue or just something that i'm imagining.

	So, when we assign something `x = y`, then it is necessary that the types of `x` and `y` are the same. Then, if the type in question is a
	reference counted type, we do some `autoAssignRefCountedValue()`, that does separate things for lvalues and rvalues. If the right-hand side
	is an lvalue, then we increment its reference count; else, we perform a 'move' of the rvalue by removing it from the reference-counting stack.

	The problem comes when we need to cast `y` to the type of `x`. If we had to do a cast, that means that we transform the (potential) rhs-lvalue into
	an rvalue. This means that, if the rhs was originally an lvalue, we would try to remove the output of the casting op from the refcounting stack,
	which it doesn't exist in.

	So, we will add the output of the casting op to the refcounting list, if the output type is reference counted. The issue comes with how we handle
	the reference count of the *original* rhs.

	The potential cases that I can think of where we might get some trouble involves `any`:

	```rust
	let x = string("hello")
	let b: any = x
	```

	In this case, `x` is an lvalue that gets casted to an rvalue of type `any`. In the assignment, we remove the casted rvalue from the rc-stack,
	(assuming we implement the fix above), then just do the store.

	```rust
	let x: any = string("world")
	let y = x as string
	```

	In this case, it's a similar thing; if we apply the mentioned fix, I don't *see* any issues...

	TODO: actually investigate this properly.

-----





## POLYMORPHIC PIPELINE DOCUMENTATION

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




















