-- shakefile.hs
-- Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
-- Licensed under the Apache License Version 2.0.

import System.Exit
import Development.Shake
import Development.Shake.Command
import Development.Shake.FilePath
import Development.Shake.Util

main :: IO()

sysroot		= "build/sysroot"
prefix		= "usr/local"
outputBin	= "flaxc"

finalOutput	= sysroot </> prefix </> "bin" </> outputBin



llvmConfig	= "llvm-config"
disableWarn	= "-Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-c++98-compat -Wno-weak-vtables -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-c++98-compat-pedantic -Wno-conversion -Wno-shadow -Wno-global-constructors -Wno-exit-time-destructors -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-shift-sign-overflow -Wno-format-nonliteral -Wno-gnu-zero-variadic-macro-arguments -Wno-trigraphs"

cxxFlags	= "-std=gnu++1y -g -Wall -Weverything " ++ disableWarn ++ " -frtti -fexceptions -fno-omit-frame-pointer -I`" ++ llvmConfig ++ " --includedir`"

llvmConfigInvoke	= "`" ++ llvmConfig ++ " --cxxflags --ldflags --system-libs --libs core jit native bitwriter`"



compiledTest		= "build/test"
testSource			= "build/test.flx"
flaxcFlags			= "-O3 -no-lowercase-builtin -o " ++ compiledTest ++ " -sysroot '" ++ sysroot ++ "'"


main = shakeArgs shakeOptions{shakeFiles="build"} $ do
	want [compiledTest]

	phony "clean" $ do
		putNormal "Cleaning files"
		removeFilesAfter "source" ["//*.o"]


	compiledTest %> \out -> do
		orderOnly [finalOutput]
		alwaysRerun
		ls <- getDirectoryFiles "" ["libs//*.flx"]

		let libs = [sysroot </> prefix </> "lib/flaxlibs" </> (takeFileName lib) | lib <- ls]
		need libs

		Exit code <- cmd Shell finalOutput [flaxcFlags] testSource

		putNormal "======================================="
		cmd Shell (if code == ExitSuccess then [compiledTest] else ["echo Test failed"])



	"//*.flx" %> \out -> do
		let fnp = "libs" </> (takeFileName out)
		let ut = (sysroot </> prefix </> "lib" </> "flaxLibs" </> (takeFileName out))

		quietly (cmd Shell "cp" [fnp] [ut])

		-- todo: Wtf? haskell plz
		-- putNormal ("fnp: " ++ fnp)
		-- putNormal ("out: " ++ out)
		-- putNormal ("ut: " ++ ut)
		-- copyFileChanged fnp ut



	finalOutput %> \out -> do
		cs <- getDirectoryFiles "" ["source//*.cpp"]
		let os = [c ++ ".o" | c <- cs]
		need os

		cmd Shell "clang++ -o" [out] [llvmConfigInvoke] os



	"source//*.cpp.o" %> \out -> do
		let c = dropExtension out
		let m = out ++ ".m"
		() <- cmd Shell "clang++ -c" [c] [cxxFlags] "-o" [out] "-MMD -MF" [m]
		needMakefileDependencies m




















