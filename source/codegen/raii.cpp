// raii.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "sst.h"
#include "codegen.h"
// #include "gluecode.h"
#include "string_consts.h"

namespace cgn
{
	void CodegenState::addRAIIValue(fir::Value* val)
	{
		if(!this->isRAIIType(val->getType()))
			error("val is not a class type! '%s'", val->getType());

		auto list = &this->blockPointStack.back().raiiValues;
		if(auto it = std::find(list->begin(), list->end(), val); it == list->end())
		{
			list->push_back(val);
		}
		else
		{
			error("adding duplicate raii value (ptr = %p, type = '%s')", reinterpret_cast<void*>(val), val->getType());
		}
	}

	void CodegenState::removeRAIIValue(fir::Value* val)
	{
		if(!this->isRAIIType(val->getType()))
			error("val is not a class type! '%s'", val->getType());

		auto list = &this->blockPointStack.back().raiiValues;
		if(auto it = std::find(list->begin(), list->end(), val); it != list->end())
		{
			list->erase(it);
		}
		else
		{
			error("removing non-existent raii value (ptr = %p, type = '%s')", reinterpret_cast<void*>(val), val->getType());
		}
	}

	std::vector<fir::Value*> CodegenState::getRAIIValues()
	{
		return this->blockPointStack.back().raiiValues;
	}

	void CodegenState::addRAIIOrRCValueIfNecessary(fir::Value* val, fir::Type* ty)
	{
		if(!ty)
			ty = val->getType();

		if(fir::isRefCountedType(ty))
			this->addRefCountedValue(val);

		if(this->isRAIIType(ty))
			this->addRAIIValue(val);
	}

	static fir::Value* getAddressOfOrMakeTemporaryLValue(CodegenState* cs, fir::Value* val, bool mut)
	{
		if(val->islvalue())
		{
			return cs->irb.AddressOf(val, mut);
		}
		else
		{
			auto tmp = cs->irb.CreateLValue(val->getType());
			cs->irb.Store(val, tmp);

			return cs->irb.AddressOf(tmp, mut);
		}
	}



	void CodegenState::callDestructor(fir::Value* val)
	{
		if(!this->typeHasDestructor(val->getType()))
			return;

		fir::Value* selfptr = getAddressOfOrMakeTemporaryLValue(this, val, /* mutable: */ true);


		if(val->getType()->isClassType())
		{
			auto cls = val->getType()->toClassType();

			// call the user-defined one first, if any:
			if(auto des = cls->getDestructor(); des)
				this->irb.Call(des, selfptr);

			// call the auto one. this will handle calling base class destructors for us!
			this->irb.Call(cls->getInlineDestructor(), selfptr);
		}
		else if(auto it = this->compilerSupportDefinitions.find(strs::names::support::RAII_TRAIT_DROP);
			it != this->compilerSupportDefinitions.end())
		{
			auto trt = dcast(sst::TraitDefn, it->second);
			if(!trt) error("invalid use of @compiler_support[\"raii_trait::drop\"] on non-trait definition!");

			iceAssert(trt->methods.size() == 1);

			auto str = dcast(sst::StructDefn, this->typeDefnMap[val->getType()]);
			iceAssert(str);

			auto destructor = this->findMatchingMethodInType(str, trt->methods[0]);
			if(destructor)
			{
				this->irb.Call(dcast(fir::Function, destructor->codegen(this).value), selfptr);
				return;
			}
		}
	}




	static fir::ClassType* doChecks(CodegenState* cs, fir::Value* from, fir::Value* target)
	{
		// this cleans up the callsites so we can just unconditionally call this.
		if(!from->getType()->isClassType())
		{
			cs->irb.Store(from, target);
			return 0;
		}

		auto clsty = from->getType()->toClassType();

		// TODO: this is a shitty error message.
		if(!target->islvalue())
			error(cs->loc(), "invalid operation on non-lvalue");

		return clsty;
	}

