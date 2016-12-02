// LiteralCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	// check builtin type
	if(this->decimal)
	{
		return Result_t(fir::ConstantFP::get(fir::PrimitiveType::getUnspecifiedLiteralFloat(), this->dval), 0);
	}
	else if(this->needUnsigned)
	{
		return Result_t(fir::ConstantInt::get(fir::PrimitiveType::getUnspecifiedLiteralUint(), (uint64_t) this->ival), 0);
	}
	else
	{
		return Result_t(fir::ConstantInt::get(fir::PrimitiveType::getUnspecifiedLiteralInt(), this->ival), 0);
	}
}

static fir::Type* _makeReal(fir::Type* pt)
{
	if(pt->isPrimitiveType() && pt->toPrimitiveType()->isLiteralType())
	{
		if(pt->toPrimitiveType()->isFloatingPointType())
			return fir::Type::getFloat64();

		else if(pt->toPrimitiveType()->isSignedIntType())
			return fir::Type::getInt64();

		else
			return fir::Type::getUint64();
	}

	return pt;
}

static fir::ConstantValue* _makeReal(fir::ConstantValue* cv)
{
	if(fir::ConstantInt* ci = dynamic_cast<fir::ConstantInt*>(cv))
	{
		return fir::ConstantInt::get(_makeReal(ci->getType()), ci->getSignedValue());
	}
	else if(fir::ConstantFP* cf = dynamic_cast<fir::ConstantFP*>(cv))
	{
		return fir::ConstantFP::get(_makeReal(cf->getType()), cf->getValue());
	}
	else if(fir::ConstantArray* ca = dynamic_cast<fir::ConstantArray*>(cv))
	{
		std::vector<fir::ConstantValue*> vs;
		for(auto v : ca->getValues())
			vs.push_back(_makeReal(v));

		return fir::ConstantArray::get(fir::ArrayType::get(vs.front()->getType(), vs.size()), vs);
	}

	return cv;
}


fir::Type* Number::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->decimal)
	{
		return fir::PrimitiveType::getUnspecifiedLiteralFloat();
	}
	else if(this->needUnsigned)
	{
		return fir::PrimitiveType::getUnspecifiedLiteralUint();
	}
	else
	{
		return fir::PrimitiveType::getUnspecifiedLiteralInt();
	}
}






Result_t BoolVal::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(fir::ConstantInt::getBool(this->val), 0);
}

fir::Type* BoolVal::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return fir::Type::getBool();
}







Result_t NullVal::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	return Result_t(fir::ConstantValue::getNull(), 0);
}

fir::Type* NullVal::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return fir::Type::getVoid()->getPointerTo();
}






Result_t StringLiteral::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	if(this->isRaw)
	{
		// good old Int8*
		fir::ConstantValue* stringVal = cgi->module->createGlobalString(this->str);
		stringVal = cgi->irb.CreateConstFixedGEP2(stringVal, 0, 0);

		return Result_t(stringVal, 0);
	}
	else
	{
		if(extra && extra->getType()->getPointerElementType()->isStringType())
		{
			// these things can't be const

			iceAssert(extra->getType()->getPointerElementType()->isStringType());


			/*
				// note(portability): see CodegenInstance::makeStringLiteral()
				std::string s = this->str;
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);
				s.insert(s.begin(), 0xFF);

				fir::Value* thestring = cgi->module->createGlobalString(s);
				thestring = cgi->irb.CreateFixedGEP2(thestring, 0, 0);

				fir::Value* len = fir::ConstantInt::getInt64(this->str.length());

				thestring = cgi->irb.CreatePointerAdd(thestring, fir::ConstantInt::getInt64(8));
				cgi->irb.CreateSetStringData(extra, thestring);
				cgi->irb.CreateSetStringLength(extra, len);
			*/

			// we don't (and can't) set the refcount, because it's probably in read-only memory.
			// the -1 is reflected in the string literal already.

			fir::ConstantString* cs = fir::ConstantString::get(this->str);
			cgi->irb.CreateStore(cs, extra);

			cgi->addRefCountedValue(extra);
			if(!extra->hasName())
				extra->setName("strlit");

			return Result_t(cgi->irb.CreateLoad(extra), extra);
		}
		else if(extra && extra->getType()->getPointerElementType()->isCharType())
		{
			if(this->str.length() == 0)
				error(this, "Character literal cannot be empty");

			else if(this->str.length() > 1)
				error(this, "Character literal can have at most 1 (ASCII) character");

			char c = this->str[0];
			fir::ConstantValue* cv = fir::ConstantChar::get(c);
			cgi->irb.CreateStore(cv, extra);

			return Result_t(cv, extra);
		}
		else
		{
			auto r = cgi->makeStringLiteral(this->str);
			r.pointer->setName("strlit");

			return r;
		}
	}
}

