// compiler.h
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include <string>
#include <vector>
#pragma once

namespace Compiler
{
	Ast::Root* compileFile(std::string filename, std::vector<std::string>& filenames, Codegen::CodegenInstance* cgi);
}
