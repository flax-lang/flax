core
====

Compiler/parser for simple language. For kicks.






#### Current Features ####

- Handwritten parser
- Generates LLVM bitcode.
- User-defined structs
- Arrays
- That's it ):



#### Language Syntax ####

- Actually mostly Swift-like
- 'func' to declare/define functions
- Two-pass parser, no need for forward declarations
- var id: type style variable declarations
- 'ffi' (foreign function interface) for calling into C-code
- Strictly defined types, (U?)Int(8|16|32|64) and Float(32|64)
- Arrays declared with Type[length] syntax
- Dynamic arrays not yet supported.





#### Future Plans ####

- Rule the world
- Type inference



#### Building the corescript compiler ####

- Screw makefiles. corescript is currently using a third-party build system.
- Binaries and sources are available here: http://91.206.143.88/capri/releases/
- Ensure that the 'capri' executable is on your path (or just provide the absolute path to your shell)
- Run 'capri' in the top-level directory.
- Find the 'corec' executable in 'build/sysroot/usr/bin'
- Additionally, default libraries will be built from 'build/sysroot/lib' and the shared library files (.dylib or .so) placed in 'build/sysroot/usr/lib'


#### Contributing (haha, who am I kidding) ####

- Found a bug? Want a feature?
- Just submit a pull request!
