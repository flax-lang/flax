// Slice.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"
#include "operators.h"
#include "runtimefuncs.h"

using namespace Ast;
using namespace Codegen;


Result_t ArraySlice::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Operators::OperatorMap::get().call(ArithmeticOp::Slice, cgi, this, { this->arr, this->start, this->end });
}

fir::Type* ArraySlice::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	fir::Type* t = this->arr->getType(cgi);
	if(t->isDynamicArrayType())
	{
		return fir::ArraySliceType::get(t->toDynamicArrayType()->getElementType());
	}
	else if(t->isArraySliceType())
	{
		return t;
	}
	else if(t->isArrayType())
	{
		return fir::ArraySliceType::get(t->toArrayType()->getElementType());
	}
	else if(t->isStringType())
	{
		// special case (as usual), slicing strings returns strings.
		return fir::Type::getStringType();
	}
	else
	{
		error(this, "Slicing operator on custom types ('%s') is not supported yet", t->str().c_str());

		// // todo: multiple subscripts
		// fir::Function* getter = Operators::getOperatorSubscriptGetter(cgi, this, t, { this, this->index });
		// if(!getter)
		// {
		// 	error(this, "Invalid subscript on type '%s', with index type '%s'", t->str().c_str(),
		// 		this->index->getType(cgi)->str().c_str());
		// }

		// return getter->getReturnType();
	}
}






