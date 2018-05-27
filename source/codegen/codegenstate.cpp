// codegenstate.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "errors.h"
#include "codegen.h"
#include "platform.h"
#include "typecheck.h"

namespace cgn
{
	void CodegenState::enterMethodBody(fir::Function* method, fir::Value* self)
	{
		this->methodSelfStack.push_back(self);

		iceAssert(self->getType()->isPointerType());

		auto ty = self->getType()->getPointerElementType();
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




	void CodegenState::enterFunction(fir::Function* fn)
	{
		this->functionStack.push_back(fn);
	}

	void CodegenState::leaveFunction()
	{
		if(this->functionStack.empty())
			error(this->loc(), "Not a in function");

		this->functionStack.pop_back();
	}

	fir::Function* CodegenState::getCurrentFunction()
	{
		if(this->functionStack.empty())
			error(this->loc(), "Not a in function");

		return this->functionStack.back();
	}



	ControlFlowPoint CodegenState::getCurrentCFPoint()
	{
		return this->breakingPointStack.back();
	}

	void CodegenState::enterBreakableBody(ControlFlowPoint cfp)
	{
		// only the block needs to exist.
		iceAssert(cfp.block);
		// cfp.vtree = this->vtree;

		this->breakingPointStack.push_back(cfp);
	}

	ControlFlowPoint CodegenState::leaveBreakableBody()
	{
		iceAssert(this->breakingPointStack.size() > 0);

		auto ret = this->breakingPointStack.back();
		this->breakingPointStack.pop_back();

		return ret;
	}




	// void CodegenState::pushLoc(const Location& l)
	// {
	// 	pushErrorLocation(l);
	// 	this->locationStack.push_back(l);
	// }

	void CodegenState::pushLoc(sst::Stmt* stmt)
	{
		this->locationStack.push_back(stmt->loc);
		// this->pushLoc(stmt->loc);

		pushErrorLocation(stmt);
	}

	void CodegenState::popLoc()
	{
		popErrorLocation();

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
		if(type->isStringType())
		{
			fir::Value* arr = this->irb.CreateValue(type);

			arr = this->irb.SetSAAData(arr, this->irb.GetArraySliceData(fir::ConstantString::get("")));
			arr = this->irb.SetSAALength(arr, fir::ConstantInt::getInt64(0));
			arr = this->irb.SetSAACapacity(arr, fir::ConstantInt::getInt64(0));
			arr = this->irb.SetSAARefCountPointer(arr, fir::ConstantValue::getZeroValue(fir::Type::getInt64Ptr()));

			return arr;
		}
		else if(type->isDynamicArrayType())
		{
			fir::Value* arr = this->irb.CreateValue(type);

			arr = this->irb.SetSAAData(arr, fir::ConstantValue::getZeroValue(type->getArrayElementType()->getMutablePointerTo()));
			arr = this->irb.SetSAALength(arr, fir::ConstantInt::getInt64(0));
			arr = this->irb.SetSAACapacity(arr, fir::ConstantInt::getInt64(0));
			arr = this->irb.SetSAARefCountPointer(arr, fir::ConstantValue::getZeroValue(fir::Type::getInt64Ptr()));

			return arr;
		}
		else if(type->isArraySliceType())
		{
			fir::Value* arr = this->irb.CreateValue(type);
			arr = this->irb.SetArraySliceData(arr, fir::ConstantValue::getZeroValue(type->getArrayElementType()->getPointerTo()));
			arr = this->irb.SetArraySliceLength(arr, fir::ConstantInt::getInt64(0));

			return arr;
		}
		else if(type->isClassType())
		{
			auto clsdef = this->typeDefnMap[type];
			iceAssert(clsdef);

			clsdef->codegen(this);

			// first need to check if we have any initialisers with 0 parameters.
			auto cls = type->toClassType();

			fir::Function* ifn = 0;
			for(auto init : cls->getInitialiserFunctions())
			{
				//* note: count == 1 because of 'self'
				if(init->getArgumentCount() == 1)
				{
					ifn = init;
					break;
				}
			}

			if(ifn == 0)
			{
				SimpleError::make(this->loc(), "Class '%s' cannot be automatically initialised as it does not have a constructor taking 0 arguments",
					cls->getTypeName()).append(SimpleError(MsgType::Note).set(clsdef, "Class '%s' was defined here:", clsdef->id.name)).postAndQuit();
			}

			// ok, we call it.
			auto self = this->irb.StackAlloc(cls);

			this->irb.Call(cls->getInlineInitialiser(), self);
			this->irb.Call(ifn, self);

			return this->irb.Load(self);
		}

		return fir::ConstantValue::getZeroValue(type);
	}



	fir::Function* CodegenState::getOrDeclareLibCFunction(std::string name)
	{
		if(name == ALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(ALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64() }, fir::Type::getMutInt8Ptr()), fir::LinkageType::External);
		}
		else if(name == FREE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(FREE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getMutInt8Ptr() }, fir::Type::getVoid()), fir::LinkageType::External);
		}
		else if(name == REALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(REALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getMutInt8Ptr(), fir::Type::getInt64() }, fir::Type::getMutInt8Ptr()), fir::LinkageType::External);
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
		else if(name == "strlen")
		{
			return this->module->getOrCreateFunction(Identifier("strlen", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getInt64()), fir::LinkageType::External);
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



	void CodegenState::addGlobalInitialiser(fir::Value* storage, fir::Value* value)
	{
		if(!storage->getType()->isPointerType())
		{
			error(this->loc(), "'storage' must be pointer type, got '%s'", storage->getType());
		}
		else if(storage->getType()->getPointerElementType() != value->getType())
		{
			error(this->loc(), "Cannot store value of type '%s' into storage of type '%s'", value->getType(),
				storage->getType());
		}

		// ok, then
		this->globalInits.push_back({ value, storage });
	}


	fir::IRBlock* CodegenState::enterGlobalInitFunction()
	{
		auto ret = this->irb.getCurrentBlock();

		if(!this->globalInitFunc)
		{
			fir::Function* func = this->module->getOrCreateFunction(Identifier("__global_init_function__", IdKind::Name),
				fir::FunctionType::get({ }, fir::Type::getVoid()), fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

			this->globalInitFunc = func;
		}

		iceAssert(this->globalInitFunc);
		this->irb.setCurrentBlock(this->globalInitFunc->getBlockList().back());

		return ret;
	}

	void CodegenState::leaveGlobalInitFunction(fir::IRBlock* restore)
	{
		this->irb.setCurrentBlock(restore);
	}

	void CodegenState::finishGlobalInitFunction()
	{
		auto r = this->enterGlobalInitFunction();

		this->irb.ReturnVoid();

		this->leaveGlobalInitFunction(r);
	}
}














