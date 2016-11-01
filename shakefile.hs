-- shakefile.hs
-- Copyright (c) 2014 - 2015, zhiayang@gmail.com
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

sysroot		= "build/sysroot"
prefix		= "usr/local"
outputBin	= "flaxc"

finalOutput	= sysroot </> prefix </> "bin" </> outputBin



llvmConfig	= "llvm-config-3.8"
disableWarn	= "-Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-c++98-compat -Wno-weak-vtables -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-c++98-compat-pedantic -Wno-conversion -Wno-shadow -Wno-global-constructors -Wno-exit-time-destructors -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-shift-sign-overflow -Wno-format-nonliteral -Wno-gnu-zero-variadic-macro-arguments -Wno-trigraphs -Wno-extra-semi -Wno-reserved-id-macro -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-redundant-move -Wno-nullability-completeness -Wno-comma -Wno-undefined-func-template"

compiledTest		= "build/test"
testSource			= "build/test.flx"
flaxcNormFlags		= "-sysroot " ++ sysroot ++ " -o '" ++ compiledTest ++ "'"
flaxcJitFlags		= "-sysroot " ++ sysroot ++ " -run"
glFrameworks		= "-framework GLUT -framework OpenGL -lsdl2"

main = shakeArgs shakeOptions { shakeVerbosity = Quiet, shakeLineBuffering = False } $ do
	want ["jit"]


	phony "jit" $ do
		need [finalOutput]

		Exit code <- cmd Shell finalOutput [flaxcJitFlags] testSource
		cmd Shell (if code == ExitSuccess then ["echo"] else ["echo Test failed"])

	phony "gl" $ do
		need [finalOutput]

		Exit code <- cmd Shell finalOutput ["-sysroot " ++ sysroot ++ " -run -framework GLUT -framework OpenGL -lsdl2"] "build/gltest.flx"
		cmd Shell (if code == ExitSuccess then ["echo"] else ["echo Test failed"])

	phony "glcompile" $ do
		need [finalOutput]

		Exit code <- cmd Shell finalOutput ["-sysroot " ++ sysroot ++ " -o build/gltest " ++ glFrameworks] "build/gltest.flx"
		cmd Shell (if code == ExitSuccess then ["echo"] else ["echo Test failed"])






	phony "compile" $ do
		need [compiledTest]


	phony "clean" $ do
		putQuiet "Cleaning files"
		removeFilesAfter "source" ["//*.o"]
		removeFilesAfter (sysroot </> prefix </> "lib" </> "flaxlibs") ["//*.flx"]


	compiledTest %> \out -> do
		need [finalOutput]
		alwaysRerun

		Exit code <- cmd Shell finalOutput [flaxcNormFlags] testSource

		cmd Shell (if code == ExitSuccess then [compiledTest] else ["echo Test failed"])



	"//*.flx" %> \out -> do
		let fnp = "libs" </> (takeFileName out)
		need [fnp]

		let ut = (sysroot </> prefix </> "lib" </> "flaxLibs/")
		quietly $ cmd Shell "cp" [fnp] [ut]


	phony "copyLibraries" $ do
		--- copy the libs to the prefix.
		--- remove the old ones first
		liftIO (removeFiles (sysroot </> prefix </> "lib" </> "flaxlibs") ["//*.flx"])

		() <- quietly $ cmd Shell "mkdir" "-p" (sysroot </> prefix </> "lib" </> "flaxlibs")
		quietly $ cmd Shell "cp" ("-R") ("libs/*") (sysroot </> prefix </> "lib" </> "flaxlibs/")


	finalOutput %> \out -> do
		need ["copyLibraries"]

		cs <- getDirectoryFiles "" ["source//*.cpp", "source//*.c"]
		let os = [c ++ ".o" | c <- cs]
		need os

		maybelconf <- getEnvWithDefault llvmConfig "LLVM_CONFIG"
		let lconf = maybelconf

		let llvmConfigInvoke = "`" ++ lconf ++ " --cxxflags --ldflags --system-libs --libs core engine native linker bitwriter lto vectorize`"


		maybeCXX <- getEnvWithDefault "clang++" "CXX"
		let cxx = maybeCXX

		() <- cmd Shell cxx "-g -o" [out] os [llvmConfigInvoke] -- " -fsanitize=address"
		putQuiet ("\x1b[0m" ++ "# built " ++ out)


	"source//*.cpp.o" %> \out -> do
		let c = dropExtension out
		let m = out ++ ".m"

		maybelconf <- getEnvWithDefault llvmConfig "LLVM_CONFIG"
		let lconf = maybelconf

		let cxxFlags = "-std=c++1z -O0 -g -Wall -Weverything " ++ disableWarn ++ " -frtti -fexceptions -fno-omit-frame-pointer -I`" ++ lconf ++ " --includedir` -Isource/include" ++ " -Xclang -fcolor-diagnostics" -- " -fsanitize=address"

		maybeCXX <- getEnvWithDefault "clang++" "CXX"
		let cxx = maybeCXX

		progress <- getProgress
		putQuiet ("\x1b[0m" ++ "# compiling " ++ c)

		() <- cmd Shell cxx "-c" [c] [cxxFlags] "-o" [out] "-MMD -MF" [m]

		needMakefileDependencies m



	"source//*.c.o" %> \out -> do
		let c = dropExtension out
		let m = out ++ ".m"

		maybelconf <- getEnvWithDefault llvmConfig "LLVM_CONFIG"
		let lconf = maybelconf

		let ccFlags = "-std=c11 -O0 -g -Wall " ++ disableWarn ++ " -fno-omit-frame-pointer -I`" ++ lconf ++ " --includedir` -Isource/include -Isource/utf8rewind/include/utf8rewind" ++ " -Xclang -fcolor-diagnostics -Wno-overlength-strings -Wno-missing-variable-declarations -Wno-unused-const-variable" -- " -fsanitize=address"

		maybeCC <- getEnvWithDefault "clang" "CC"
		let cc = maybeCC

		progress <- getProgress
		putQuiet ("\x1b[0m" ++ "# compiling " ++ c)

		() <- cmd Shell cc "-c" [c] [ccFlags] "-o" [out] "-MMD -MF" [m]

		needMakefileDependencies m





















