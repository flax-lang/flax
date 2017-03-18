// LiteralCodegen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "codegen.h"

using namespace Ast;
using namespace Codegen;

Result_t Number::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	if(this->str.find('.') != std::string::npos)
	{
		// parse a ƒloating point
		try
		{
			long double num = std::stold(this->str);

			if(extratype && extratype->isFloatingPointType())
			{
				if(fir::checkFloatingPointLiteralFitsIntoType(extratype->toPrimitiveType(), num))
					return Result_t(fir::ConstantFP::get(extratype, num), 0);

				else
					error(this, "Floating point literal '%Lf' cannot fit into type '%s'", num, extratype->str().c_str());
			}


			// use f64 if we can, if not f80
			if(fir::checkFloatingPointLiteralFitsIntoType(fir::Type::getFloat64(), num))
				return Result_t(fir::ConstantFP::get(fir::Type::getFloat64(), num), 0);

			else
				return Result_t(fir::ConstantFP::get(fir::Type::getFloat80(), num), 0);
		}
		catch(std::out_of_range& e)
		{
			error(this, "Number '%s' is out of range", this->str.c_str());
		}
	}
	else
	{
		std::string s = this->str;
		// because c++ is stupid, 1. it doesn't support binary and 2. the default base is 10 where it doesn't autodetect,
		// just figure out the base on our own.

		int base = 10;
		if(s.find("0x") != std::string::npos)
			base = 16, s = s.substr(2);

		else if(s.find("0b") != std::string::npos)
			base = 2, s = s.substr(2);

		try
		{
			int64_t num = std::stoll(s, 0, base);

			if(extratype && extratype->isIntegerType())
			{
				if(fir::checkSignedIntLiteralFitsIntoType(extratype->toPrimitiveType(), num))
					return Result_t(fir::ConstantInt::get(extratype, num), 0);

				else
					error(this, "Integer literal '%lld' cannot fit into type '%s'", num, extratype->str().c_str());
			}

			return Result_t(fir::ConstantInt::get(fir::Type::getInt64(), num), 0);
		}
		catch(std::out_of_range& e)
		{
			try
			{
				uint64_t num = std::stoull(this->str, 0, base);

				if(extratype && extratype->isIntegerType())
				{
					if(fir::checkUnsignedIntLiteralFitsIntoType(extratype->toPrimitiveType(), num))
						return Result_t(fir::ConstantInt::get(extratype, num), 0);

					else
						error(this, "Integer literal '%llu' cannot fit into type '%s'", num, extratype->str().c_str());
				}


				return Result_t(fir::ConstantInt::get(fir::Type::getUint64(), num), 0);
			}
			catch(std::out_of_range& e1)
			{
				error(this, "Number '%s' is out of range of the largest possible size (u64)", this->str.c_str());
			}
		}
	}
}

// static fir::Type* _makeReal(fir::Type* pt)
// {
// 	if(pt->isPrimitiveType() && pt->toPrimitiveType()->isLiteralType())
// 	{
// 		if(pt->toPrimitiveType()->isFloatingPointType())
// 			return fir::Type::getFloat64();

// 		else if(pt->toPrimitiveType()->isSignedIntType())
// 			return fir::Type::getInt64();

// 		else
// 			return fir::Type::getUint64();
// 	}

// 	return pt;
// }

// static fir::ConstantValue* _makeReal(fir::ConstantValue* cv)
// {
// 	if(fir::ConstantInt* ci = dynamic_cast<fir::ConstantInt*>(cv))
// 	{
// 		return fir::ConstantInt::get(_makeReal(ci->getType()), ci->getSignedValue());
// 	}
// 	else if(fir::ConstantFP* cf = dynamic_cast<fir::ConstantFP*>(cv))
// 	{
// 		return fir::ConstantFP::get(_makeReal(cf->getType()), cf->getValue());
// 	}
// 	else if(fir::ConstantArray* ca = dynamic_cast<fir::ConstantArray*>(cv))
// 	{
// 		std::vector<fir::ConstantValue*> vs;
// 		for(auto v : ca->getValues())
// 			vs.push_back(_makeReal(v));

// 		return fir::ConstantArray::get(fir::ArrayType::get(vs.front()->getType(), vs.size()), vs);
// 	}

// 	return cv;
// }


