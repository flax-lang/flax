-- shakefile.hs
-- Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
-- Licensed under the Apache License Version 2.0.

{-# OPTIONS_GHC -fno-warn-tabs #-}

import System.Exit
import System.IO.Unsafe()
import Data.IORef()
import Data.Maybe()
import Development.Shake
import Development.Shake.Command
import Development.Shake.FilePath
import Development.Shake.Util

main :: IO()

sysroot		= "build/sysroot" :: [Char]
prefix		= "usr/local" :: [Char]
outputBin	= "flaxc" :: [Char]

finalOutput	= sysroot </> prefix </> "bin" </> outputBin :: [Char]



llvmConfig	= "llvm-config"
disableWarn	= "-Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-c++98-compat -Wno-weak-vtables -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-c++98-compat-pedantic -Wno-conversion -Wno-shadow -Wno-global-constructors -Wno-exit-time-destructors -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-shift-sign-overflow -Wno-format-nonliteral -Wno-gnu-zero-variadic-macro-arguments -Wno-trigraphs -Wno-extra-semi -Wno-reserved-id-macro"

compiledTest		= "build/test"
testSource			= "build/test.flx"
flaxcNormFlags		= "-O3 -sysroot " ++ sysroot ++ " -no-lowercase-builtin -o '" ++ compiledTest ++ "'"
flaxcJitFlags		= "-Ox -sysroot " ++ sysroot ++ " -no-lowercase-builtin -run"


main = shakeArgs shakeOptions { shakeFiles = "build", shakeVerbosity = Quiet } $ do
	want ["build"]

	phony "build" $ do
		need [finalOutput]
		putNormal "======================================="
		cmd Shell finalOutput [flaxcJitFlags] testSource


	phony "compile" $ do
		need [compiledTest]


	phony "clean" $ do
		putQuiet "Cleaning files"
		removeFilesAfter "source" ["//*.o"]


	compiledTest %> \out -> do
		need [finalOutput]
		alwaysRerun

		Exit code <- cmd Shell finalOutput [flaxcNormFlags] testSource

		putNormal "======================================="
		cmd Shell (if code == ExitSuccess then [compiledTest] else ["echo Test failed"])



	"//*.flx" %> \out -> do
		let fnp = "libs" </> (takeFileName out)
		need [fnp]

		let ut = (sysroot </> prefix </> "lib" </> "flaxLibs/")
		quietly $ cmd Shell "cp" [fnp] [ut]


	phony "copyLibraries" $ do
		--- copy the libs to the prefix.
		() <- quietly $ cmd Shell "mkdir" "-p" (sysroot </> prefix </> "lib" </> "flaxlibs")
		quietly $ cmd Shell "cp" ("libs/*.flx") (sysroot </> prefix </> "lib" </> "flaxlibs/")


	finalOutput %> \out -> do
		cs <- getDirectoryFiles "" ["source//*.cpp"]
		let os = [c ++ ".o" | c <- cs]
		need os

		maybelconf <- getEnvWithDefault llvmConfig "LLVM_CONFIG"
		let lconf = maybelconf

		let llvmConfigInvoke = "`" ++ lconf ++ " --cxxflags --ldflags --system-libs --libs core engine native linker bitwriter`"

		() <- cmd Shell "clang++ -g -o" [out] os [llvmConfigInvoke]
		putQuiet ("\x1b[0m" ++ "# built " ++ out)

		need ["copyLibraries"]


	"source//*.cpp.o" %> \out -> do
		let c = dropExtension out
		let m = out ++ ".m"

		maybelconf <- getEnvWithDefault llvmConfig "LLVM_CONFIG"
		let lconf = maybelconf

		let cxxFlags = "-std=c++11 -O0 -g -Wall -Weverything " ++ disableWarn ++ " -frtti -fexceptions -fno-omit-frame-pointer -I`" ++ lconf ++ " --includedir` -Isource/include" ++ " -Xclang -fcolor-diagnostics"

		putQuiet ("\x1b[0m" ++ "# compiling " ++ c)
		() <- cmd Shell "clang++ -c" [c] [cxxFlags] "-o" [out] "-MMD -MF" [m]

		needMakefileDependencies m




















