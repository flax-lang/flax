# Random Rants


## 17th February 2018
### RE: Class inheritance, take 2

Nothing much has actually been done the past 3 weeks, from the time when the previous thing was written, other than the fact that we managed to get
`using` working on both namespaces and enum cases.

Apart from that, there are some issues that need to be resolved; firstly, the problem of accesssing base-class fields from a derived class without an
explicit `super.foo` (same thing with methods). For this, it should be somewhat straightforward (famous last words), because we don't allow overriding of
methods and fields. (kind of -- non-virtual overriding for methods is prohibited). Thus, if we first check for duplicates, we should be able to throw
appropriate error messages, and then just import the stuff in the base class to our own class. Assuming the base class has been typechecked prior (which
we enforce), then we should transitively also import any grandparent, great-grandparent, etc definitions into ourselves.

The next issue is constructors (or init-functions). For these, we want to only be able to call them with `super(...)`, instead of some other kind of thing.
Thus, when we import stuff into ourselves as above, we must exclude importing constructors.

The conflict is that we *should* be able to "hide" constructors in a derived class that's -- the opposite of what we want for methods. Reason being that we
always know the type of what we're constructing, regardless of the dynamic type; the same cannot be said of method calls.

So, base-class init methods must only be accessible from a `super()` call, and `init()` calls must only expose the derived class constructors.





## 28th January 2018
### RE: Class inheritance

As of this writing, we have a very basic version of inheritance working, that hopefully isn't too poorly architected such that we'd have troubles down the
line. So, as to how it works:

The actual mechanism of 'copying' base-class fields into the derived class happens at the *FIR* layer, and the front/middle-end code doesn't need to know
about the specifics of that. To the typechecker/code-generator, once we `setBaseClass(...)` on a `fir::ClassType`, the fields from the base class 'magically'
appear in the derived class, and we can access them by name like normal fields.

This naturally precludes us from having a field with the same name as another in the base class. We do this check recursively during typechecking, so you
cannot 'hide' any fields (and in the future, methods) of any class along your inheritance hierarchy.

Back to the FIR implementation: we don't actually *physically copy* the fields into the member list of the ClassType (because they're not *really* there).
Instead, when you call one of the field-related functions like `getElementIndex()` and `getElementWithName()` or something, if they're not found in the
current class, we traverse upwards till we find it (or error out). Note that, thus, `getElementCount()` returns the number of elements *defined in the class*,
*excluding* those defined in base classes.

In the translator (to LLVM), that's where we physically insert the fields from base classes into the data representation of the derived class. Theoretically,
it shouldn't be that hard to offset all accesses by 1 to have our vtable be at the beginning of the class (in memory). Theoretically.


Either way, there's still some gaps:

1. We can't do the implicit self thing when accessing base-class fields -- right now we need to use `self.base_field` to access it. Will be fixed.
2. Methods are completely not handled at all, and we've got nothing on the front of dynamic dispatch at all. Oh well.
3. Inheriting from a non-terminal class (eg. `A : B`, where `B : C`) has not been tested. Although it should work, I have no idea.

This is likely the end-of-the-week commit for now.



## 27th January 2018
### RE: Class constructors

Currently, we know that we need to call the proper constructor when making class values. Problem is, do we even want to allow implicit instantiation
of classes? We can enforce that class types must all be explicitly initialised. This would disallow the following pattern, however:

```
var foo: SomeClass
if(some_condition)
	foo = getClass(10)

else
	foo = getClass(20)
```
Even though a human can tell that `foo` will always be given a valid value regardless of the branch taken, the compiler in its current state (and indeed
even clang++, I believe), cannot.

For the current iteration, I think we cannot enforce the 'explicit initialisation' matter because the compiler simply doesn't allow facilities to support
productive programming *without* it. Once we can get some level of proper analysis about the thing above (which should be simple; loops don't guarantee
anything, and for if-elses: all `if` branches must explicitly assign to it, and there *must* be an `else` branch), we can probably do the original idea.

In that situation, we can leave the default value of `foo` as a `memset` of 0. We cannot use it before it is assigned so its actual contents don't matter
(but unlike C/C++ is actually in a deterministic state, just perhaps not valid to things that operate on the class); so we don't have to do unnecessary
work initialising a dummy value which would just be replaced eventually.