	static void doMemberWiseStuffIfNecessary(CodegenState* cs, fir::ClassType* clsty, fir::Value* from, fir::Value* target, bool move)
	{
		// check if there are even any class types inside. if not, do the simple thing!
		bool needSpecial = false;

		for(auto m : clsty->getElements())
		{
			if(m->isClassType())
			{
				needSpecial = true;
				break;
			}
		}

		if(needSpecial)
		{
			auto selfptr = getAddressOfOrMakeTemporaryLValue(cs, target, true);
			auto otherptr = getAddressOfOrMakeTemporaryLValue(cs, from, true);

			// assign `lhs = rhs`

			for(const auto& name : clsty->getNameList())
			{
				auto lhs = cs->irb.GetStructMember(cs->irb.Dereference(selfptr), name);
				auto rhs = cs->irb.GetStructMember(cs->irb.Dereference(otherptr), name);

				if(move)    cs->moveRAIIValue(rhs, lhs);
				else        cs->copyRAIIValue(rhs, lhs);
			}
		}
		else
		{
			cs->irb.Store(from, target);
		}
	}






	fir::Value* CodegenState::copyRAIIValue(fir::Value* value)
	{
		if(!value->getType()->isClassType())
			return value;

		// this will zero-initialise!
		auto ret = this->irb.CreateLValue(value->getType());

		this->copyRAIIValue(value, ret, /* enableMoving: */ false);
		return ret;
	}



	void CodegenState::copyRAIIValue(fir::Value* from, fir::Value* target, bool enableMoving)
	{
		iceAssert(from->getType() == target->getType());

		auto clsty = doChecks(this, from, target);
		if(!clsty) return;

		if(!from->islvalue() && enableMoving)
		{
			this->moveRAIIValue(from, target);
			return;
		}

		// if there is a copy-constructor, then we will call the copy constructor.
		if(auto copycon = clsty->getCopyConstructor(); copycon)
		{
			auto selfptr = getAddressOfOrMakeTemporaryLValue(this, target, true);
			auto otherptr = getAddressOfOrMakeTemporaryLValue(this, from, true);

			this->irb.Call(copycon, selfptr, otherptr);
		}
		else
		{
			doMemberWiseStuffIfNecessary(this, clsty, from, target, /* move: */ false);
		}
	}

	void CodegenState::moveRAIIValue(fir::Value* from, fir::Value* target)
	{
		iceAssert(from->getType() == target->getType());

		auto clsty = doChecks(this, from, target);
		if(!clsty) return;

		if(from->islvalue())
		{
			// you can't move from lvalues!
			this->copyRAIIValue(from, target);
			return;
		}

		if(auto movecon = clsty->getMoveConstructor(); movecon)
		{
			auto selfptr = getAddressOfOrMakeTemporaryLValue(this, target, true);
			auto otherptr = getAddressOfOrMakeTemporaryLValue(this, from, true);

			this->irb.Call(movecon, selfptr, otherptr);
		}
		else
		{
			doMemberWiseStuffIfNecessary(this, clsty, from, target, /* move: */ true);
		}

		this->removeRAIIValue(from);
	}


	// TODO: memoise this for each type; the typeHas-blalba ones also!
	static bool findRAIITraitImpl(CodegenState* cs, fir::Type* ty, const std::string& name)
	{
		if(ty->isClassType())
			return true;

		if(!ty->isStructType())
			return false;

		auto str = ty->toStructType();
		auto def = dcast(sst::StructDefn, cs->typeDefnMap[str]);
		iceAssert(def);

		return util::matchAny(def->traits, [name](sst::TraitDefn* trt) -> bool {
			// if we do not have such an attribute, then ::get returns an empty UA,
			// with an empty string as a name.
			return trt->attrs.get(strs::attrs::COMPILER_SUPPORT).name == strs::attrs::COMPILER_SUPPORT;
		});
	}


	bool CodegenState::typeHasDestructor(fir::Type* ty)
	{
		return findRAIITraitImpl(this, ty, strs::names::support::RAII_TRAIT_DROP);
	}

	bool CodegenState::typeHasCopyConstructor(fir::Type* ty)
	{
		return findRAIITraitImpl(this, ty, strs::names::support::RAII_TRAIT_COPY);
	}

	bool CodegenState::typeHasMoveConstructor(fir::Type* ty)
	{
		return findRAIITraitImpl(this, ty, strs::names::support::RAII_TRAIT_MOVE);
	}

	bool CodegenState::isRAIIType(fir::Type* ty)
	{
		return this->typeHasDestructor(ty) || this->typeHasCopyConstructor(ty) || this->typeHasMoveConstructor(ty);
	}
}





