fir::Type* StringLiteral::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	if(this->isRaw)
		return fir::Type::getInt8Ptr();

	else
		return fir::Type::getStringType();
}












Result_t ArrayLiteral::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Type* tp = 0;
	std::vector<fir::ConstantValue*> vals;

	if(this->values.size() == 0)
	{
		if(!extra)
		{
			error(this, "Unable to infer type for empty array");
		}

		tp = extra->getType()->getPointerElementType();
	}
	else
	{
		tp = _makeReal(this->values.front()->getType(cgi));

		for(Expr* e : this->values)
		{
			fir::Value* v = e->codegen(cgi).value;
			if(dynamic_cast<fir::ConstantValue*>(v))
			{
				fir::ConstantValue* c = dynamic_cast<fir::ConstantValue*>(v);
				vals.push_back(_makeReal(c));

				if(vals.back()->getType() != tp)
				{
					error(e, "Array members must have the same type, got %s and %s",
						tp->str().c_str(), vals.back()->getType()->str().c_str());
				}
			}
			else
			{
				error(e, "Array literal members must be constant");
			}
		}
	}

	fir::ArrayType* atype = fir::ArrayType::get(tp, this->values.size());
	fir::Value* alloc = cgi->irb.CreateStackAlloc(atype);
	fir::Value* val = fir::ConstantArray::get(atype, vals);

	cgi->irb.CreateStore(val, alloc);
	return Result_t(val, alloc);
}

fir::Type* ArrayLiteral::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	return fir::ArrayType::get(_makeReal(this->values.front()->getType(cgi)), this->values.size());
}

















fir::TupleType* Tuple::getType(CodegenInstance* cgi, bool allowFail, fir::Value* extra)
{
	// todo: handle named tuples.
	// would probably just be handled as implicit anon structs
	// (randomly generated name or something), with appropriate code to handle
	// assignment to and from.

	if(this->ltypes.size() == 0)
	{
		iceAssert(!this->didCreateType);

		for(Expr* e : this->values)
			this->ltypes.push_back(_makeReal(e->getType(cgi)));

		this->ident.name = "__anonymoustuple_" + std::to_string(cgi->typeMap.size());
		this->createdType = fir::TupleType::get(this->ltypes, cgi->getContext());
		this->didCreateType = true;

		// todo: debate, should we add this?
		// edit: no.
		// cgi->addNewType(this->createdType, this, TypeKind::Tuple);
	}

	return this->createdType;
}

fir::Type* Tuple::createType(CodegenInstance* cgi)
{
	(void) cgi;
	return 0;
}

Result_t Tuple::codegen(CodegenInstance* cgi, fir::Value* extra)
{
	fir::Value* gep = 0;
	if(extra)
	{
		gep = extra;
	}
	else
	{
		gep = cgi->getStackAlloc(this->getType(cgi));
	}

	iceAssert(gep);

	fir::TupleType* tuptype = gep->getType()->getPointerElementType()->toTupleType();
	iceAssert(tuptype);

	// set all the values.
	// do the gep for each.

	iceAssert(tuptype->getElementCount() == this->values.size());

	for(size_t i = 0; i < tuptype->getElementCount(); i++)
	{
		fir::Value* member = cgi->irb.CreateStructGEP(gep, i);
		fir::Value* val = this->values[i]->codegen(cgi).value;

		val = cgi->autoCastType(member->getType()->getPointerElementType(), val);

		if(val->getType() != member->getType()->getPointerElementType())
		{
			error(this, "Element %zu of tuple is mismatched, expected '%s' but got '%s'", i,
				member->getType()->getPointerElementType()->str().c_str(), val->getType()->str().c_str());
		}

		cgi->irb.CreateStore(val, member);
	}

	return Result_t(cgi->irb.CreateLoad(gep), gep);
}