// fir::Type* Number::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
// {
// 	if(this->str.find('.') != std::string::npos)
// 		return fir::PrimitiveType::getUnspecifiedLiteralFloat();


// 	std::string s = this->str;
// 	// because c++ is stupid, 1. it doesn't support binary and 2. the default base is 10 where it doesn't autodetect,
// 	// just figure out the base on our own.

// 	int base = 10;
// 	if(s.find("0x") != std::string::npos)
// 		base = 16, s = s.substr(2);

// 	else if(s.find("0b") != std::string::npos)
// 		base = 2, s = s.substr(2);

// 	try
// 	{
// 		std::stoll(this->str, 0, base);
// 		return fir::PrimitiveType::getUnspecifiedLiteralInt();
// 	}
// 	catch(std::out_of_range& e)
// 	{
// 		try
// 		{
// 			std::stoull(this->str, 0, base);
// 			return fir::PrimitiveType::getUnspecifiedLiteralUint();
// 		}
// 		catch(std::out_of_range& e1)
// 		{
// 			error(this, "Number '%s' is out of range of the largest possible size (u64)", this->str.c_str());
// 		}
// 	}
// }


fir::Type* Number::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	if(this->str.find('.') != std::string::npos)
	{
		// parse a ƒloating point
		try
		{
			long double num = std::stold(this->str);

			if(extratype && extratype->isFloatingPointType())
			{
				if(fir::checkFloatingPointLiteralFitsIntoType(extratype->toPrimitiveType(), num))
					return extratype;

				else
					error(this, "Floating point literal '%Lf' cannot fit into type '%s'", num, extratype->str().c_str());
			}


			// use f64 if we can, if not f80
			if(fir::checkFloatingPointLiteralFitsIntoType(fir::Type::getFloat64(), num))
				return fir::Type::getFloat64();

			else
				return fir::Type::getFloat80();
		}
		catch(std::out_of_range& e)
		{
			error(this, "Number '%s' is out of range", this->str.c_str());
		}
	}
	else
	{
		std::string s = this->str;
		// because c++ is stupid, 1. it doesn't support binary and 2. the default base is 10 where it doesn't autodetect,
		// just figure out the base on our own.

		int base = 10;
		if(s.find("0x") != std::string::npos)
			base = 16, s = s.substr(2);

		else if(s.find("0b") != std::string::npos)
			base = 2, s = s.substr(2);

		try
		{
			int64_t num = std::stoll(s, 0, base);

			if(extratype && extratype->isIntegerType())
			{
				if(fir::checkSignedIntLiteralFitsIntoType(extratype->toPrimitiveType(), num))
					return extratype;

				else
					error(this, "Integer literal '%lld' cannot fit into type '%s'", num, extratype->str().c_str());
			}

			return fir::Type::getInt64();
		}
		catch(std::out_of_range& e)
		{
			try
			{
				uint64_t num = std::stoull(this->str, 0, base);

				if(extratype && extratype->isIntegerType())
				{
					if(fir::checkUnsignedIntLiteralFitsIntoType(extratype->toPrimitiveType(), num))
						return extratype;

					else
						error(this, "Integer literal '%llu' cannot fit into type '%s'", num, extratype->str().c_str());
				}


				return fir::Type::getUint64();
			}
			catch(std::out_of_range& e1)
			{
				error(this, "Number '%s' is out of range of the largest possible size (u64)", this->str.c_str());
			}
		}
	}
}






Result_t BoolVal::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	return Result_t(fir::ConstantInt::getBool(this->val), 0);
}

fir::Type* BoolVal::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return fir::Type::getBool();
}







Result_t NullVal::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	return Result_t(fir::ConstantValue::getNull(), 0);
}

fir::Type* NullVal::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	return fir::Type::getVoid()->getPointerTo();
}






