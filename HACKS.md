## What is this file? ##
========================

This file documents all (or most) of the code hacks that are in the Flax compiler.
It's mostly serves to document these so that efforts can be made in the future to fix them.

### Hacks ###
=============

1. The compiler generates less than optimal code.
This mostly applies to structs. This is the reason why the `lhsPtr` argument was introduced to `::codegen()`. For struct access, a `Struct*` is required for `CreateStructGEP()`. Therefore, without an `lhsPtr`, llvm's architecture dictates we create an `alloca`, then a `store` from the original `lhs`... Which generates very bad and non-performant IR.

2. The `lhsPtr` argument itself isn't used by most `::codegen()` functions...

3. `@strong typealias` and `enum` are currently implemented by wrapping their base type in a `struct`, which creates a lot of if-checks at various sites. Tentatively labelled a hack, since I don't know if there's a better way.



### Code Smells ###
===================

1. There's a lot of duplicate, redundant, and messy code everywhere, particularly in `BinOpCodegen.cpp` and `CodegenUtils.cpp`

2. `if` checks and workarounds are littered everywhere in the codebase.

3. Generic function resolution is really suboptimal. There should be a way to prune the list of potential candidates before having to loop through each one.
