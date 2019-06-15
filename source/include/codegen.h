// codegen.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "defs.h"
#include "stcommon.h"
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
	struct TypeDefn;
	struct BinaryOp;
	struct StateTree;
	struct FunctionDefn;
	struct FunctionCall;
	struct DefinitionTree;
}

namespace cgn
{
	struct ControlFlowPoint
	{
		ControlFlowPoint(sst::Block* b, fir::IRBlock* bp, fir::IRBlock* cp) :
			block(b), breakPoint(bp), continuePoint(cp) { }

		sst::Block* block = 0;

		// std::vector<fir::Value*> refCountedValues;
		// std::vector<fir::Value*> refCountedPointers;

		fir::IRBlock* breakPoint = 0;
		fir::IRBlock* continuePoint = 0;
	};

	struct BlockPoint
	{
		BlockPoint(sst::Block* b) : block(b) { }

		sst::Block* block = 0;

		std::vector<fir::Value*> refCountedValues;
		std::vector<fir::Value*> refCountedPointers;
	};

	struct CodegenState
	{
		enum class OperatorFn
		{
			None,

			Builtin,
			UserDefined
		};



		CodegenState(const fir::IRBuilder& i) : irb(i) { }
		fir::Module* module = 0;
		sst::StateTree* stree = 0;

		fir::IRBuilder irb;

		std::pair<fir::Function*, Location> entryFunction = { };

		std::vector<Location> locationStack;
		util::hash_map<sst::Defn*, CGResult> valueMap;
		std::vector<fir::Value*> methodSelfStack;

		fir::Function* globalInitFunc = 0;
		std::vector<std::pair<fir::Value*, fir::Value*>> globalInits;

		util::hash_map<fir::Function*, fir::Type*> methodList;

		util::hash_map<fir::Type*, sst::TypeDefn*> typeDefnMap;


		size_t _debugIRIndent = 0;
		void pushIRDebugIndentation();
		void printIRDebugMessage(const std::string& msg, const std::vector<fir::Value*>& vals);
		void popIRDebugIndentation();


		void pushLoc(sst::Stmt* stmt);
		void popLoc();

		Location loc();

		void enterMethodBody(fir::Function* method, fir::Value* self);
		void leaveMethodBody();

		bool isInMethodBody();
		fir::Value* getMethodSelf();

		std::vector<fir::Function*> functionStack;
		fir::Function* getCurrentFunction();
		void enterFunction(fir::Function* fn);
		void leaveFunction();

		std::vector<ControlFlowPoint> breakingPointStack;
		ControlFlowPoint getCurrentCFPoint();

		void enterBreakableBody(const ControlFlowPoint& cfp);
		ControlFlowPoint leaveBreakableBody();

		std::vector<BlockPoint> blockPointStack;
		BlockPoint getCurrentBlockPoint();
		void enterBlock(const BlockPoint& bp);
		void leaveBlock();

		std::vector<fir::Value*> subscriptArrayLengthStack;
		fir::Value* getCurrentSubscriptArrayLength();
		void enterSubscriptWithLength(fir::Value* len);
		void leaveSubscript();


		CGResult performBinaryOperation(const Location& loc, std::pair<Location, fir::Value*> lhs, std::pair<Location, fir::Value*> rhs,
			std::string op);
		CGResult performLogicalBinaryOperation(sst::BinaryOp* bo);

		std::pair<fir::Value*, fir::Value*> autoCastValueTypes(fir::Value* lhs, fir::Value* rhs);
		fir::Value* oneWayAutocast(fir::Value* from, fir::Type* target);

		fir::Value* getDefaultValue(fir::Type* type);

		fir::Value* getConstructedStructValue(fir::StructType* str, const std::vector<FnCallArgument>& args);
		void constructClassWithArguments(fir::ClassType* cls, sst::FunctionDefn* constr, fir::Value* selfptr, const std::vector<FnCallArgument>& args,
			bool callInlineInitialiser);

		fir::Value* callVirtualMethod(sst::FunctionCall* call);

		fir::ConstantValue* unwrapConstantNumber(fir::ConstantValue* cv);
		fir::ConstantValue* unwrapConstantNumber(fir::ConstantNumber* cv, fir::Type* target);

		CGResult getStructFieldImplicitly(std::string name);

		fir::Function* getOrDeclareLibCFunction(std::string name);

		void addGlobalInitialiser(fir::Value* storage, fir::Value* value);

		fir::IRBlock* enterGlobalInitFunction();
		void leaveGlobalInitFunction(fir::IRBlock* restore);
		void finishGlobalInitFunction();

		void generateDecompositionBindings(const DecompMapping& bind, CGResult rhs, bool allowref);

		util::hash_map<std::string, size_t> getNameIndexMap(sst::FunctionDefn* fd);

		std::vector<fir::Value*> codegenAndArrangeFunctionCallArguments(sst::Defn* target, fir::FunctionType* ft, const std::vector<FnCallArgument>& args);



		void addVariableUsingStorage(sst::VarDefn* var, fir::Value* ptr, CGResult val);

		void createWhileLoop(const std::function<void (fir::IRBlock*, fir::IRBlock*)>& check, const std::function<void ()>& body);

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

		void performRefCountingAssignment(fir::Value* lhs, fir::Value* rhs, bool isInitial);
		void moveRefCountedValue(fir::Value* lhs, fir::Value* rhs, bool isInitial);

		void autoAssignRefCountedValue(fir::Value* lhs, fir::Value* rhs, bool isInitial, bool performStore);
	};

	fir::Module* codegen(sst::DefinitionTree* dtr);
}