namespace Operators
{
	static void _complainAboutSliceIndices(CodegenInstance* cgi, std::string fmt, fir::Value* complaintValue, Parser::Pin pos)
	{
		fir::Function* fprintfn = cgi->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
			fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
			fir::Type::getInt32()), fir::LinkageType::External);

		fir::Function* fdopenf = cgi->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
			fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
			fir::LinkageType::External);

		// basically:
		// void* stderr = fdopen(2, "w")
		// fprintf(stderr, "", bla bla)

		fir::ConstantValue* tmpstr = cgi->module->createGlobalString("w");
		fir::ConstantValue* fmtstr = cgi->module->createGlobalString(fmt);

		iceAssert(fmtstr);

		auto loc = fir::ConstantString::get(Parser::pinToString(pos));
		fir::Value* posstr = cgi->irb.CreateGetStringData(loc);
		fir::Value* err = cgi->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

		cgi->irb.CreateCall(fprintfn, { err, fmtstr, posstr, complaintValue });

		cgi->irb.CreateCall0(cgi->getOrDeclareLibCFunc("abort"));
		cgi->irb.CreateUnreachable();
	}




	Result_t operatorSlice(CodegenInstance* cgi, ArithmeticOp op, Expr* usr, std::vector<Expr*> args)
	{
		// slice that motherfucker up
		iceAssert(args.size() == 3);
		iceAssert(op == ArithmeticOp::Slice);

		// ok.
		fir::Value* lhs = 0; fir::Value* lhsptr = 0;
		std::tie(lhs, lhsptr) = args[0]->codegen(cgi);

		iceAssert(lhs);
		if(!lhsptr) lhsptr = cgi->irb.CreateImmutStackAlloc(lhs->getType(), lhs);

		// ok then
		fir::Type* lt = lhs->getType();

		Parser::Pin apos = (args[1] ? args[1]->pin : args[0]->pin);
		Parser::Pin bpos = (args[2] ? args[2]->pin : args[0]->pin);

		if(lt->isDynamicArrayType())
		{
			// make that shit happen
			fir::Value* beginIndex = 0;
			fir::Value* endIndex = 0;

			if(args[1])	beginIndex = args[1]->codegen(cgi).value;
			else		beginIndex = fir::ConstantInt::getInt64(0);

			if(args[2])	endIndex = args[2]->codegen(cgi).value;
			else		endIndex = cgi->irb.CreateGetDynamicArrayLength(lhsptr);


			if(!beginIndex->getType()->isIntegerType())
				error(args[1], "Expected integer type for array slice; got '%s'", beginIndex->getType()->str().c_str());

			if(!endIndex->getType()->isIntegerType())
				error(args[2], "Expected integer type for array slice; got '%s'", endIndex->getType()->str().c_str());


			beginIndex = cgi->autoCastType(fir::Type::getInt64(), beginIndex);
			endIndex = cgi->autoCastType(fir::Type::getInt64(), endIndex);

			fir::Value* length = cgi->irb.CreateSub(endIndex, beginIndex);

			// ok, make the slice
			fir::Type* slct = fir::ArraySliceType::get(lt->toDynamicArrayType()->getElementType());
			fir::Value* ai = cgi->irb.CreateStackAlloc(slct);

			// fir::Value* ptr = cgi->irb.CreateGetDynamicArrayData(lhsptr);

			// do a check
			auto neg_begin = cgi->irb.addNewBlockInFunction("neg_begin", cgi->irb.getCurrentFunction());
			auto neg_end = cgi->irb.addNewBlockInFunction("neg_end", cgi->irb.getCurrentFunction());
			auto neg_len = cgi->irb.addNewBlockInFunction("neg_len", cgi->irb.getCurrentFunction());
			auto check1 = cgi->irb.addNewBlockInFunction("check1", cgi->irb.getCurrentFunction());
			auto check2 = cgi->irb.addNewBlockInFunction("check2", cgi->irb.getCurrentFunction());
			auto merge = cgi->irb.addNewBlockInFunction("merge", cgi->irb.getCurrentFunction());

			{
				fir::Value* neg = cgi->irb.CreateICmpLT(beginIndex, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(neg, neg_begin, check1);
			}

			cgi->irb.setCurrentBlock(check1);
			{
				fir::Value* neg = cgi->irb.CreateICmpLT(endIndex, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(neg, neg_end, check2);
			}

			cgi->irb.setCurrentBlock(check2);
			{
				fir::Value* neg = cgi->irb.CreateICmpLT(length, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateCondBranch(neg, neg_len, merge);
			}


			cgi->irb.setCurrentBlock(neg_begin);
			_complainAboutSliceIndices(cgi, "%s: Start index for array slice was negative (%zd)\n", beginIndex, apos);

			cgi->irb.setCurrentBlock(neg_end);
			_complainAboutSliceIndices(cgi, "%s: Ending index for array slice was negative (%zd)\n", endIndex, bpos);

			cgi->irb.setCurrentBlock(neg_len);
			_complainAboutSliceIndices(cgi, "%s: Length for array slice was negative (%zd)\n", length, bpos);


			cgi->irb.setCurrentBlock(merge);

			// bounds check.
			{
				// endindex is non-inclusive, so do the len vs len check
				fir::Function* checkf = RuntimeFuncs::Array::getBoundsCheckFunction(cgi, true);
				iceAssert(checkf);

				fir::Value* max = cgi->irb.CreateGetDynamicArrayLength(lhsptr);
				cgi->irb.CreateCall3(checkf, max, endIndex, fir::ConstantString::get(Parser::pinToString(apos)));
			}


			// FINALLY.
			// increment ptr
			fir::Value* data = cgi->irb.CreateGetDynamicArrayData(lhsptr);
			fir::Value* newptr = cgi->irb.CreatePointerAdd(data, beginIndex);

			cgi->irb.CreateSetArraySliceData(ai, newptr);
			cgi->irb.CreateSetArraySliceLength(ai, length);

			// slices are rvalues
			return Result_t(cgi->irb.CreateLoad(ai), ai, ValueKind::RValue);
		}
		else if(lt->isArrayType())
		{
			error("enotsup");
		}
		else if(lt->isArraySliceType())
		{
			error("enotsup");
		}
		else if(lt->isStringType())
		{
			error("enotsup");
		}
		else
		{
			error("enotsup");
		}
	}
}














