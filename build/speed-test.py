#!/usr/bin/env python3

import re
import os
import subprocess
import generate_test

import prettytable
from prettytable import PrettyTable

tab = PrettyTable([ "reps", "firs", "total", "lexer", "parser", "typecheck", "codegen" ])
tab.set_style(prettytable.PLAIN_COLUMNS)
tab.align = "l"


counts = range(32, 1025, 32)
# counts = range(32, 64, 32)

plots = open("build/plots.txt", "wt")

for i in counts:
	generate_test.gen_test(i)
	if os.name == "nt":
		flaxc_path = "build/meson-rel/flaxc.exe"
	else:
		flaxc_path = "build/sysroot/usr/local/bin/flaxc"

	output = subprocess.run([ flaxc_path, "-sysroot", "build/sysroot", "-run", "-backend", "none", "build/massive.flx" ],
		capture_output = True, text = True).stdout

	# rex = re.findall(r"compile took (\d+\.\d+) \(lexer: (\d+\.\d+), parser: (\d+\.\d+), typecheck: (\d+\.\d+), codegen (\d+\.\d+)\) ms(.+)",
		# output)

	rex = re.compile(r"compile took (\d+\.\d+) \(lexer: (\d+\.\d+), parser: (\d+\.\d+), typecheck: (\d+\.\d+), codegen: (\d+\.\d+)\) ms.*\n(\d+) FIR values generated")

	m = rex.search(output)

	t_compile       = m.group(1)
	t_lexer         = m.group(2)
	t_parser        = m.group(3)
	t_typecheck     = m.group(4)
	t_codegen       = m.group(5)
	n_fvals         = m.group(6)

	n_lines = 0
	with open("build/massive.flx", "r") as f:
		for k, l in enumerate(f):
			pass
		n_lines = k + 1
		f.close()

	tab.add_row([ n_lines, n_fvals, t_compile, t_lexer, t_parser, t_typecheck, t_codegen ])
	print(n_lines, n_fvals, t_compile, t_lexer, t_parser, t_typecheck, t_codegen)


plots.write(str(tab))
plots.close()