Note: use-before-init is also one of the analyses that we need to have before we can implement this.


### RE: AllocA in the Compiler

In the current state of the compiler codebase, there's a lot less of the pattern where we create a temporary stack allocation, perform some operations,
then load to get the value -- compared to the previous iteration (pre-rewrite). This is because of the way LLVM's value types were defined, in not having
addressses of values.

However, there are still some potential cases where I think it can be slightly improved, such as calling class constructors. In that situation, the init
function needs a `self` pointer, but there isn't one (we're creating it!). The current solution is to create a temporary allocation (to get a pointer),
pass that to the constructor, then load it and return that value.

The problem is, is there a better way to do this?





## 25th January 2018
### RE: Virtual methods

For starters, I think it should be necessary to mark functions as `virtual` at the declaration site *at the base class*, in order to allow dynamic
dispatch on that function. The following, non-exhaustive scenario list applies:

A: We define a virtual method in the base class, and overload it in derived classes explicitly with an `override` keyword (or something similar).
It would be an error to virtually-override a method without such a keyword.

B: We define a normal, non-virtual method in the base class, and attempt to overload it in a derived class. Without an explicit overload
keyword, this is an error -- we don't really want to support method-"hiding" the way C++ does, as it's most likely an unintentional thing.

C: We define a non-virtual method in the base class, and attempt to overload it with the keyword, or by declaring another method but marking
it as `virtual`. This will create an error, since the base class did not indicate that the given method was supposed to be accessible via dynamic
dispatch.

D: We define an extension for the base class, and using some as-yet-undecided syntax, re-declare a given method as a virtual function. Now, derived
classes see the original method as if it had always been declared virtual, and dynamic dispatch works. While this seems like (and in reality *is*)
extra work on everybody's part, it's much more explicit that we're re-jiggering something in the base class, and modifying it in such a way that the
original author might not have intended it to be done.


This might seem like some kind of futile discussion if we can just go into the library itself and edit the definition to make the function virtual, but
someday when we support some kind of binary format for libraries (and no longer do source-only kinda thing), it would come in handy.

However, we should consider the ramifications of modifying the ABI of a function like that. It would be fine-*ish* if we modified the ABI of such a method
if the changes were only visible to the current program -- since we compile the whole program simultaneously (well you know what I mean, in 1 TU), then
any and all extensions that modify classes like this would be visible to the rest of the program, and the compiler can adjust how it calls the given method.

On the other hand, if we're creating some kind of library, then it is impractical to either (a) make the ABI changes only affect the current program, or
(b) transparently allow other programs transitively depending on the base class to know about the new calling ABI.


At the end of the day, while we want safety, we cannot compromise *that much* on flexibility. If the original base class author did not mark the function
as virtual, it's a question of how much we want to 'bend' to allow users of that class to call a given method dynamically. It might have been an accidental
omission, or it might've been fully intentional.

Something like the `final` keyword could be used (but of course not that itself -- I think the well-known meaning of `final` should stay, ie. to mark
that a class cannot be derived from), to say that all methods defined in such a marked class cannot be "re-defined" later to be virtual, or perhaps
the opposite (that any and all given methods can be re-declared to be virtual in derived classes). Maybe an `open class { ... }`?






## 2nd December 2017
### RE: Multi-dimensional arrays

In the old system, we had an `alloc[N][M]` syntax that would give so-called 'multi-dimensional' arrays, which were really just an array of arrays.
Right now, I've removed the multi-dim alloc, but we want something to replace it, potentially for matrix purposes.

So, with the current syntax `alloc(T, N, ...)`, if you give more than one dimension, then you get a multi-dimensional array back. Of course, the number
of dimensions is fixed at compile-time, since we know how many lengths were passed to `alloc()`. The length of each dimension would be a runtime
variable, but fixed -- so you could have an NxM matrix where N and M are user-inputs, but you can only ever fix it as NxM. I feel like I explained that
very poorly.

Here's the in-memory representation of such a `MultiDimArray` with `K` dimensions of type `T`:
```
struct __multi_dim_array_K_T
{
	ptr: T* = ...
	dims: i64[K] = [ N, M, ... ]
}
```

