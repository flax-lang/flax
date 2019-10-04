# hmm

Param
(
	[string] $buildType,
	[string] $theProgram,

	[Parameter(ValueFromRemainingArguments=$true)]
	[string[]]$extraArgs
)

robocopy "libs" "build\sysroot\usr\local\lib\flaxlibs" /mir /nfl /ndl /njh /njs /nc /ns /np

if($buildType -eq "release") {
	$buildDir = "build\meson-rel"
}

if($buildType -eq "debug") {
	$buildDir = "build\meson-dbg"
}

if($buildType -eq "debugopt") {
	$buildDir = "build\meson-reldbg"
}

ninja -C $buildDir

if($?) {
	cls
	& $buildDir\flaxc.exe -Ox -sysroot build\sysroot -run "build\$theProgram.flx" $extraArgs
}

if($buildType -eq "release") {
	copy "$buildDir\flaxc.exe" build\sysroot\usr\local\bin\
}
