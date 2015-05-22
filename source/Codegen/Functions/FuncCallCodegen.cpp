// FuncCallCodegen.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.


#include "ast.h"
#include "codegen.h"
#include "llvm_all.h"

using namespace Ast;
using namespace Codegen;


Result_t CodegenInstance::callTypeInitialiser(TypePair_t* tp, Expr* user, std::vector<llvm::Value*> args)
{
	iceAssert(tp);
	llvm::Value* ai = this->allocateInstanceInBlock(tp->first, "tmp");

	args.insert(args.begin(), ai);

	llvm::Function* initfunc = this->getStructInitialiser(user, tp, args);

	this->builder.CreateCall(initfunc, args);
	llvm::Value* val = this->builder.CreateLoad(ai);

	return Result_t(val, ai);
}

Result_t FuncCall::codegen(CodegenInstance* cgi, llvm::Value* lhsPtr, llvm::Value* rhs)
{
	// always try the type first.
	if(cgi->getType(this->name) != nullptr)
	{
		std::vector<llvm::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(cgi->getType(this->name), this, args);
	}
	else if(cgi->getType(cgi->mangleRawNamespace(this->name)) != nullptr)
	{
		std::vector<llvm::Value*> args;
		for(Expr* e : this->params)
			args.push_back(e->codegen(cgi).result.first);

		return cgi->callTypeInitialiser(cgi->getType(cgi->mangleRawNamespace(this->name)), this, args);
	}

	std::vector<llvm::Value*> args;
	std::vector<llvm::Value*> argPtrs;

	FuncPair_t* fp = cgi->getDeclaredFunc(this);
	if(!fp)
	{
		// print a better error message.
		std::vector<std::string> argtypes;
		for(auto a : this->params)
			argtypes.push_back(cgi->getReadableType(a).c_str());

		std::string argstr;
		for(auto s : argtypes)
			argstr += ", " + s;

		argstr = argstr.substr(2);

		std::string candidates;
		for(auto fs : cgi->funcStack)
		{
			for(auto f : fs)
			{
				if(f.second.second && f.second.second->name == this->name)
					candidates += cgi->printAst(f.second.second) + "\n";
			}
		}

		error(cgi, this, "No such function '%s' taking parameters (%s)\nPossible candidates:\n%s",
			this->name.c_str(), argstr.c_str(), candidates.c_str());
	}

	llvm::Function* target = fp->first;
	bool checkVarArg = target->isVarArg();


	if((target->arg_size() != this->params.size() && !checkVarArg) || (checkVarArg && target->arg_size() > 0 && this->params.size() == 0))
	{
		error(cgi, this, "Expected %ld arguments, but got %ld arguments instead", target->arg_size(), this->params.size());
	}


	int argNum = 0;
	for(Expr* e : this->params)
	{
		ValPtr_t res = e->codegen(cgi).result;
		llvm::Value* arg = res.first;

		if(arg == nullptr || arg->getType()->isVoidTy())
			GenError::nullValue(cgi, this, argNum);

		if(checkVarArg && arg->getType()->isStructTy() && arg->getType()->getStructName() != "String")
			warn(cgi, e, "Passing structs to vararg functions can have unexpected results.");

		if(target->isVarArg() && res.first->getType()->isStructTy() && res.first->getType()->getStructName() == "String")
		{
			// this function knows what to do.
			cgi->autoCastType(llvm::Type::getInt8PtrTy(cgi->getContext()), arg, res.second);
		}

		args.push_back(arg);
		argPtrs.push_back(res.second);
		argNum++;
	}




	auto arg_it = target->arg_begin();
	for(size_t i = 0; i < args.size() && arg_it != target->arg_end(); i++, arg_it++)
	{
		if(arg_it->getType() != args[i]->getType())
			cgi->autoCastType(arg_it, args[i], argPtrs[i]);

		if(arg_it->getType() != args[i]->getType())
		{
			error(cgi, this, "Argument %zu of function call is mismatched; expected '%s', got '%s'", i + 1,
				cgi->getReadableType(arg_it).c_str(), cgi->getReadableType(args[i]).c_str());
		}
	}

	return Result_t(cgi->builder.CreateCall(target, args), 0);
}
