The type of this array is `T[,,]`, for `K = 3`. The slight issue with this is that, for instance, if we are passing such a multi-dim array around, then
the size of each dimension is not fixed, even though the number of dimensions is; eg. we could be passing a 2x2 matrix to a function expecting a 3x3 matrix.
Both are 2D arrays, but one is clearly larger than the other.

To mitigate this problem, we're probably also going to have fixed-dimension arrays, of the type `T[N, M]`, which function like the fixed arrays of today,
where `T[2, 2]` is a distinct type from `T[3, 3]`. Otherwise, they'd function similarly and perform similar actions. One would be able to pass a fixed
multi-array to a variable-multi-array, but not vice-versa -- for obivous reasons.

However, there should be no confusion as to the variable nature of the non-fixed multi-dim array; they vary in the size of each dimension, yes, but only
*once* -- they are runtime-allocated, non-expanding arrays. So, if you did `let mda = alloc(int, 3, 3)`, then `mda` is a 3x3 matrix, but there is no way
to *expand* it to a 4x4 matrix for example, without first freeing, then re-alllocating `mda`.

Speaking of which, the members of these multi-dim arrays are stored in contiguous memory, and some math is done to access them. Since we know the number
of dimensions, the there should be no need for any loops at runtime.







### RE: Everything is broken

Well, turns out the entirety of arrays and shit are completely and utterly broken. There's some madness going on with cloning and growing and reallocations.
Somehow, we have a bug where some particular string's length is being corrupted, but not the rest. I hate this stupid memory debugging, it sucks ass.

Here's the deal:
```
PRE X
POST X
ZERO
ONE
grow (realloc) array: 0000016644113BE8 / len 1 / cap (old) 1 / cap (new) 4
TWO
THREE
FOUR
grow (realloc) array: 00000166447CA508 / len 4 / cap (old) 4 / cap (new) 8
FIVE ALL OK - (aaa, 00000166441907E7, 3), (BBB, 00000166441907F3, 3), (ccc, 00000166441907FF, 3), (DDD, 000001664419080B, -842150451),
(eee, 0000016644190817, 3)
OK 2
OK 3
hello, clone, 0/5
clone string 000001664481CFC0, 00000166441907E7, 3, 12
clone string 'aaa' / 3 / 000001664481CFC8
hello, clone, 0/5
hello, clone, 1/5
clone string 000001664481D7E0, 00000166441907F3, 3, 12
clone string 'BBB' / 3 / 000001664481D7E8
hello, clone, 1/5
hello, clone, 2/5
clone string 000001664481D420, 00000166441907FF, 3, 12
clone string 'ccc' / 3 / 000001664481D428
hello, clone, 2/5
hello, clone, 3/5
clone string 0000000000000000, 000001664419080B, -842150451, -842150442
```

Notice how, for the string 'DDD', the length is some super negative number, but those around it aren't affected at all. I'm not sure why this is
happening, but I'm not looking forward to debugging this mess tomorrow (or today, rather).

#### Solved
Fixed it -- at the core of the issue was an off-by-one thing. Took the opportunity to clean up some of the debugging output.





## 1st December 2017
### RE: Decomposition

Decomposition should be able to work like it did before the rewrite, with one small caveat -- we need to be generating fake definitions during the typecheck
phase, so that the `valueMap` has a unique referrer during the code generation phase. Otherwise, it should be good.

One case worth considering is when wanting to declare multiple variables to have the same value, like so:
```
let (a, b, c) = 30;
```
Currently that would be a compiler error, since `30` isn't a tuple type with 3 members. One potential solution is to use a 'splat' operator that
'multiplies' its operand an specified number of times, like so:
```
let (a, b, c) = ...30;
```
Since this isn't going to be code-generated, at the assignment site during typechecking we can just check if the RHS is one of these splat operators,
and do appropriate magic to handle it. Otherwise, it's going to be an invalid construct.

There's also the case where we want to splat a single value into an N-sized array containing N such values, but there's no way to do this at compile-time,
and will probably need to be a runtime operation. No big deal, not a high priority.







### RE: Even more scoping problems

Some progress has been made, but some other progress has been lost. So, it turns out that scoping is somewhat important during code generation,
namely for the `ValueTree`s that we have. It would be simple to just rely on a program-wide `valueMap` to resolve values from definitions, but the
issue comes when we're doing reference counting.

