// codegenstate.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "platform.h"
#include "gluecode.h"
#include "typecheck.h"

namespace cgn
{
	void CodegenState::enterMethodBody(fir::Function* method, fir::Value* self)
	{
		this->methodSelfStack.push_back(self);

		auto ty = self->getType();
		iceAssert(ty->isClassType() || ty->isStructType());

		this->methodList[method] = ty;
	}

	void CodegenState::leaveMethodBody()
	{
		iceAssert(this->methodSelfStack.size() > 0);
		this->methodSelfStack.pop_back();
	}

	bool CodegenState::isInMethodBody()
	{
		return this->methodSelfStack.size() > 0 && this->functionStack.size() > 0
			&& this->methodList.find(this->functionStack.back()) != this->methodList.end();
	}

	fir::Value* CodegenState::getMethodSelf()
	{
		iceAssert(this->methodSelfStack.size() > 0);
		return this->methodSelfStack.back();
	}


	void CodegenState::enterSubscriptWithLength(fir::Value* len)
	{
		iceAssert(len->getType()->isIntegerType());
		this->subscriptArrayLengthStack.push_back(len);
	}

	fir::Value* CodegenState::getCurrentSubscriptArrayLength()
	{
		iceAssert(this->subscriptArrayLengthStack.size() > 0);
		return this->subscriptArrayLengthStack.back();
	}

	void CodegenState::leaveSubscript()
	{
		iceAssert(this->subscriptArrayLengthStack.size() > 0);
		this->subscriptArrayLengthStack.pop_back();
	}





	void CodegenState::enterFunction(fir::Function* fn)
	{
		this->functionStack.push_back(fn);
	}

	void CodegenState::leaveFunction()
	{
		if(this->functionStack.empty())
			error(this->loc(), "not a in function");

		this->functionStack.pop_back();
	}

	fir::Function* CodegenState::getCurrentFunction()
	{
		if(this->functionStack.empty())
			error(this->loc(), "not a in function");

		return this->functionStack.back();
	}



	ControlFlowPoint CodegenState::getCurrentCFPoint()
	{
		return this->breakingPointStack.back();
	}

	void CodegenState::enterBreakableBody(const ControlFlowPoint& cfp)
	{
		// only the block needs to exist.
		iceAssert(cfp.block);
		this->breakingPointStack.push_back(cfp);
	}

	ControlFlowPoint CodegenState::leaveBreakableBody()
	{
		iceAssert(this->breakingPointStack.size() > 0);

		auto ret = this->breakingPointStack.back();
		this->breakingPointStack.pop_back();

		return ret;
	}


	BlockPoint CodegenState::getCurrentBlockPoint()
	{
		return this->blockPointStack.back();
	}

	void CodegenState::enterBlock(const BlockPoint& bp)
	{
		iceAssert(bp.block);
		this->blockPointStack.push_back(bp);
	}

	void CodegenState::leaveBlock()
	{
		iceAssert(this->blockPointStack.size() > 0);
		this->blockPointStack.pop_back();
	}



	void CodegenState::pushLoc(const Location& l)
	{
		this->locationStack.push_back(l);
	}

	void CodegenState::pushLoc(sst::Stmt* stmt)
	{
		this->pushLoc(stmt->loc);
	}

	void CodegenState::popLoc()
	{
		iceAssert(this->locationStack.size() > 0);
		this->locationStack.pop_back();
	}

	Location CodegenState::loc()
	{
		iceAssert(this->locationStack.size() > 0);
		return this->locationStack.back();
	}


	void CodegenState::createWhileLoop(const std::function<void (fir::IRBlock*, fir::IRBlock*)>& docheck, const std::function<void ()>& dobody)
	{
		fir::IRBlock* check = this->irb.addNewBlockInFunction("check", this->irb.getCurrentFunction());
		fir::IRBlock* body = this->irb.addNewBlockInFunction("body", this->irb.getCurrentFunction());
		fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", this->irb.getCurrentFunction());

		this->irb.UnCondBranch(check);
		this->irb.setCurrentBlock(check);

		//* we expect this to do its own branching.
		docheck(body, merge);

		this->irb.setCurrentBlock(body);
		dobody();

		//* but not the body.
		this->irb.UnCondBranch(check);

		//* back to regularly scheduled programming
		this->irb.setCurrentBlock(merge);
	}



