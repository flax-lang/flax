project('flax', version: '0.41.7-pre', default_options: [ 'warning_level=3' ])

add_languages(['c', 'cpp'])


the_compiler = meson.get_compiler('c')


if the_compiler.get_id() == 'msvc'
	add_project_arguments('/utf-8', language:  ['c', 'cpp'])
	add_project_arguments('/std:c++latest', language: 'cpp')
	add_project_arguments('/permissive-', language: 'cpp')
	add_project_arguments('-D_STDC_LIMIT_MACROS', language: 'cpp')
	add_project_arguments('-D_SCL_SECURE_NO_WARNINGS', language:  ['c', 'cpp'])
	add_project_arguments('-D_CRT_SECURE_NO_WARNINGS', language:  ['c', 'cpp'])
	add_project_arguments('-D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING', language: 'cpp')

	add_project_arguments('/W4', language: ['c', 'cpp'])

	# disable some useless warnings:
	add_project_arguments('/wd4100', language: ['c', 'cpp'])    # unused function parameter
	add_project_arguments('/wd4127', language: ['c', 'cpp'])    # if expression is constant (msvc apparently doesn't understand &&)
	add_project_arguments('/wd4456', language: ['c', 'cpp'])    # local variable shadowing
	add_project_arguments('/wd4457', language: ['c', 'cpp'])    # function parameter shadowing
	add_project_arguments('/wd4458', language: ['c', 'cpp'])    # class member shadowing

	# enable some useful warnings (set them to fire at level 1):
	add_project_arguments('/w14062', language: ['c', 'cpp'])    # enum value not handled in switch-case
	add_project_arguments('/w14263', language: ['c', 'cpp'])    # member method hides base method
	add_project_arguments('/w14265', language: ['c', 'cpp'])    # non-virtual destructor when class has virtual methods
	add_project_arguments('/w14548', language: ['c', 'cpp'])    # first part of comma-expression has no side-effects

	# until meson fixes the PCH issue on MSVC (https://github.com/mesonbuild/meson/issues/5648)
	# we must force include precompile.h to get the STL types!
	add_project_arguments('/FIsource/include/precompile.h', language: 'cpp')


	add_project_link_arguments('/ignore:4099', language: ['c', 'cpp'])
	add_project_link_arguments('/machine:X64', language:  ['c', 'cpp'])

	add_project_link_arguments('/opt:noref', language:  ['c', 'cpp'])
	add_project_link_arguments('/incremental', language:  ['c', 'cpp'])

	add_project_link_arguments('/nodefaultlib:libcmt.lib', language:  ['c', 'cpp'])
	add_project_link_arguments('/nodefaultlib:libcmtd.lib', language:  ['c', 'cpp'])

	if get_option('buildtype') == 'debug'
		libKind = 'Debug'
	else
		libKind = 'Release'
	endif

	# ok, so because meson is dumb, and we want to allow configuring the locations *without* editing this file
	# eg. in CI environments, we run an external command that's just an echo of the environment variable, and
	# capture the output to use. all because it's apparently "a bad idea" to allow people to read env vars...

	envname_mpir    = '%MPIR_ROOT_DIR%'
	envname_mpfr    = '%MPFR_ROOT_DIR%'
	envname_llvm    = '%LLVM_ROOT_DIR%'
	envname_libffi  = '%LIBFFI_ROOT_DIR%'

	llvm_version   = '9.0.0'

	mpir_root_dir = run_command('cmd.exe', '/C', 'echo', envname_mpir).stdout().strip()
	if mpir_root_dir == envname_mpir
		mpir_root_dir = 'D:/Projects/lib/mpir'
	endif

	mpfr_root_dir = run_command('cmd.exe', '/C', 'echo', envname_mpfr).stdout().strip()
	if mpfr_root_dir == envname_mpfr
		mpfr_root_dir = 'D:/Projects/lib/mpfr'
	endif

	llvm_root_dir = run_command('cmd.exe', '/C', 'echo', envname_llvm).stdout().strip()
	if llvm_root_dir == envname_llvm
		llvm_root_dir = 'D:/Projects/lib/llvm/' + llvm_version
	endif

	libffi_root_dir = run_command('cmd.exe', '/C', 'echo', envname_libffi).stdout().strip()
	if libffi_root_dir == envname_libffi
		libffi_root_dir = 'D:/Projects/lib/libffi'
	endif

	message('mpir_root: ' + mpir_root_dir)
	message('mpfr_root: ' + mpfr_root_dir)
	message('llvm_root: ' + llvm_root_dir)
	message('libffi_root: ' + libffi_root_dir)

	mpir_hdr_dir    = mpir_root_dir   + '/' + libKind + '/include/'
	mpfr_hdr_dir    = mpfr_root_dir   + '/' + libKind + '/include/'
	llvm_hdr_dir    = llvm_root_dir   + '/' + libKind + '/include/'
	libffi_hdr_dir  = libffi_root_dir + '/' + libKind + '/include/'

	mpir_lib_dir    = mpir_root_dir   + '/' + libKind + '/lib/'
	mpfr_lib_dir    = mpfr_root_dir   + '/' + libKind + '/lib/'
	llvm_lib_dir    = llvm_root_dir   + '/' + libKind + '/lib/'
	libffi_lib_dir  = libffi_root_dir + '/' + libKind + '/lib/'

	legacy_stdio_dep = declare_dependency(dependencies: the_compiler.find_library('legacy_stdio_definitions'))

	mpir_dep = declare_dependency(version: '3.0.0', include_directories: include_directories(mpir_hdr_dir),
		dependencies: the_compiler.find_library('mpir', dirs: mpir_lib_dir))

	mpfr_dep = declare_dependency(version: '4.0.0', include_directories: include_directories(mpfr_hdr_dir),
		dependencies: the_compiler.find_library('mpfr', dirs: mpfr_lib_dir))

	libffi_dep = declare_dependency(version: '3.2.1', include_directories: include_directories(libffi_hdr_dir),
		dependencies: the_compiler.find_library('libffi', dirs: libffi_lib_dir))

	llvm_dep = declare_dependency(version: llvm_version, include_directories: include_directories(llvm_hdr_dir),
		dependencies: [ mpfr_dep, mpir_dep,
			the_compiler.find_library('LLVMMC', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMipo', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMCore', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMMCJIT', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMOrcJIT', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMLinker', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMTarget', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMObject', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMPasses', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMX86Info', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMCodegen', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMX86Desc', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMSupport', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMRemarks', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMMCParser', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMX86Utils', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMAnalysis', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMTablegen', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMDemangle', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMVectorize', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMBitReader', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMLibDriver', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMBitWriter', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMAsmPrinter', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMGlobalISel', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMScalarOpts', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMX86CodeGen', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMProfileData', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMInstCombine', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMRuntimeDyld', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMSelectionDAG', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMBinaryFormat', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMX86AsmParser', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMDebugInfoPDB', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMDebugInfoDWARF', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMTransformUtils', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMMCDisassembler', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMExecutionEngine', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMBitstreamReader', dirs: llvm_lib_dir),
			the_compiler.find_library('LLVMDebugInfoCodeView', dirs: llvm_lib_dir),
		]
	)

	all_deps = [ legacy_stdio_dep, mpir_dep, mpfr_dep, libffi_dep, llvm_dep ]

