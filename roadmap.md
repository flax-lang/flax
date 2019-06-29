# Flax 1.0 Roadmap

As more of the core features are slowly being finished up, the 1.0 milestone is in sight. This is just a list of things that
should be finished before the initial implementation can be labelled "fit-for-use". Of course, this doesn't preclude any
bugfixes that are necessary.


## Features

1. Traits
2. Extensions
3. Cleanup and modernise operator overloading
4. Standard library
5. Proper documentation for the entire language and all features
6. Proper introduction to language features (eg. for a README)


### Cleanup/modernise Operator Overloading

The operator overloading mechanism is dated pre-resolver rewrite, which means that it probably isn't as extensible as we'd
like it to be. For example, polymorphic operators aren't a possiblity currently, and that should be a thing that is allowed.

