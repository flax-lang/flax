# notes


each scope already exists as a StateTree, so, whenever a public definition is encountered, it is added to the
`exports` list of its tree.

when importing another module:
1. if the module is imported `as _`, add the module's StateTree to the current (toplevel) scope's `imports` list
2. if the module is imported `as foo::bar`, add the module's StateTree to the `imports` list of `foo::bar` (creating if needed)

3. do a (complete) tree traversal "in-step" with the current scope, to check for duplicate (incompatbile) definitions.
	- big oof, but if we want the current module "paradigm" to work, this has to be done.

when resolving a definition:
1. follow the same resolution order (deepest-to-widest)
2. after checking each level, additionally check the list of imports.
3. of course, we should not traverse "down" into the imported tree -- only look at its top-level defs
	- we need to be going up, not down!
4. for each imported tree, we should only check definitions that are in its `exports` list
