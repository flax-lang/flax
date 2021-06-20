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
		if(type->isArraySliceType())
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
			return this->module->getOrCreateFunction(fir::Name::of(ALLOCATE_MEMORY_FUNC),
				fir::FunctionType::get({ fir::Type::getNativeWord() }, fir::Type::getMutInt8Ptr()), fir::LinkageType::External);
		}
		else if(name == FREE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(fir::Name::of(FREE_MEMORY_FUNC),
				fir::FunctionType::get({ fir::Type::getMutInt8Ptr() }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == REALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(fir::Name::of(REALLOCATE_MEMORY_FUNC),
				fir::FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getNativeWord() }, fir::Type::getMutInt8Ptr()),
					fir::LinkageType::External);
		}
		else if(name == CRT_FDOPEN)
		{
			return this->module->getOrCreateFunction(fir::Name::of(CRT_FDOPEN),
				fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr()), fir::LinkageType::External);
		}
		else if(name == "printf")
		{
			return this->module->getOrCreateFunction(fir::Name::of("printf"),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getInt8Ptr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else if(name == "abort")
		{
			return this->module->getOrCreateFunction(fir::Name::of("abort"),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == "exit")
		{
			return this->module->getOrCreateFunction(fir::Name::of("exit"),
				fir::FunctionType::get({ fir::Type::getInt32() }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == "strlen")
		{
			return this->module->getOrCreateFunction(fir::Name::of("strlen"),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getNativeWord()), fir::LinkageType::External);
		}
		else if(name == "fprintf")
		{
			return this->module->getOrCreateFunction(fir::Name::of("fprintf"),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else if(name == "fflush")
		{
			return this->module->getOrCreateFunction(fir::Name::of("fflush"),
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

	fir::IRBlock* CodegenState::enterGlobalInitFunction(fir::GlobalValue* val)
	{
		if(this->isInsideGlobalInitFunc)
			error(this->loc(), "unsynchronised use of global init function!!! (entering when already inside)");

		auto ret = this->irb.getCurrentBlock();

		{
			fir::Function* func = this->module->getOrCreateFunction(
				fir::Name::obfuscate(zpr::sprint("%s_piece_%d", strs::names::GLOBAL_INIT_FUNCTION, this->globalInitPieces.size())),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::Internal);

			auto b = this->irb.addNewBlockInFunction("b", func);
			this->irb.setCurrentBlock(b);

			this->globalInitPieces.push_back(std::make_pair(val, func));
		}

		this->isInsideGlobalInitFunc = true;
		return ret;
	}

	void CodegenState::leaveGlobalInitFunction(fir::IRBlock* restore)
	{
		if(!this->isInsideGlobalInitFunc)
			error(this->loc(), "unsynchronised use of global init function!!! (leaving when not inside)");

		// terminate the current function.
		this->irb.ReturnVoid();

		this->irb.setCurrentBlock(restore);
		this->isInsideGlobalInitFunc = false;
	}

	void CodegenState::finishGlobalInitFunction()
	{
		if(this->finalisedGlobalInitFunction != 0)
		{
			// clear all the blocks from it.
			for(auto b : this->finalisedGlobalInitFunction->getBlockList())
				delete b;

			this->finalisedGlobalInitFunction->getBlockList().clear();
		}
		else
		{
			this->finalisedGlobalInitFunction = this->module->getOrCreateFunction(
				fir::Name::obfuscate(strs::names::GLOBAL_INIT_FUNCTION),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::Internal);
		}

		auto restore = this->irb.getCurrentBlock();

		// ok, now we can do some stuff.
		// what we wanna do is just call all the "piece" global init functions that we made with enter/leave.
		// but, this function has no blocks (either cos it's new, or we deleted them all). so, make one.
		auto blk = this->irb.addNewBlockInFunction("entry", this->finalisedGlobalInitFunction);
		this->irb.setCurrentBlock(blk);

		for(auto piece : this->globalInitPieces)
			this->irb.Call(piece.second);

		// ok now return
		this->irb.ReturnVoid();
		this->irb.setCurrentBlock(restore);
	}
}














