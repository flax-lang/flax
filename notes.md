## notes

## to fix

1. `public static` doesn't work, but `static public` works.

2. underlining breaks for multi-line spans; just don't underline in that case.

3. types don't appear before functions for some reason, for typechecking. (ie. order becomes important)

4. enums need to be fixed, for real.

6. "unsynchronised use of global init function!!!" -- need to figure out a way to serialise access to the global init function

7. polymorphic stuff breaks when manually instantiating

8. compiler crash:
```
import libc as _

struct Cat<T> {
     fn greet() {
        printf("i love manga uwu\n")
    }
}

@entry fn main() {
    let c: Cat!<Integer> = Cat()

    c.greet();
}
```

9. ambiguous call to initialiser of class

10. assignment to runtime variable in #run block

11. numbers basically don't cast properly at all



## to refactor

3. all uses of defer() need to be audited. there are instances where an exception is thrown during typechecking
	as a result of unwrapping a `TCResult` that contains an error; as the stack unwinds, it may encounter a
	defer() object, which necessitates calling the code inside.

	- in general that is fine (most of the time, it's just `fs->pushLoc()` followed by `defer(fs->popLoc())`),
		but in some instances it is not fine --- most notably, when we push a body context and pop it in the defer.

	- since the popping of the body context asserts that the top of the stack is the same as what we expect, we might
		end up in a situation where exception-throwing function pushes a different context and throws before popping it,
		leaving the calling frame with an inconsistent view

	- the assertion will cause the compiler to terminate when it should have just printed the error message generated
		by the exception-throwing function.

4. instead of keeping a separate list of `unresolvedGenericDefns` which is pretty ugly, it should be feasible to create
	a sort of `sst::ParametricDefn` that simply contains a pointer to the original `ast::Parameterisable` as its only
	field; then, we should be able to simplify the typechecking code substantially by moving those the polymorph
	instantiation into that definition's typecheck method instead.

5. setting the method's containing class via the `infer` parameter feels super fucking dirty, and ngl it has felt dirty
	for the past 5 years.


## to investigate

1. we rely on a lot of places to set `enclosingScope` correctly when typechecking structs etc. there should
	be a better way to do this, probably automatically or something like that. basically anything except error-prone manual setting.
