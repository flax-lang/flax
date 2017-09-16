// codegen.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include "defs.h"
#include "ir/irbuilder.h"

namespace fir
{
	struct Module;
	struct IRBuilder;
}

namespace sst
{
	struct Expr;
	struct Stmt;
	struct Defn;
	struct BinaryOp;
	struct StateTree;
	struct DefinitionTree;
}

namespace cgn
{
	struct CodegenState;

	namespace glue
	{
		#define ALLOCATE_MEMORY_FUNC						"malloc"
		#define REALLOCATE_MEMORY_FUNC						"realloc"
		#define FREE_MEMORY_FUNC							"free"

		namespace string
		{
			fir::Function* getCloneFunction(CodegenState* cs);
			fir::Function* getAppendFunction(CodegenState* cs);
			fir::Function* getCompareFunction(CodegenState* cs);
			fir::Function* getCharAppendFunction(CodegenState* cs);
			fir::Function* getBoundsCheckFunction(CodegenState* cs);
			fir::Function* getRefCountIncrementFunction(CodegenState* cs);
			fir::Function* getRefCountDecrementFunction(CodegenState* cs);
			fir::Function* getCheckLiteralWriteFunction(CodegenState* cs);
		}

		namespace array
		{
			fir::Function* getCloneFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getCloneFunction(CodegenState* cs, fir::ArraySliceType* arrtype);
			fir::Function* getCloneFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* elmtype);
			fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* elmtype);
			fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isPerformingDecomposition);
			fir::Function* getElementAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf);
			fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveExtraSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);

		}
	}

	struct ValueTree
	{
		ValueTree(std::string n, ValueTree* p) : name(n), parent(p) { }

		std::string name;
		ValueTree* parent = 0;

		std::unordered_map<std::string, std::vector<CGResult>> values;
		std::unordered_map<std::string, ValueTree*> subs;
	};

	struct CodegenState
	{
		CodegenState(const fir::IRBuilder& i) : irb(i) { }
		fir::Module* module = 0;
		sst::StateTree* stree = 0;
		cgn::ValueTree* vtree = 0;

		fir::IRBuilder irb;

		std::pair<fir::Function*, Location> entryFunction = { };

		std::vector<Location> locationStack;
		std::unordered_map<sst::Defn*, CGResult> valueMap;

		void pushLoc(const Location& loc);
		void pushLoc(sst::Stmt* stmt);
		void popLoc();

		Location loc();

		std::pair<sst::StateTree*, ValueTree*> setNamespace(std::vector<std::string> scope);
		void restoreNamespace(std::pair<sst::StateTree*, ValueTree*>);

		void enterNamespace(std::string name);
		void leaveNamespace();

		CGResult performBinaryOperation(const Location& loc, std::pair<Location, CGResult> lhs, std::pair<Location, CGResult> rhs, Operator op);
		CGResult performLogicalBinaryOperation(sst::BinaryOp* bo);

		std::pair<CGResult, CGResult> autoCastValueTypes(const CGResult& lhs, const CGResult& rhs);
		CGResult oneWayAutocast(const CGResult& from, fir::Type* target);

		fir::Value* getDefaultValue(fir::Type* type);

		fir::ConstantValue* unwrapConstantNumber(fir::ConstantValue* cv);
		fir::ConstantValue* unwrapConstantNumber(mpfr::mpreal num, fir::Type* target);

		fir::Function* getOrDeclareLibCFunction(std::string name);

		bool isRefCountedType(fir::Type* type);
		void incrementRefCount(fir::Value* ptr);
		void decrementRefCount(fir::Value* ptr);
	};

	fir::Module* codegen(sst::DefinitionTree* dtr);
}