Result_t StringLiteral::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	if(this->isRaw)
	{
		// good old Int8*
		fir::Value* stringVal = cgi->module->createGlobalString(this->str);
		return Result_t(stringVal, 0);
	}
	else
	{
		if(target && target->getType()->isPointerType() && target->getType()->getPointerElementType()->isStringType())
		{
			// these things can't be const
			iceAssert(target->getType()->getPointerElementType()->isStringType());

			// we don't (and can't) set the refcount, because it's probably in read-only memory.

			fir::ConstantString* cs = fir::ConstantString::get(this->str);
			cgi->irb.CreateStore(cs, target);

			if(!target->hasName())
				target->setName("strlit");

			return Result_t(cs, target);
		}
		else if(target && target->getType()->isPointerType() && target->getType()->getPointerElementType()->isCharType())
		{
			if(this->str.length() == 0)
				error(this, "Character literal cannot be empty");

			else if(this->str.length() > 1)
				error(this, "Character literal can have at most 1 (ASCII) character (have '%s')", this->str.c_str());

			char c = this->str[0];
			fir::ConstantValue* cv = fir::ConstantChar::get(c);
			cgi->irb.CreateStore(cv, target);

			return Result_t(cv, target);
		}
		else
		{
			return cgi->makeStringLiteral(this->str);
		}
	}
}

fir::Type* StringLiteral::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	if(this->isRaw)	return fir::Type::getInt8Ptr();

	return fir::Type::getStringType();
}












Result_t ArrayLiteral::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	bool raw = this->attribs & Attr_RawString;

	fir::Type* tp = 0;
	std::vector<fir::ConstantValue*> vals;

	if(this->values.size() == 0)
	{
		if(!extratype && !target)
		{
			error(this, "Unable to infer type for empty array");
		}

		if(raw && extratype->isPointerType() && extratype->getPointerElementType()->isArrayType())
		{
			// iceAssert(extratype->isPointerType() && extratype->getPointerElementType()->isArrayType());
			tp = extratype->getPointerElementType()->toArrayType()->getElementType();
		}
		else
		{
			tp = extratype;
			if(tp->isDynamicArrayType())
			{
				// ok, make a dynamic array instead. don't return some half-assed thing
				auto elmtype = tp->toDynamicArrayType()->getElementType();
				if(elmtype->isParametricType())
					error(this, "Unable to infer type for empty array literal");

				fir::Value* ai = cgi->irb.CreateStackAlloc(fir::DynamicArrayType::get(elmtype));

				cgi->irb.CreateSetDynamicArrayData(ai, cgi->irb.CreatePointerTypeCast(fir::ConstantValue::getNull(), fir::Type::getInt64Ptr()));
				cgi->irb.CreateSetDynamicArrayLength(ai, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateSetDynamicArrayCapacity(ai, fir::ConstantInt::getInt64(0));

				return Result_t(cgi->irb.CreateLoad(ai), ai, ValueKind::RValue);
			}
			else if(target && target->getType()->isPointerType() && target->getType()->getPointerElementType()->isDynamicArrayType())
			{
				// ok, make a dynamic array instead. don't return some half-assed thing
				fir::Value* ai = target;

				cgi->irb.CreateSetDynamicArrayData(ai, cgi->irb.CreatePointerTypeCast(fir::ConstantValue::getNull(), fir::Type::getInt64Ptr()));
				cgi->irb.CreateSetDynamicArrayLength(ai, fir::ConstantInt::getInt64(0));
				cgi->irb.CreateSetDynamicArrayCapacity(ai, fir::ConstantInt::getInt64(0));

				return Result_t(cgi->irb.CreateLoad(ai), ai, ValueKind::RValue);
			}
			else
			{
				error(this, "?? extratype wrong: '%s' / '%s'", extratype->str().c_str(),
					target ? target->getType()->str().c_str() : "(null)");
			}
		}
	}
	else
	{
		tp = this->values.front()->getType(cgi);

		fir::Type* targetType = 0;
		if(extratype)
		{
			if(extratype->isArrayType())
				targetType = extratype->toArrayType()->getElementType();

			else if(extratype->isDynamicArrayType())
				targetType = extratype->toDynamicArrayType()->getElementType();
		}
		else if(target)
		{
			if(target->getType()->isPointerType() && target->getType()->getPointerElementType()->isArrayType())
				targetType = target->getType()->getPointerElementType()->toArrayType()->getElementType();

			else if(target->getType()->isPointerType() && target->getType()->getPointerElementType()->isDynamicArrayType())
				targetType = target->getType()->getPointerElementType()->toDynamicArrayType()->getElementType();
		}

		if(targetType)
			tp = targetType;


		for(Expr* e : this->values)
		{
			fir::Value* v = e->codegen(cgi).value;
			if(dynamic_cast<fir::ConstantValue*>(v))
			{
				fir::ConstantValue* c = dynamic_cast<fir::ConstantValue*>(v);

				// attempt to make a proper thing if we're int literals
				if(c->getType()->isIntegerType() && tp->isIntegerType())
				{
					fir::ConstantInt* ci = dynamic_cast<fir::ConstantInt*>(c);
					iceAssert(ci);

					// c is a constant.
					bool res = false;
					if(c->getType()->isSignedIntType())
						res = fir::checkSignedIntLiteralFitsIntoType(tp->toPrimitiveType(), ci->getSignedValue());

					else
						res = fir::checkUnsignedIntLiteralFitsIntoType(tp->toPrimitiveType(), ci->getUnsignedValue());


					if(res)
						c = fir::ConstantInt::get(tp, ci->getUnsignedValue());
				}


				if(c->getType() != tp)
				{
					error(e, "Array members must have the same type, got %s and %s",
						c->getType()->str().c_str(), tp->str().c_str());
				}

				vals.push_back(c);
			}
			else
			{
				error(e, "Array literal members must be constant");
			}
		}
	}

	// since we're creating a constant array in between as well, this is necessary.

	fir::ArrayType* atype = fir::ArrayType::get(tp, this->values.size());
	auto ret = fir::ConstantDynamicArray::get(fir::ConstantArray::get(atype, vals));
	return Result_t(ret, 0);
}