else
	add_project_arguments('-std=c11', language: 'c')
	add_project_arguments('-std=c++17', language: 'cpp')
	# add_project_arguments('-include', 'source/include/precompile.h', language: 'cpp')

	add_project_arguments('-Wall', '-Wno-unused-parameter', '-Wno-sign-conversion', '-Wno-padded', '-Wno-conversion',
		'-Wno-shadow', '-Wno-missing-noreturn', '-Wno-unused-macros', '-Wno-switch-enum', '-Wno-deprecated', '-Wno-format-nonliteral',
		'-Wno-trigraphs', '-Wno-unused-const-variable', '-Wno-deprecated-declarations', '-Wno-unused-lambda-capture',
		'-Wno-unused-variable',
		language:'cpp')

	# on unix, we use GMP instead of MPIR.
	mpfr_dep = dependency('mpfr', version: '>= 4.0.0')
	libffi_dep = dependency('libffi', version: '>= 3.2.1')
	llvm_dep = dependency('llvm', static: true, version: '9.0.0', modules: [
		'core', 'engine', 'native', 'linker', 'bitwriter',
		'lto', 'vectorize', 'all-targets', 'object', 'orcjit'
	])

	all_deps = [ mpfr_dep, libffi_dep, llvm_dep ]

endif


source_files = files([
	'source/main.cpp',

	'source/misc/mpool.cpp',
	'source/misc/allocator.cpp',
	'source/misc/identifier.cpp',
	'source/misc/destructors.cpp',

	'source/repl/driver.cpp',
	'source/repl/execute.cpp',

	'source/frontend/pts.cpp',
	'source/frontend/file.cpp',
	'source/frontend/lexer.cpp',
	'source/frontend/errors.cpp',
	'source/frontend/import.cpp',
	'source/frontend/arguments.cpp',
	'source/frontend/collector.cpp',
	'source/frontend/dependencies.cpp',

	'source/platform/compiler.cpp',
	'source/platform/platform.cpp',
	'source/platform/msvcfinder.cpp',

	'source/frontend/parser/misc.cpp',
	'source/frontend/parser/expr.cpp',
	'source/frontend/parser/type.cpp',
	'source/frontend/parser/literal.cpp',
	'source/frontend/parser/variable.cpp',
	'source/frontend/parser/function.cpp',
	'source/frontend/parser/toplevel.cpp',
	'source/frontend/parser/operators.cpp',
	'source/frontend/parser/controlflow.cpp',

	'source/backend/backend.cpp',
	'source/backend/x64AsmBackend.cpp',

	'source/backend/llvm/jit.cpp',
	'source/backend/llvm/linker.cpp',
	'source/backend/llvm/translator.cpp',

	'source/backend/interp/driver.cpp',

	'source/typecheck/misc.cpp',
	'source/typecheck/call.cpp',
	'source/typecheck/type.cpp',
	'source/typecheck/using.cpp',
	'source/typecheck/slice.cpp',
	'source/typecheck/defer.cpp',
	'source/typecheck/loops.cpp',
	'source/typecheck/dotop.cpp',
	'source/typecheck/enums.cpp',
	'source/typecheck/alloc.cpp',
	'source/typecheck/assign.cpp',
	'source/typecheck/ranges.cpp',
	'source/typecheck/sizeof.cpp',
	'source/typecheck/unions.cpp',
	'source/typecheck/traits.cpp',
	'source/typecheck/special.cpp',
	'source/typecheck/structs.cpp',
	'source/typecheck/classes.cpp',
	'source/typecheck/function.cpp',
	'source/typecheck/variable.cpp',
	'source/typecheck/literals.cpp',
	'source/typecheck/toplevel.cpp',
	'source/typecheck/subscript.cpp',
	'source/typecheck/operators.cpp',
	'source/typecheck/arithmetic.cpp',
	'source/typecheck/directives.cpp',
	'source/typecheck/destructure.cpp',
	'source/typecheck/controlflow.cpp',
	'source/typecheck/typecheckstate.cpp',

	'source/typecheck/polymorph/misc.cpp',
	'source/typecheck/polymorph/driver.cpp',
	'source/typecheck/polymorph/solver.cpp',
	'source/typecheck/polymorph/transforms.cpp',
	'source/typecheck/polymorph/instantiator.cpp',

	'source/typecheck/resolver/misc.cpp',
	'source/typecheck/resolver/driver.cpp',
	'source/typecheck/resolver/resolver.cpp',

	'source/codegen/raii.cpp',
	'source/codegen/misc.cpp',
	'source/codegen/call.cpp',
	'source/codegen/loops.cpp',
	'source/codegen/slice.cpp',
	'source/codegen/alloc.cpp',
	'source/codegen/enums.cpp',
	'source/codegen/dotop.cpp',
	'source/codegen/ranges.cpp',
	'source/codegen/sizeof.cpp',
	'source/codegen/assign.cpp',
	'source/codegen/unions.cpp',
	'source/codegen/traits.cpp',
	'source/codegen/structs.cpp',
	'source/codegen/classes.cpp',
	'source/codegen/logical.cpp',
	'source/codegen/builtin.cpp',
	'source/codegen/variable.cpp',
	'source/codegen/function.cpp',
	'source/codegen/toplevel.cpp',
	'source/codegen/literals.cpp',
	'source/codegen/operators.cpp',
	'source/codegen/subscript.cpp',
	'source/codegen/arithmetic.cpp',
	'source/codegen/directives.cpp',
	'source/codegen/destructure.cpp',
	'source/codegen/refcounting.cpp',
	'source/codegen/controlflow.cpp',
	'source/codegen/constructor.cpp',
	'source/codegen/autocasting.cpp',
	'source/codegen/codegenstate.cpp',

	'source/codegen/glue/any.cpp',
	'source/codegen/glue/misc.cpp',
	'source/codegen/glue/arrays.cpp',
	'source/codegen/glue/strings.cpp',
	'source/codegen/glue/saa_common.cpp',

	'source/fir/interp/wrappers.cpp',
	'source/fir/interp/compiler.cpp',
	'source/fir/interp/interpreter.cpp',

	'source/fir/ConstantValue.cpp',
	'source/fir/GlobalValue.cpp',
	'source/fir/Instruction.cpp',
	'source/fir/IRBuilder.cpp',
	'source/fir/Function.cpp',
	'source/fir/IRBlock.cpp',
	'source/fir/Module.cpp',
	'source/fir/Value.cpp',

	'source/fir/Types/DynamicArrayType.cpp',
	'source/fir/Types/ArraySliceType.cpp',
	'source/fir/Types/PrimitiveType.cpp',
	'source/fir/Types/FunctionType.cpp',
	'source/fir/Types/RawUnionType.cpp',
	'source/fir/Types/PointerType.cpp',
	'source/fir/Types/SingleTypes.cpp',
	'source/fir/Types/OpaqueType.cpp',
	'source/fir/Types/StructType.cpp',
	'source/fir/Types/TypeUtils.cpp',
	'source/fir/Types/ArrayType.cpp',
	'source/fir/Types/TraitType.cpp',
	'source/fir/Types/ClassType.cpp',
	'source/fir/Types/TupleType.cpp',
	'source/fir/Types/UnionType.cpp',
	'source/fir/Types/EnumType.cpp',
	'source/fir/Types/Type.cpp'
])

utf8rewind_dep = declare_dependency(include_directories: include_directories('external/utf8rewind/include'),
	sources: [
				'external/utf8rewind/source/utf8rewind.c',
				'external/utf8rewind/source/unicodedatabase.c',

				'external/utf8rewind/source/internal/seeking.c',
				'external/utf8rewind/source/internal/database.c',
				'external/utf8rewind/source/internal/streaming.c',
				'external/utf8rewind/source/internal/codepoint.c',
				'external/utf8rewind/source/internal/composition.c',
				'external/utf8rewind/source/internal/casemapping.c',
				'external/utf8rewind/source/internal/decomposition.c'
			]
)

tinyproclib_dep = declare_dependency(include_directories: include_directories('external/tinyprocesslib'),
	sources: [
				'external/tinyprocesslib/process.cpp',
				'external/tinyprocesslib/process_os.cpp'
			]
)



executable('flaxc', source_files,
	include_directories: include_directories([ 'source/include', 'external' ]),
	dependencies: all_deps + [ tinyproclib_dep, utf8rewind_dep ],
	# cpp_pch: 'source/include/precompile.h'
)









