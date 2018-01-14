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
	struct Block;
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
			fir::Function* getUnicodeLengthFunction(CodegenState* cs);
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
			fir::Function* getPopElementFromBackFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getBoundsCheckFunction(CodegenState* cs, bool isPerformingDecomposition);
			fir::Function* getElementAppendFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getSetElementsToDefaultValueFunction(CodegenState* cs, fir::Type* elmType);
			fir::Function* getCompareFunction(CodegenState* cs, fir::Type* arrtype, fir::Function* opf);
			fir::Function* getConstructFromTwoFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getIncrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getDecrementArrayRefCountFunction(CodegenState* cs, fir::Type* arrtype);
			fir::Function* getReserveSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
			fir::Function* getReserveExtraSpaceForElementsFunction(CodegenState* cs, fir::DynamicArrayType* arrtype);
		}

		namespace misc
		{
			fir::Function* getRangeSanityCheckFunction(CodegenState* cs);
		}
	}

	struct ControlFlowPoint
	{
		ControlFlowPoint(sst::Block* b, fir::IRBlock* bp, fir::IRBlock* cp) :
			block(b), breakPoint(bp), continuePoint(cp) { }

		sst::Block* block = 0;

		std::vector<fir::Value*> refCountedValues;
		std::vector<fir::Value*> refCountedPointers;

		fir::IRBlock* breakPoint = 0;
		fir::IRBlock* continuePoint = 0;
	};


	struct CodegenState
	{
		CodegenState(const fir::IRBuilder& i) : irb(i) { }
		fir::Module* module = 0;
		sst::StateTree* stree = 0;

		fir::IRBuilder irb;

		std::pair<fir::Function*, Location> entryFunction = { };

		std::vector<Location> locationStack;
		std::unordered_map<sst::Defn*, CGResult> valueMap;
		std::vector<fir::Value*> methodSelfStack;

		fir::Function* globalInitFunc = 0;
		std::vector<std::pair<fir::Value*, fir::Value*>> globalInits;

		void pushLoc(const Location& loc);
		void pushLoc(sst::Stmt* stmt);
		void popLoc();

		Location loc();

		void enterMethodBody(fir::Value* self);
		void leaveMethodBody();

		bool isInMethodBody();
		fir::Value* getMethodSelf();

		std::vector<fir::Function*> functionStack;
		fir::Function* getCurrentFunction();
		void enterFunction(fir::Function* fn);
		void leaveFunction();

		std::vector<ControlFlowPoint> breakingPointStack;
		ControlFlowPoint getCurrentCFPoint();

		void enterBreakableBody(ControlFlowPoint cfp);
		ControlFlowPoint leaveBreakableBody();

		// CGResult findValueInTree(std::string name, ValueTree* vt = 0);

		CGResult performBinaryOperation(const Location& loc, std::pair<Location, CGResult> lhs, std::pair<Location, CGResult> rhs, std::string op);
		CGResult performLogicalBinaryOperation(sst::BinaryOp* bo);

		std::pair<CGResult, CGResult> autoCastValueTypes(const CGResult& lhs, const CGResult& rhs);
		CGResult oneWayAutocast(const CGResult& from, fir::Type* target);

		fir::Value* getDefaultValue(fir::Type* type);

		fir::ConstantValue* unwrapConstantNumber(fir::ConstantValue* cv);
		fir::ConstantValue* unwrapConstantNumber(mpfr::mpreal num, fir::Type* target);

		CGResult getStructFieldImplicitly(std::string name);

		fir::Function* getOrDeclareLibCFunction(std::string name);

		void addGlobalInitialiser(fir::Value* storage, fir::Value* value);

		fir::IRBlock* enterGlobalInitFunction();
		void leaveGlobalInitFunction(fir::IRBlock* restore);
		void finishGlobalInitFunction();

		enum class OperatorFn
		{
			None,

			Builtin,
			UserDefined
		};

		std::pair<OperatorFn, fir::Function*> getOperatorFunctionForTypes(fir::Type* a, fir::Type* b, std::string op);

		bool isRefCountedType(fir::Type* type);
		void incrementRefCount(fir::Value* val);
		void decrementRefCount(fir::Value* val);

		void addRefCountedValue(fir::Value* val);
		void removeRefCountedValue(fir::Value* val, bool ignoreMissing = false);

		void addRefCountedPointer(fir::Value* ptr);
		void removeRefCountedPointer(fir::Value* ptr, bool ignoreMissing = false);

		std::vector<fir::Value*> getRefCountedValues();
		std::vector<fir::Value*> getRefCountedPointers();

		void performRefCountingAssignment(CGResult lhs, CGResult rhs, bool isInitial);
		void moveRefCountedValue(CGResult lhs, CGResult rhs, bool isInitial);

		void autoAssignRefCountedValue(CGResult lhs, CGResult rhs, bool isInitial, bool performStore);
	};

	fir::Module* codegen(sst::DefinitionTree* dtr);
}







