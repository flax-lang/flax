# Random Rants


## 2nd December 2017
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