Since automatic reference counting is entirely dependent on semantic scopes to know when to decrement the reference count, each list of reference
counted values must be intrinsically tied to a scope, and have its reference count be manipulated in accordance with that scope.

So, we have a dilemma here; reinstate the code-generation scoping system, or find another way to store the reference counted things. The reason why we're
facing the problem that we are, is that when we 'leave' the current scope to generate, for instance, our function-call-target, the value-tree doesn't change,
and we mess up the book-keeping of the reference counted things.

One possible solution is to tie the reference counted values with the SST nodes directly; each `Block` will store its own list of reference counted things,
instead of relying on the `CodegenState` to do it? Is this feasible? By the time we reach the code-generation state, there really should be no issue with this
thing. Each `Block` by right should only be touched once, and be generated once; so it should be ok to associate the `fir::Value`s with the node itself.


One problem I have with all of these 'hack-ish' solutions is that I have no idea how well they'll cooperatate with generic functions and types, and further
down the line proper incremental compilation. It feels like the entire compiler is built on the assumption that we have complete and unfettered access to
all nodes in all files, which might not be the case when we separate out compilation into proper modules and such.

Time will tell how it goes -- I really do not foresee the mental endurance or capacity for another rewrite when the time comes.


The best part is that this isn't even the initial issue I set out to solve; it was uncovered by debugging and setting up a reproducible test case. I haven't
gotten around to debugging the initial issue yet, which is that we're somehow not generating global variables properly, leading to a compilation failure
in the `intlimits` test.

I don't even have the slightest idea about that one.


#### Solved
Turns out the 'initial' problem was a manifestation of the deeper problem with `ValueTree`s; I've just eliminated their entire existence from the
compiler, and made `ControlFlowPoint`s store the reference counting things instead -- this way we don't need to modify the SST nodes to insert
stateful values, and the nesting nature is controlled by a simple stack that's run through during codegen instead of stored permanently somewhere
we don't need.

----

## 30th November 2017
### RE: More scoping problems

So, it turns out that the problem I was describing in (3) below (previous day) turns out to be true. We teleport into the scope of the dot-operator
when doing typechecking:

```
let a = 10;
let b = 30;
foo.bar(a, b);
```

In the above snippet, `a` and `b` are resolved in the scope of `foo`, instead of the current scope. Thus, the two local variables `a` and `b` are not
seen at all, leading to a compilation error.

Because we need to know the type of the arguments (and hence typecheck them) in resolving overloads, we still need to be in the previous thing.
So, possibly the solution is to break apart our resolution into separate steps, and inevitably duplicate some code...

1. Teleport to the scope of `foo`, and find function candidates from there.
2. After getting the candidates, teleport back to the original scope, and perform overload resolution (and hence argument typechecking) there.
3. Then, it's done, hopefully.

This necessitates splitting up the resolution code so we can do more fine-grained resolving.
Another possible alternative is to typecheck each argument first, before teleporting to the function scope??? who knows.

----

## 29th November 2017
### RE: Import-as and scoping problems

Here's some notes on how to fix this import-scope-problem, hopefully.

1. Functions need to be decoupled from their scopes more.

	Here's the thing: we perform module import shennanigans after each module is typechecked. So, the proper thing to do is that
	the 'scope' of each definition is its *original* scope. Any other way to refer to it is like a nickname, and doesn't change
	the original.

	However we do it, we must ensure that the SST nodes in the original typechecked module are left *untouched and immutable*, since
	we don't want module X that imports module Y to change how module Z sees module Y's definitions. (duh, you shouldn't be changing
	the state of something that you're importing since you don't own it)

	The real problem comes during code generation. For some reason (not-yet-looked-into), we need to 'teleport' to the original scope
	of the definition before continuing with code generating. Seems quite weird, honestly.

	In theory, if we implemented everything according to my internal mental model, during typechecking all references that may be involved
	in any kind of scoped operation should already have their target resolved (in terms of a `sst::Defn*` pointer) and stored. So, there
	really shouldn't be any kind of scope-related operation during code generation. Given that we're in fact not doing any kind of dot-op
	resolution at code-generation time, this should be doable.

	We need to investigate whether we can eliminate the necessity to have the scope-teleporting system in place. If so, it makes our job
	slightly (or a lot) easier. If it cannot be done, then appropriate changes must be made, once investigations show the exact reason
	we need the teleporting nonsense.


	After that, it shouldn't matter in what scope we generate the function.