	fir::Value* CodegenState::getDefaultValue(fir::Type* type)
	{
		fir::Value* ret = 0;
		if(type->isStringType())
		{
			fir::Value* arr = this->irb.CreateValue(type);

			arr = this->irb.SetSAAData(arr, this->irb.PointerTypeCast(this->irb.GetArraySliceData(fir::ConstantCharSlice::get("")),
				fir::Type::getMutInt8Ptr()));
			arr = this->irb.SetSAALength(arr, fir::ConstantInt::getNative(0));
			arr = this->irb.SetSAACapacity(arr, fir::ConstantInt::getNative(0));
			arr = this->irb.SetSAARefCountPointer(arr, fir::ConstantValue::getZeroValue(fir::Type::getNativeWord()->getPointerTo()));

			ret = arr;
		}
		else if(type->isDynamicArrayType())
		{
			fir::Value* arr = this->irb.CreateValue(type);

			arr = this->irb.SetSAAData(arr, fir::ConstantValue::getZeroValue(type->getArrayElementType()->getMutablePointerTo()));
			arr = this->irb.SetSAALength(arr, fir::ConstantInt::getNative(0));
			arr = this->irb.SetSAACapacity(arr, fir::ConstantInt::getNative(0));
			arr = this->irb.SetSAARefCountPointer(arr, fir::ConstantValue::getZeroValue(fir::Type::getNativeWord()->getPointerTo()));

			ret = arr;
		}
		else if(type->isArraySliceType())
		{
			fir::Value* arr = this->irb.CreateValue(type);
			arr = this->irb.SetArraySliceData(arr, fir::ConstantValue::getZeroValue(type->getArrayElementType()->getPointerTo()));
			arr = this->irb.SetArraySliceLength(arr, fir::ConstantInt::getNative(0));

			ret = arr;
		}
		else if(type->isClassType())
		{
			// TODO
			//! use constructClassWithArguments!!!

			auto clsdef = dcast(sst::ClassDefn, this->typeDefnMap[type]);
			iceAssert(clsdef);

			clsdef->codegen(this);

			// first need to check if we have any initialisers with 0 parameters.
			auto cls = type->toClassType();

			sst::FunctionDefn* ifn = 0;
			for(auto init : clsdef->initialisers)
			{
				//* note: count == 1 because of 'self'
				if(init->arguments.size() == 1)
				{
					ifn = init;
					break;
				}
			}

			if(ifn == 0)
			{
				SimpleError::make(this->loc(), "class '%s' cannot be automatically initialised as it does not have a constructor taking 0 arguments",
					cls->getTypeName())->append(SimpleError::make(MsgType::Note, clsdef->loc, "class '%s' was defined here:", clsdef->id.name))
					->postAndQuit();
			}

			ret = this->constructClassWithArguments(cls, ifn, { });
		}
		else
		{
			ret = fir::ConstantValue::getZeroValue(type);
		}

		if(fir::isRefCountedType(type))
			this->addRefCountedValue(ret);

		ret->setKind(fir::Value::Kind::prvalue);
		return ret;
	}

	void CodegenState::pushIRDebugIndentation()
	{
		this->_debugIRIndent++;
	}

	void CodegenState::printIRDebugMessage(const std::string& msg, const std::vector<fir::Value*>& vals)
	{
		fir::Value* tmpstr = this->module->createGlobalString(std::string(this->_debugIRIndent * 4, ' ') + msg + "\n");
		this->irb.Call(this->getOrDeclareLibCFunction("printf"), tmpstr + vals);
	}

	void CodegenState::popIRDebugIndentation()
	{
		this->_debugIRIndent--;
	}





	fir::Function* CodegenState::getOrDeclareLibCFunction(std::string name)
	{
		if(name == ALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(ALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getNativeWord() }, fir::Type::getMutInt8Ptr()), fir::LinkageType::External);
		}
		else if(name == FREE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(FREE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getMutInt8Ptr() }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == REALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(REALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getNativeWord() }, fir::Type::getMutInt8Ptr()),
					fir::LinkageType::External);
		}
		else if(name == CRT_FDOPEN)
		{
			return this->module->getOrCreateFunction(Identifier(CRT_FDOPEN, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()), fir::LinkageType::External);
		}
		else if(name == "printf")
		{
			return this->module->getOrCreateFunction(Identifier("printf", IdKind::Name),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getInt8Ptr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else if(name == "abort")
		{
			return this->module->getOrCreateFunction(Identifier("abort", IdKind::Name),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == "exit")
		{
			return this->module->getOrCreateFunction(Identifier("exit", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt32() }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == "strlen")
		{
			return this->module->getOrCreateFunction(Identifier("strlen", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getNativeWord()), fir::LinkageType::External);
		}
		else if(name == "fprintf")
		{
			return this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else if(name == "fflush")
		{
			return this->module->getOrCreateFunction(Identifier("fflush", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getVoidPtr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else
		{
			error("enotsup: %s", name);
		}
	}




	bool CodegenState::isWithinGlobalInitFunction()
	{
		return this->isInsideGlobalInitFunc;
	}

	fir::IRBlock* CodegenState::enterGlobalInitFunction()
	{
		auto ret = this->irb.getCurrentBlock();

		if(!this->globalInitFunc)
		{
			fir::Function* func = this->module->getOrCreateFunction(util::obfuscateIdentifier(strs::names::GLOBAL_INIT_FUNCTION),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

			this->globalInitFunc = func;
		}

		iceAssert(this->globalInitFunc);
		this->irb.setCurrentBlock(this->globalInitFunc->getBlockList().back());

		this->isInsideGlobalInitFunc = true;
		return ret;
	}

	void CodegenState::leaveGlobalInitFunction(fir::IRBlock* restore)
	{
		this->irb.setCurrentBlock(restore);
		this->isInsideGlobalInitFunc = false;
	}

	void CodegenState::finishGlobalInitFunction()
	{
		auto r = this->enterGlobalInitFunction();

		// ok, here's a thing -- if the current block doesn't end with a return, then we insert it.
		// this lets us keep calling finishGlobalInitFunction() without repercussions, even if we
		// already "finished" the constructor function. this workaround is used to make our life
		// simpler when doing compile-time execution.

		if(const auto& instrs = this->irb.getCurrentBlock()->getInstructions();
			instrs.empty() || instrs.back()->opKind != fir::OpKind::Value_Return)
		{
			this->irb.ReturnVoid();
		}

		this->leaveGlobalInitFunction(r);
	}
}














