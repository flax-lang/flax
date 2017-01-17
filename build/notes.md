misc notes
==========




## Windows porting

1. Need to fix compile mode. JIT mode mostly works, I think. In compile mode, files aren't written at all.
2. shit like `fdopen()`, `write()`, etc. need fucking underscores in windows, because it's a special little child. `_fdopen`, `_write`.
	- `fdopen()` is called internally to write to stderr during a runtime error. It's the only portable way to do so.
3. Getting the full path of a file in windows is *SUPER FUCKING COMPLICATED* apparently, because their fucking API functions don't even follow their own fucking MSDN spec sheet.
	- `GetFullPathName` never seems to return a non-zero value, so we need a fuckton more code than we would for unix. `realpath()` ftw.
4. `vasprintf` doesn't fucking exist. linux has it. BSD has it. but no, special little fucking windows doesn't have it.
	- CODE GETS MESSIER BECAUSE OF YOU
5. `setenv()`? NO, WE MUST BE SPECIAL AND CALL IT `_putenv_s`!
6. ALSO UCS2 IS STUPID, WCHAR_T IS STUPID