2. Definitions shouldn't be explicitly aware of the scope in which they're being poked at.

	The current situation is that definitions are teleporting around and being self-aware. The thing we need to do is... not to have that.
	Assuming that point (1) was successfully resolved (ie. definitions care less about their scope), then during code-generation we shouldn't
	have to care *at all* about scopes.

	The current module being typechecked will, by guarantee, be importing modules that have already been typechecked, and their public
	definitions inserted into the proper `StateTree` location in the current module. Thus, when we traverse the StateTree to resolve
	an identifier (for example during a dot-op resolution), we get the definition -- if all goes according to plan, it doesn't care about
	its scope; we just set the target of the dot-op (for example) and hand it off to code generation.

	One thing -- during dot-op checking, we perform scope teleportation. However, at typecheck time this shoudn't be an issue, since whatever
	we're trying to actively resolve should only reside in our own module and whatever that's outside should have already been resolved.
	So, by right there shouldn't be any issues.

	The only reason we're teleporting around when resolving scopes is so we don't have to manually 'resolve' any things ourselves.
	For such a statement: `foo.bar()`, if we just teleport to the scope of `foo`, and let `bar()` typecheck itself, then we basically
	do what needs to be done without duplicating work.

	Sidenote: (TODO: FIXME:) one issue that I can forsee (again, based on my limited mental model that assumes 100% perfect implementation) is
	that, when typechecking the function arguments for the call to `bar()`, (eg. `bar(x, y, z)`), we will basically "absorb" whatever's in `foo`,
	such they will implicitly "prefer" `foo.x`, `foo.y`, and `foo.z` (if they exist). This can (will?) cause unexpected shadowing issues that are
	counter-intuitive. This is an issue.



3. StateTree aliases need to exist

	This is required for import-as, and probably using, statements. Again, if everything was implemented according to my internal mental
	model, then this should be as simple as twisting the `subtrees` map a little bit. Assuming that module `X` is aliased to the
	identifier `foo`, then `subtrees["foo"] = X`.

	For nested usings, eg. `using foo = X.Y.Z`, it might be possible to just `subtrees["foo"] = Z`. If it works, it greatly simplifies a great
	load of bullshit. Plus, it should following scoping rules:

	```
	do {
		using foo = bar;
	}

	foo.hello()		<-- invalid
	```

	The using statement is placed in its own stree due to the 'do'-block pushing a new scope, and the 'fake-mapping' doesn't 'escape' from
	the scope.

	Note that there might need to be some considerations for 'public using', where we export an alias. Most of the time we don't want to be
	importing aliases (it's like putting 'using namespace std' in a header file, thus giving everybody including that header to contract
	metaphorical, programming-STDs), so we need a way to "filter" out these. Right now we can do a simple check consisting of (first, if the
	map-key matches the actual tree name, and if true, if the parent of the tree is the current parent)

	(note: the second check is for the case were we have `using foo = bar.foo`, where the names would match but the trees aren't directly
	related so it'd still be an alias)

	TODO: FIXME: However, I feel like this entire system is slightly somewhat kind of completely utterly not-very-convincing-nor-robust, so,
	provided what we say *actually works*, we should, *at some point*, look into actually making some proper book-keeping for using/aliases.

	Also a side issue is that, since named StateTrees are paired with NamespaceDefns, I think a similar thing needs to be done for the 'defns'
	list that we have, and possibly the same, non-robust "is-this-an-alias" check consisting of name checking. Problem is, there's no "parent"
	thing for us to do the second check.

	Based on an entirely un-verified preliminary consideration, it might not be necessary to maintain `sst::NamespaceDefns`. In fact, they're
	really unnecessary and feel like duplicates of the stuff that StateTrees contain, just inferior. If we remove them, then the alias-checking
	problem won't be an issue.

	On a surface-level kind of thing, it seems like resolutions of identifiers primarily use the `StateTree` definitions map to find their targets,
	so it should be entirely feasible to eliminate `NamespaceDefn`s at the SST level.






