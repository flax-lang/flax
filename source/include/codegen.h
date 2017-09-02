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
	struct StateTree;
	struct DefinitionTree;
}

namespace cgn
{
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

		void enterNamespace(std::string name);
		void leaveNamespace();

		CGResult performBinaryOperation(const Location& loc, std::pair<Location, CGResult> lhs, std::pair<Location, CGResult> rhs, Operator op);

		std::pair<CGResult, CGResult> autoCastValueTypes(const CGResult& lhs, const CGResult& rhs);
		CGResult oneWayAutocast(const CGResult& from, fir::Type* target);

		fir::Value* getDefaultValue(fir::Type* type);

		fir::ConstantValue* unwrapConstantNumber(fir::ConstantValue* cv);
		fir::ConstantValue* unwrapConstantNumber(mpfr::mpreal num, fir::Type* target);
	};

	fir::Module* codegen(sst::DefinitionTree* dtr);
}