fir::Type* ArrayLiteral::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	bool raw = this->attribs & Attr_RawString;

	if(this->values.empty())
	{
		if(!extratype)
		{
			// error(this, "Unable to infer type for empty array");
			return fir::DynamicArrayType::get(fir::ParametricType::get("~"));
		}

		if(raw && extratype->isArrayType())
		{
			auto tp = extratype->toArrayType()->getElementType();
			return fir::ArrayType::get(tp, 0);
		}
		else if(extratype->isDynamicArrayType())
		{
			// ok, make a dynamic array instead. don't return some half-assed thing
			auto elmtype = extratype->toDynamicArrayType()->getElementType();
			return fir::DynamicArrayType::get(elmtype);
		}
		else
		{
			error(this, "?? extratype wrong: '%s'", extratype->str().c_str());
		}
	}
	else
	{
		if(raw)	return fir::ArrayType::get(this->values.front()->getType(cgi), this->values.size());
		else	return fir::DynamicArrayType::get(this->values.front()->getType(cgi));
	}
}
















static size_t _counter = 0;
fir::TupleType* Tuple::getType(CodegenInstance* cgi, fir::Type* extratype, bool allowFail)
{
	// todo: handle named tuples.
	// would probably just be handled as implicit anon structs
	// (randomly generated name or something), with appropriate code to handle
	// assignment to and from.

	if(this->ltypes.size() == 0)
	{
		iceAssert(!this->didCreateType);

		for(Expr* e : this->values)
			this->ltypes.push_back(e->getType(cgi));

		this->ident.name = "__anonymoustuple_" + std::to_string(_counter++);
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

Result_t Tuple::codegen(CodegenInstance* cgi, fir::Type* extratype, fir::Value* target)
{
	fir::TupleType* tuptype = this->getType(cgi)->toTupleType();
	iceAssert(tuptype);

	iceAssert(tuptype->getElementCount() == this->values.size());

	// first check if we can make a constant.
	bool allConst = true;
	std::vector<fir::Value*> vals;
	for(auto v : this->values)
	{
		auto cgv = v->codegen(cgi).value;
		allConst = allConst && (dynamic_cast<fir::ConstantValue*>(cgv) != 0);

		vals.push_back(cgv);
	}

	if(allConst)
	{
		std::vector<fir::ConstantValue*> cvs;
		for(auto v : vals)
		{
			auto cv = dynamic_cast<fir::ConstantValue*>(v); iceAssert(cv);
			cvs.push_back(cv);
		}

		fir::ConstantTuple* ct = fir::ConstantTuple::get(cvs);
		iceAssert(ct);

		// if all const, fuck storing anything
		// return an rvalue
		return Result_t(ct, 0);
	}
	else
	{
		// set all the values.
		// do the gep for each.

		fir::Value* gep = target ? target : cgi->getStackAlloc(this->getType(cgi));
		iceAssert(gep);

		for(size_t i = 0; i < tuptype->getElementCount(); i++)
		{
			fir::Value* member = cgi->irb.CreateStructGEP(gep, i);
			fir::Value* val = vals[i];

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
}











