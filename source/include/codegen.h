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

	namespace interp
	{
		struct InterpState;
	}
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
	struct FunctionDecl;
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

		fir::IRBlock* breakPoint = 0;
		fir::IRBlock* continuePoint = 0;
	};

	struct BlockPoint
	{
		BlockPoint(sst::Block* b) : block(b) { }

		sst::Block* block = 0;
		std::vector<fir::Value*> raiiValues;
	};


	struct CodegenState
	{
		enum class OperatorFn
		{
			None,

			Builtin,
			UserDefined
		};

		CodegenState(const fir::IRBuilder& i);

		size_t id = 0;
		fir::Module* module = 0;

		fir::IRBuilder irb;

		std::pair<fir::Function*, Location> entryFunction = { };

		std::vector<Location> locationStack;
		util::hash_map<sst::Defn*, CGResult> valueMap;
		std::vector<fir::Value*> methodSelfStack;


		util::hash_map<fir::Function*, fir::Type*> methodList;

		util::hash_map<fir::Type*, sst::TypeDefn*> typeDefnMap;
		util::hash_map<std::string, sst::Defn*> compilerSupportDefinitions;


		size_t _debugIRIndent = 0;
		void pushIRDebugIndentation();
		void printIRDebugMessage(const std::string& msg, const std::vector<fir::Value*>& vals);
		void popIRDebugIndentation();


		void pushLoc(const Location& l);
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
		fir::Value* constructClassWithArguments(fir::ClassType* cls, sst::FunctionDefn* constr, const std::vector<FnCallArgument>& args);

		fir::Value* callVirtualMethod(sst::FunctionCall* call);
		CGResult getStructFieldImplicitly(std::string name);

		fir::Function* getOrDeclareLibCFunction(std::string name);


		bool isInsideGlobalInitFunc = false;

		// this one holds the finalised global initialiser function -- this one is supposed to
		// call all the pieces; we always regenerate this function when we call finishGlobalInitFunction().
		fir::Function* finalisedGlobalInitFunction = 0;

		// this is getting a bit complicated. this holds each "piece" of the global init function,
		// where each piece probably corresponds to the initialisation of a single global value.
		std::vector<std::pair<fir::GlobalValue*, fir::Function*>> globalInitPieces;

		bool isWithinGlobalInitFunction();
		fir::IRBlock* enterGlobalInitFunction(fir::GlobalValue* val);
		void leaveGlobalInitFunction(fir::IRBlock* restore);
		void finishGlobalInitFunction();





		void generateDecompositionBindings(const DecompMapping& bind, CGResult rhs, bool allowref);

		util::hash_map<std::string, size_t> getNameIndexMap(sst::FunctionDefn* fd);

		std::vector<fir::Value*> codegenAndArrangeFunctionCallArguments(sst::Defn* target, fir::FunctionType* ft, const std::vector<FnCallArgument>& args);



		void addVariableUsingStorage(sst::VarDefn* var, fir::Value* ptr);

		void createWhileLoop(const std::function<void (fir::IRBlock*, fir::IRBlock*)>& check, const std::function<void ()>& body);

		std::pair<OperatorFn, fir::Function*> getOperatorFunctionForTypes(fir::Type* a, fir::Type* b, std::string op);

		void performAssignment(fir::Value* lhs, fir::Value* rhs, bool isInitial);

		void addRAIIOrRCValueIfNecessary(fir::Value* val, fir::Type* typeOverride = 0);

		void addRAIIValue(fir::Value* val);
		void removeRAIIValue(fir::Value* val);
		std::vector<fir::Value*> getRAIIValues();

		sst::FunctionDefn* findMatchingMethodInType(sst::TypeDefn* td, sst::FunctionDecl* fn);

		bool isRAIIType(fir::Type* ty);
		bool typeHasDestructor(fir::Type* ty);
		bool typeHasCopyConstructor(fir::Type* ty);
		bool typeHasMoveConstructor(fir::Type* ty);

		void callDestructor(fir::Value* val);

		fir::Value* copyRAIIValue(fir::Value* value);
		void copyRAIIValue(fir::Value* from, fir::Value* target, bool enableMoving = true);
		void moveRAIIValue(fir::Value* from, fir::Value* target);
	};

	fir::Module* codegen(sst::DefinitionTree* dtree);
}







