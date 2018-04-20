// operators.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"

static bool isBuiltinType(fir::Type* ty)
{
	return (ty->isConstantNumberType()
		|| ty->isDynamicArrayType()
		|| ty->isArraySliceType()
		|| ty->isPrimitiveType()
		|| ty->isFunctionType()
		|| ty->isPointerType()
		|| ty->isStringType()
		|| ty->isRangeType()
		|| ty->isArrayType()
		|| ty->isVoidType()
		|| ty->isNullType()
		|| ty->isCharType()
		|| ty->isBoolType());
}

static bool isBuiltinOperator(std::string op)
{
	return (op == Operator::Plus ||
			op == Operator::Minus ||
			op == Operator::Multiply ||
			op == Operator::Divide ||
			op == Operator::Modulo ||
			op == Operator::UnaryPlus ||
			op == Operator::UnaryMinus ||
			op == Operator::PointerDeref ||
			op == Operator::AddressOf ||
			op == Operator::BitwiseNot ||
			op == Operator::BitwiseAnd ||
			op == Operator::BitwiseOr ||
			op == Operator::BitwiseXor ||
			op == Operator::BitwiseShiftLeft ||
			op == Operator::BitwiseShiftRight ||
			op == Operator::LogicalNot ||
			op == Operator::LogicalAnd ||
			op == Operator::LogicalOr ||
			op == Operator::CompareEQ ||
			op == Operator::CompareNEQ ||
			op == Operator::CompareLT ||
			op == Operator::CompareLEQ ||
			op == Operator::CompareGT ||
			op == Operator::CompareGEQ ||
			op == Operator::Assign ||
			op == Operator::PlusEquals ||
			op == Operator::MinusEquals ||
			op == Operator::MultiplyEquals ||
			op == Operator::DivideEquals ||
			op == Operator::ModuloEquals ||
			op == Operator::BitwiseShiftLeftEquals ||
			op == Operator::BitwiseShiftRightEquals ||
			op == Operator::BitwiseAndEquals ||
			op == Operator::BitwiseOrEquals ||
			op == "cast" || op == ".");
}



TCResult ast::OperatorOverloadDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	if(this->kind == Kind::Invalid)
		error(this, "Invalid operator kind; must be one of 'infix', 'postfix', or 'prefix'");

	if(fs->isInStructBody())
		error(this, "Operator overloads cannot be methods of a type.");

	this->generateDeclaration(fs, infer, { });

	// call the superclass method.
	return this->ast::FuncDefn::typecheck(fs, infer, gmaps);
}

TCResult ast::OperatorOverloadDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	// there's nothing different.
	auto defn = dcast(sst::FunctionDefn, this->ast::FuncDefn::generateDeclaration(fs, infer, gmaps).defn());
	iceAssert(defn);


	// ok, do our checks on the defn instead.
	auto ft = defn->type->toFunctionType();

	if(this->kind == Kind::Infix)
	{
		if(ft->getArgumentTypes().size() != 2)
		{
			error(this, "Operator overload for binary operator '%s' must have exactly 2 parameters, but %d %s found",
				this->symbol, ft->getArgumentTypes().size(), ft->getArgumentTypes().size() == 1 ? "was" : "were");
		}
		else if(!Operator::isAssignment(this->symbol) && isBuiltinType(ft->getArgumentN(0)) && isBuiltinType(ft->getArgumentN(1))
			 && isBuiltinOperator(this->symbol))
		{
			exitless_error(this, "Binary operator overload (for operator '%s') cannot take two builtin types as arguments (have '%s' and '%s')",
				this->symbol, ft->getArgumentN(0), ft->getArgumentN(1));

			info("At least one of the parameters must be a user-defined type");
			doTheExit();
		}
	}
	else if(this->kind == Kind::Postfix || this->kind == Kind::Prefix)
	{
		if(ft->getArgumentTypes().size() != 1)
		{
			error(this, "Operator overload for unary operator '%s' must have exactly 1 parameter, but %d %s found",
				this->symbol, ft->getArgumentTypes().size(), ft->getArgumentTypes().size() == 1 ? "was" : "were");
		}
		else if(isBuiltinType(ft->getArgumentN(0)) && isBuiltinOperator(this->symbol))
		{
			error(defn->arguments[0], "Unary operator '%s' cannot be overloaded for the builtin type '%s'",
				this->symbol, ft->getArgumentN(0));
		}
	}

	// ok, further checks.
	if(Operator::isAssignment(this->symbol))
	{
		if(!ft->getReturnType()->isVoidType())
		{
			error(this, "Operator overload for assignment operators (have '%s') must return void, but a return type of '%s' was found",
				this->symbol, ft->getReturnType());
		}
		else if(!ft->getArgumentN(0)->isPointerType())
		{
			error(defn->arguments[0], "Operator overload for assignment operator '%s' must take a pointer to the type as the first parameter, found '%s'",
				this->symbol, ft->getArgumentN(0));
		}
		else if(isBuiltinType(ft->getArgumentN(0)->getPointerElementType()))
		{
			error(defn->arguments[0], "Assignment operator '%s' cannot be overloaded for the builtin type '%s'",
				this->symbol, ft->getArgumentN(0));
		}
	}

	// before we add, check for duplication.
	auto thelist = (this->kind == Kind::Infix ? &fs->stree->infixOperatorOverloads : (this->kind == Kind::Prefix
		? &fs->stree->prefixOperatorOverloads : &fs->stree->postfixOperatorOverloads));

	for(auto it : (*thelist)[this->symbol])
	{
		if(fs->isDuplicateOverload(it->params, defn->params))
		{
			exitless_error(this, "Duplicate operator overload for '%s' taking identical arguments", this->symbol);
			info(it, "Previous definition was here:");

			doTheExit();
		}
	}

	// ok, we should be good now.
	(*thelist)[this->symbol].push_back(defn);
	this->genericVersions.push_back({ defn, fs->getCurrentGenericContextStack() });

	return TCResult(defn);
}

























