## notes

each scope already exists as a StateTree, so, whenever a public definition is encountered, it is added to the
`exports` list of its tree.

when importing another module:
1. if the module is imported `as _`, add the module's StateTree to the current (toplevel) scope's `imports` list
2. if the module is imported `as foo::bar`, add the module's StateTree to the `imports` list of `foo::bar` (creating if needed)

3. do a (complete) tree traversal "in-step" with the current scope, to check for duplicate (incompatbile) definitions.
	- big oof, but if we want the current module "paradigm" to work, this has to be done.

(done) when resolving a definition:
1. follow the same resolution order (deepest-to-widest)
2. after checking each level, additionally check the list of imports.
3. of course, we should not traverse "down" into the imported tree -- only look at its top-level defs
	- we need to be going up, not down!
4. for each imported tree, we should only check definitions that are in its `exports` list

to refactor using:
we should simply treat it as a scope-level import, ie. for `using foo::bar as qux`, we just create a new scope
`qux`, then attach `foo::bar` to `qux` by appending to its `imports` list. here, we should check that `qux` does
not already exist.
(this is different for imports, where two imported modules are able merge their exported namespaces. for `using`,
i'm not too sure that's a good idea, but it's easy enough to change if needed)

for `using foo::bar as _`, we do the same thing as `import as _`, and just attach the tree of `foo::bar` to
the imports list of the current scope.



## to fix

1. `public static` doesn't work, but `static public` works.

2. underlining breaks for multi-line spans; just don't underline in that case.


## to refactor

2. a lot of places probably still have the concept of `scope == std::vector<std::string>`.
	- after the first scope refactor, i think these instances will be reduced
	- undoubtedly there will be more. for instance, Identifier holds the scope as exactly that.
		(but, is there actually a need for it to be anything else? it really is just an identifier, after all)

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

1. we rely on a lot of places to set `realScope` (and `enclosingScope`) correctly when typechecking structs etc. there should
	be a better way to do this, probably automatically or something like that. basically anything except error-prone manual setting.
