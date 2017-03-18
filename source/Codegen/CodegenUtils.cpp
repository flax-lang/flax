// LlvmCodeGen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>

#include <map>
#include <set>
#include <algorithm>

#include "pts.h"
#include "ast.h"
#include "codegen.h"
#include "operators.h"
#include "runtimefuncs.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	void doCodegen(std::string filename, Root* root, CodegenInstance* cgi)
	{
		iceAssert(cgi->module);
		cgi->rootNode = root;

		// todo: proper.
		if(sizeof(void*) == 8)
			cgi->module->setExecutionTarget(fir::ExecutionTarget::getLP64());

		else if(sizeof(void*) == 4)
			cgi->module->setExecutionTarget(fir::ExecutionTarget::getILP32());

		else
			error("enotsup: ptrsize = %zu", sizeof(void*));

		iceAssert(cgi->module->getExecutionTarget());
		cgi->pushScope();

		cgi->currentFuncTree = cgi->rootNode->rootFuncStack;

		// rootFuncStack should really be empty, except we know that there should be
		// stuff inside from imports.
		// thus, solidify the insides of these, by adding the function to fir::Module.

		// wtf is ^ talking about?
		// todo: specify whether a given import is private, or re-exporting.
		// ie. if A imports B, will B also get imported to something that imports A.

		// cgi->cloneFunctionTree(cgi->rootNode->rootFuncStack, cgi->rootNode->rootFuncStack, true);

		// auto p = prof::Profile("root codegen (" + Compiler::getFilenameFromPath(filename) + ")");
		cgi->rootNode->codegen(cgi);

		cgi->popScope();

		// free the memory
		cgi->clearScope();

		// this is all in ir-space. no scopes needed.
		cgi->finishGlobalConstructors();
	}



















	fir::FTContext* CodegenInstance::getContext()
	{
		auto c = fir::getDefaultFTContext();
		c->module = this->module;

		return fir::getDefaultFTContext();
	}

	typedef std::tuple<std::vector<SymTab_t>, std::vector<std::vector<fir::Value*>>, FunctionTree*> _Scope_t;

	_Scope_t CodegenInstance::saveAndClearScope()
	{
		auto s = std::make_tuple(this->symTabStack, this->refCountingStack, this->currentFuncTree);
		this->clearScope();

		return s;
	}

	void CodegenInstance::restoreScope(_Scope_t s)
	{
		std::tie(this->symTabStack, this->refCountingStack, this->currentFuncTree) = s;
	}




	void CodegenInstance::popScope()
	{
		this->symTabStack.pop_back();
		this->refCountingStack.pop_back();
	}

	void CodegenInstance::clearScope()
	{
		this->symTabStack.clear();
		this->refCountingStack.clear();
		this->currentFuncTree = this->rootNode->rootFuncStack;
	}

	void CodegenInstance::pushScope()
	{
		this->symTabStack.push_back(SymTab_t());
		this->refCountingStack.push_back({ });
	}

	void CodegenInstance::addRefCountedValue(fir::Value* val)
	{
		iceAssert(val->getType()->isPointerType() && "must refcount a pointer type");
		this->refCountingStack.back().push_back(val);
	}


	void CodegenInstance::removeRefCountedValue(fir::Value* val)
	{
		iceAssert(val->getType()->isPointerType() && "must refcount a pointer type");

		auto it = std::find(this->refCountingStack.back().begin(), this->refCountingStack.back().end(), val);
		if(it == this->refCountingStack.back().end())
			error("val does not exist in refcounting stack, cannot remove");

		this->refCountingStack.back().erase(it);
	}

	void CodegenInstance::removeRefCountedValueIfExists(fir::Value* val)
	{
		if(val == 0) return;

		iceAssert(val->getType()->isPointerType() && "must refcount a pointer type");

		auto it = std::find(this->refCountingStack.back().begin(), this->refCountingStack.back().end(), val);
		if(it == this->refCountingStack.back().end())
		{
			// error("does not exist");
			return;
		}

		this->refCountingStack.back().erase(it);
	}


	std::vector<fir::Value*> CodegenInstance::getRefCountedValues()
	{
		return this->refCountingStack.back();
	}


	Func* CodegenInstance::getCurrentFunctionScope()
	{
		return this->funcScopeStack.size() > 0 ? this->funcScopeStack.back() : 0;
	}

	void CodegenInstance::setCurrentFunctionScope(Func* f)
	{
		this->funcScopeStack.push_back(f);
	}

	void CodegenInstance::clearCurrentFunctionScope()
	{
		this->funcScopeStack.pop_back();
	}


	SymTab_t& CodegenInstance::getSymTab()
	{
		return this->symTabStack.back();
	}

	SymbolPair_t* CodegenInstance::getSymPair(Expr* user, const std::string& name)
	{
		for(int i = symTabStack.size(); i-- > 0;)
		{
			SymTab_t& tab = symTabStack[i];
			if(tab.find(name) != tab.end())
				return &(tab[name]);
		}

		return nullptr;
	}

	fir::Value* CodegenInstance::getSymInst(Expr* user, const std::string& name)
	{
		SymbolPair_t* pair = getSymPair(user, name);
		if(pair)
		{
			return pair->first;
		}

		return nullptr;
	}

	VarDecl* CodegenInstance::getSymDecl(Expr* user, const std::string& name)
	{
		SymbolPair_t* pair = nullptr;
		if((pair = getSymPair(user, name)))
			return pair->second;

		return nullptr;
	}

	bool CodegenInstance::isDuplicateSymbol(const std::string& name)
	{
		return getSymTab().find(name) != getSymTab().end();
	}

	void CodegenInstance::addSymbol(std::string name, fir::Value* ai, VarDecl* vardecl)
	{
		SymbolPair_t sp(ai, vardecl);
		this->getSymTab()[name] = sp;
	}

	void CodegenInstance::addNewType(fir::Type* ltype, StructBase* atype, TypeKind e)
	{
		TypePair_t tpair(ltype, TypedExpr_t(atype, e));

		// FunctionTree* ftree = this->currentFuncTree;
		// iceAssert(ftree);

		FunctionTree* ftree = this->getFuncTreeFromNS(atype->ident.scope);
		iceAssert(ftree);

		if(ftree->types.find(atype->ident.name) != ftree->types.end())
		{
			// only if there's an actual, fir::Type* there.
			if(ftree->types[atype->ident.name].first)
				error(atype, "Duplicate type %s (in ftree %s:%zu)", atype->ident.name.c_str(), ftree->nsName.c_str(), ftree->id);
		}

		// if there isn't one, add it.
		ftree->types[atype->ident.name] = tpair;


		if(this->typeMap.find(atype->ident.str()) == this->typeMap.end())
		{
			this->typeMap[atype->ident.str()] = tpair;
		}
		else
		{
			error(atype, "Duplicate type %s (full: %s)", atype->ident.name.c_str(), atype->ident.str().c_str());
		}

		TypeInfo::addNewType(this, ltype, atype, e);
	}


	TypePair_t* CodegenInstance::getType(const Identifier& ident)
	{
		return this->getTypeByString(ident.str());
	}


	TypePair_t* CodegenInstance::getTypeByString(std::string name)
	{
		if(name == "Inferred")
			return 0;


		if(this->typeMap.find(name) != this->typeMap.end())
			return &(this->typeMap[name]);


		// find nested types.
		if(this->nestedTypeStack.size() > 0)
		{
			StructBase* cls = this->nestedTypeStack.back();

			// only allow one level of implicit use
			for(auto n : cls->nestedTypes)
			{
				if(n.first->ident.name == name)
					return this->getType(n.second);
			}
		}


		// try generic types.
		{
			// this is somewhat complicated.
			// resolveGenericType returns an fir::Type*.
			// we need to return a TypePair_t* here. So... we should be able to "reverse-find"
			// the actual TypePair_t by calling the other version of getType(fir::Type*).

			// confused? source code explains better than I can.
			fir::Type* possibleGeneric = this->resolveGenericType(name);
			if(possibleGeneric)
			{
				if(this->isBuiltinType(possibleGeneric))
				{
					// create a typepair. allows constructor syntax
					// only applicable in generic functions.
					// todo(leak): this will leak...

					return new TypePair_t(possibleGeneric, std::make_pair(nullptr, TypeKind::BuiltinType));
				}
				else if(possibleGeneric->isParametricType())
				{
					// todo(leak): this leaks too
					return new TypePair_t(possibleGeneric, std::make_pair(nullptr, TypeKind::Parametric));
				}
				else if(possibleGeneric->isArrayType())
				{
					// all of this shit leaks
					return new TypePair_t(possibleGeneric, std::make_pair(nullptr, TypeKind::Array));
				}
				else if(possibleGeneric->isDynamicArrayType())
				{
					return new TypePair_t(possibleGeneric, std::make_pair(nullptr, TypeKind::Array));
				}
				else if(possibleGeneric->isTupleType())
				{
					return new TypePair_t(possibleGeneric, std::make_pair(nullptr, TypeKind::Tuple));
				}



				TypePair_t* tp = this->getType(possibleGeneric);
				iceAssert(tp);

				return tp;
			}
		}

		return 0;
	}

	TypePair_t* CodegenInstance::getType(fir::Type* type)
	{
		if(!type)
			return nullptr;

		for(auto pair : this->typeMap)
		{
			if(pair.second.first == type)
			{
				return &this->typeMap[pair.first];
			}
		}

		return nullptr;
	}

	bool CodegenInstance::isDuplicateType(const Identifier& id)
	{
		return this->getType(id) != nullptr;
	}

	void CodegenInstance::popBracedBlock()
	{
		this->blockStack.pop_back();
	}

	BracedBlockScope* CodegenInstance::getCurrentBracedBlockScope()
	{
		return this->blockStack.size() > 0 ? &this->blockStack.back() : 0;
	}

	void CodegenInstance::pushBracedBlock(BreakableBracedBlock* block, fir::IRBlock* body, fir::IRBlock* after)
	{
		BracedBlockScope cs = std::make_pair(block, std::make_pair(body, after));
		this->blockStack.push_back(cs);
	}




	void CodegenInstance::pushNestedTypeScope(StructBase* nest)
	{
		this->nestedTypeStack.push_back(nest);
	}

	void CodegenInstance::popNestedTypeScope()
	{
		iceAssert(this->nestedTypeStack.size() > 0);
		this->nestedTypeStack.pop_back();
	}

	std::vector<std::string> CodegenInstance::getFullScope()
	{
		return this->getNSFromFuncTree(this->getCurrentFuncTree());
	}

	std::vector<std::string> CodegenInstance::getNSFromFuncTree(FunctionTree* ftree)
	{
		std::vector<std::string> ret;
		auto bottom = ftree;

		while(bottom)
		{
			ret.insert(ret.begin(), bottom->nsName);
			bottom = bottom->parent;
		}

		for(auto s : this->nestedTypeStack)
			ret.push_back(s->ident.name);

		return ret;
	}






	// generic type stacks
	void CodegenInstance::pushGenericTypeStack()
	{
		auto newPart = std::map<std::string, fir::Type*>();
		this->instantiatedGenericTypeStack.push_back(newPart);
	}

	void CodegenInstance::pushGenericType(std::string id, fir::Type* type)
	{
		iceAssert(this->instantiatedGenericTypeStack.size() > 0);
		for(auto g : this->instantiatedGenericTypeStack.back())
		{
			if(g.first == id)
				error("Error: generic type %s already exists in the current stack frame", id.c_str());
		}

		this->instantiatedGenericTypeStack.back()[id] = type;
	}

	fir::Type* CodegenInstance::resolveGenericType(std::string id)
	{
		for(int i = this->instantiatedGenericTypeStack.size(); i-- > 0;)
		{
			auto& map = this->instantiatedGenericTypeStack[i];
			if(map.find(id) != map.end())
				return map[id];
		}

		return 0;
	}

	void CodegenInstance::popGenericTypeStack()
	{
		iceAssert(this->instantiatedGenericTypeStack.size() > 0);
		this->instantiatedGenericTypeStack.pop_back();
	}


	FunctionTree* CodegenInstance::getCurrentFuncTree()
	{
		return this->currentFuncTree;
	}













	void CodegenInstance::importOtherCgi(CodegenInstance* othercgi)
	{
		auto p = prof::Profile("importOtherCgi");
		this->importFunctionTreeInto(this->rootNode->rootFuncStack, othercgi->rootNode->rootFuncStack);
	}


	// static size_t seenCnt;
	// static std::set<FunctionTree*> seen;
	void CodegenInstance::importFunctionTreeInto(FunctionTree* ftree, FunctionTree* other)
	{
		// other is the source
		// ftree is the target

		// static size_t dc1 = 0;
		// static size_t dc2 = 0;
		// static size_t dc3 = 0;
		// static size_t dc4 = 0;

		// if(seen.find(other) != seen.end())
		// 	printf("seen %s (%zu)\n", other->nsName.c_str(), ++seenCnt);

		// else
		// 	seen.insert(other);

		// static size_t x = 0;

		// printf("import %zu ftrees\n", x++);
		ftree->nsName = other->nsName;
		{
			auto p = prof::Profile("import funcs");

			// printf("size: %zu\n", other->funcs.size());

			for(auto pair : other->funcs)
			{
				if(pair.funcDecl->attribs & Attr_VisPublic)
				{
					bool existing = false;


					if(pair.funcDecl->genericTypes.size() > 0)
					{
						// loop through
						for(auto f : ftree->funcs)
						{
							if(f.funcDecl == pair.funcDecl)
								existing = true;
						}
					}
					else
					{
						existing = ftree->funcSet.find(pair.firFunc->getName()) != ftree->funcSet.end();
					}

					if(!existing)
					{
						// printf("wtf? %s did not exist\n", pair.funcDecl->ident.str().c_str());

						iceAssert(pair.funcDecl);
						if(pair.funcDecl->genericTypes.size() == 0)
						{
							// declare new one
							auto f = this->module->getOrCreateFunction(pair.firFunc->getName(), pair.firFunc->getType(),
								fir::LinkageType::External);

							ftree->funcs.push_back(FuncDefPair(f, pair.funcDecl, pair.funcDef));
							ftree->funcSet.insert(f->getName());
						}
						else
						{
							// for generics, just push as-is
							ftree->funcs.push_back(FuncDefPair(pair.firFunc, pair.funcDecl, pair.funcDef));
							// ftree->funcSet.insert(pair.firFunc->getName());
						}
					}
				}
			}
		}



		{
			auto p = prof::Profile("import types");

			for(auto t : other->types)
			{
				bool existing = ftree->types.find(t.first) != ftree->types.end();

				if(!existing && t.first != "Type")
				{
					if(StructBase* sb = dynamic_cast<StructBase*>(t.second.second.first))
					{
						ftree->types[sb->ident.name] = t.second;
						this->typeMap[sb->ident.str()] = t.second;


						// check what kind of struct.
						if(StructDef* str = dynamic_cast<StructDef*>(sb))
						{
							if(str->attribs & Attr_VisPublic)
							{
								for(auto f : str->initFuncs)
									this->module->getOrCreateFunction(f->getName(), f->getType(), fir::LinkageType::External);
							}
						}
						else if(ClassDef* cls = dynamic_cast<ClassDef*>(sb))
						{
							if(cls->attribs & Attr_VisPublic)
							{
								for(auto f : cls->initFuncs)
								{
									this->module->getOrCreateFunction(f->getName(), f->getType(), fir::LinkageType::External);
								}

								for(auto ao : cls->assignmentOverloads)
								{
									this->module->getOrCreateFunction(ao->lfunc->getName(), ao->lfunc->getType(), fir::LinkageType::External);
								}

								for(auto so : cls->subscriptOverloads)
								{
									this->module->getOrCreateFunction(so->getterFunc->getName(), so->getterFunc->getType(),
										fir::LinkageType::External);

									if(so->setterFunc)
									{
										this->module->getOrCreateFunction(so->setterFunc->getName(), so->setterFunc->getType(),
											fir::LinkageType::External);
									}
								}

								for(auto fp : cls->functionMap)
								{
									if(fp.first->decl->attribs & Attr_VisPublic)
									{
										this->module->getOrCreateFunction(fp.second->getName(), fp.second->getType(), fir::LinkageType::External);
									}
								}
							}
						}
						else if(dynamic_cast<Tuple*>(sb))
						{
							// ignore
						}
						else if(dynamic_cast<EnumDef*>(sb))
						{
							// nothing
						}
						else
						{
							iceAssert(0);
						}
					}
				}
			}
		}





		{
			auto p = prof::Profile("import extensions");

			for(auto ext : other->extensions)
			{
				bool existing = false;
				for(auto e : ftree->extensions)
				{
					if(e.second == ext.second)
					{
						existing = true;
						break;
					}
				}

				if(!existing && ext.second->attribs & Attr_VisPublic)
				{
					ftree->extensions.insert(std::make_pair(ext.first, ext.second));

					ExtensionDef* ed = ext.second;
					for(auto& ff : ed->functionMap)
					{
						if(ff.first->decl->attribs & Attr_VisPublic)
						{
							this->module->getOrCreateFunction(ff.second->getName(), ff.second->getType(), fir::LinkageType::External);
						}
					}
				}
			}
		}





		{
			auto p = prof::Profile("import operators");

			for(auto oo : other->operators)
			{
				bool existing = false;
				for(auto ooc : ftree->operators)
				{
					if(oo->func->decl->ident == ooc->func->decl->ident)
					{
						existing = true;
						break;
					}
				}


				if(!existing && oo->attribs & Attr_VisPublic)
				{
					ftree->operators.push_back(oo);
				}
			}
		}





		{
			auto p = prof::Profile("import vars");

			for(auto var : other->vars)
			{
				if(var.second.second->attribs & Attr_VisPublic)
				{
					bool existing = ftree->vars.find(var.first) != ftree->vars.end();

					if(!existing && var.second.second->attribs & Attr_VisPublic)
					{
						// add to the module list
						// note: we're getting the ptr element type since the Value* stored is the allocated storage, which is a ptr.

						fir::GlobalVariable* potentialGV = this->module->tryGetGlobalVariable(var.second.second->ident);

						if(potentialGV == 0)
						{
							auto gv = this->module->declareGlobalVariable(var.second.second->ident,
								var.second.first->getType()->getPointerElementType(), var.second.second->immutable);

							ftree->vars[var.first] = SymbolPair_t(gv, var.second.second);
						}
						else
						{
							if(potentialGV->getType() != var.second.first->getType())
							{
								error(var.second.second, "Conflicting types for global variable %s: %s vs %s.",
									var.second.second->ident.str().c_str(), var.second.first->getType()->getPointerElementType()->str().c_str(),
									potentialGV->getType()->getPointerElementType()->str().c_str());
							}

							ftree->vars[var.first] = SymbolPair_t(potentialGV, var.second.second);
						}
					}
				}
			}
		}



		{
			auto p = prof::Profile("import generics");

			for(auto gf : other->genericFunctions)
			{
				bool existing = false;
				for(auto cgf : ftree->genericFunctions)
				{
					if(cgf.first == gf.first || cgf.second == gf.second)
					{
						existing = true;
						break;
					}
				}

				if(!existing && gf.first->attribs & Attr_VisPublic)
					ftree->genericFunctions.push_back(gf);
			}
		}





		{
			auto p = prof::Profile("import protocols");

			for(auto prot : other->protocols)
			{
				bool existing = false;
				for(auto cprot : ftree->protocols)
				{
					if(prot.first == cprot.first && prot.second != cprot.second)
					{
						error(prot.second, "conflicting protocols with the same name");
					}
					else if(prot.first == cprot.first && prot.second == cprot.second)
					{
						existing = true;
						break;
					}
				}

				if(!existing)
				{
					ftree->protocols[prot.first] = prot.second;
				}
			}
		}



		{
			// auto p = prof::Profile("import subs");

			// static size_t r = 0;
			// if(other->subs.size() > 0)
				// printf("recursively importing %zu subs (%zu)\n", other->subs.size(), r += other->subs.size());

			for(auto sub : other->subs)
			{
				FunctionTree* found = ftree->subMap[sub->nsName];

				if(found)
				{
					this->importFunctionTreeInto(found, sub);
				}
				else
				{
					auto nft = new FunctionTree(ftree);
					this->importFunctionTreeInto(nft, sub);

					ftree->subMap[sub->nsName] = nft;
					ftree->subs.push_back(nft);
				}
			}
		}

		// printf("checks: %zu / %zu / %zu / %zu\n", dc1, dc2, dc3, dc4);
	}



	// FunctionTree* CodegenInstance::getCurrentFuncTree(std::vector<std::string>* nses, FunctionTree* root)
	// {
	// 	if(root == 0) root = this->rootNode->rootFuncStack;
	// 	if(nses == 0) nses = &this->namespaceStack;

	// 	iceAssert(root);
	// 	iceAssert(nses);

	// 	std::vector<FunctionTree*> ft = root->subs;

	// 	if(nses->size() == 0) return root;

	// 	size_t i = 0;
	// 	size_t max = nses->size();

	// 	for(auto ns : *nses)
	// 	{
	// 		i++;

	// 		bool found = false;
	// 		for(auto f : ft)
	// 		{
	// 			if(f->nsName == ns)
	// 			{
	// 				ft = f->subs;

	// 				if(i == max)
	// 					return f;

	// 				found = true;
	// 				break;
	// 			}
	// 		}

	// 		if(!found)
	// 		{
	// 			return 0;
	// 		}
	// 	}

	// 	return 0;
	// }












	void CodegenInstance::pushNamespaceScope(std::string namespc)
	{
		FunctionTree* existing = this->currentFuncTree;
		if(existing->subMap.find(namespc) == existing->subMap.end())
		{
			FunctionTree* ft = new FunctionTree(existing);
			ft->nsName = namespc;

			existing->subs.push_back(ft);
			existing->subMap[namespc] = ft;
		}

		this->currentFuncTree = existing->subMap[namespc];
	}

	void CodegenInstance::popNamespaceScope()
	{
		iceAssert(this->currentFuncTree->parent);
		this->currentFuncTree = this->currentFuncTree->parent;
	}

	void CodegenInstance::addFunctionToScope(FuncDefPair func, FunctionTree* root)
	{
		FunctionTree* cur = root;
		if(!cur) cur = this->currentFuncTree;

		iceAssert(cur);

		for(auto& fp : cur->funcs)
		{
			if(fp.firFunc == 0 && fp.funcDecl == func.funcDecl)
			{
				fp.firFunc = func.firFunc;
				cur->funcSet.insert(fp.firFunc->getName());
				return;
			}
			else if(fp.firFunc == func.firFunc && fp.funcDef == func.funcDef)
			{
				return;
			}
		}

		cur->funcs.push_back(func);

		if(func.firFunc) cur->funcSet.insert(func.firFunc->getName());
	}

	void CodegenInstance::removeFunctionFromScope(FuncDefPair func)
	{
		FunctionTree* cur = this->currentFuncTree;
		iceAssert(cur);

		auto it = std::find(cur->funcs.begin(), cur->funcs.end(), func);
		if(it != cur->funcs.end())
			cur->funcs.erase(it);

		if(func.firFunc)
		{
			auto it = cur->funcSet.find(func.firFunc->getName());
			if(it != cur->funcSet.end())
				cur->funcSet.erase(it);
		}
	}











	std::vector<FuncDefPair> CodegenInstance::resolveFunctionName(std::string basename)
	{
		FunctionTree* curFT = this->currentFuncTree;
		std::vector<FuncDefPair> candidates;


		auto _isDupe = [this](FuncDefPair a, FuncDefPair b) -> bool {

			if(a.firFunc == 0 || b.firFunc == 0)
			{
				iceAssert(a.funcDecl);
				iceAssert(b.funcDecl);

				if(a.funcDecl->params.size() != b.funcDecl->params.size()) return false;
				if(a.funcDecl->genericTypes.size() > 0 && b.funcDecl->genericTypes.size() > 0)
				{
					if(a.funcDecl->genericTypes != b.funcDecl->genericTypes) return false;
				}

				for(size_t i = 0; i < a.funcDecl->params.size(); i++)
				{
					// allowFail = true
					if(a.funcDecl->params[i]->getType(this, 0, true) != b.funcDecl->params[i]->getType(this, 0, true))
						return false;
				}

				return true;
			}
			else if(a.firFunc == b.firFunc || a.funcDecl == b.funcDecl)
			{
				return true;
			}
			else
			{
				return a.firFunc->getType() == b.firFunc->getType();
			}
		};



		while(curFT)
		{
			for(auto f : curFT->funcs)
			{
				auto isDupe = [this, f, _isDupe](FuncDefPair fp) -> bool {
					return _isDupe(f, fp);
				};

				if(f.funcDecl->genericTypes.size() == 0
					&& (f.funcDecl ? f.funcDecl->ident.name : f.firFunc->getName().str()) == basename)
				{
					if(std::find_if(candidates.begin(), candidates.end(), isDupe) == candidates.end())
						candidates.push_back(f);
				}
			}

			curFT = curFT->parent;
		}

		return candidates;
	}




	static Resolved_t _doTheResolveFunction(CodegenInstance* cgi, Expr* user, std::string basename, std::vector<fir::Type*> givenParams,
		std::vector<std::pair<FuncDefPair, int>> finals)
	{
		// disambiguate this.
		// with casting distance.
		if(finals.size() > 1)
		{
			// go through each.
			std::vector<std::pair<FuncDefPair, int>> mostViable;
			for(auto f : finals)
			{
				if(mostViable.size() == 0 || mostViable.front().second > f.second)
				{
					mostViable.clear();
					mostViable.push_back(f);
				}
				else if(mostViable.size() > 0 && mostViable.front().second == f.second)
				{
					mostViable.push_back(f);
				}
			}

			if(mostViable.size() == 1)
			{
				return Resolved_t(mostViable.front().first);
			}
			else
			{
				// parameters
				std::string pstr;
				for(auto e : givenParams)
					pstr += e->str() + ", ";

				if(givenParams.size() > 0)
					pstr = pstr.substr(0, pstr.size() - 2);

				// candidates
				std::string cstr;
				for(auto c : finals)
				{
					if(c.first.funcDecl)
						cstr += cgi->printAst(c.first.funcDecl) + "\n";
				}

				error(user, "Ambiguous function call to function %s with parameters: (%s), have %zu candidates:\n%s",
					basename.c_str(), pstr.c_str(), finals.size(), cstr.c_str());
			}
		}
		else if(finals.size() == 0)
		{
			return Resolved_t();
		}

		return Resolved_t(finals.front().first);
	}



	Resolved_t CodegenInstance::resolveFunctionFromList(Expr* user, std::vector<FuncDefPair> list, std::string basename,
		std::vector<Expr*> params, bool exactMatch)
	{
		std::vector<FuncDefPair> candidates = list;
		if(candidates.size() == 0) return Resolved_t();

		std::vector<fir::Type*> prms;

		std::vector<std::pair<FuncDefPair, int>> finals;
		for(auto c : candidates)
		{
			int distance = 0;

			// note: if we don't provide the FuncDecl, assume we have everything down, including the basename.
			if((c.funcDecl ? c.funcDecl->ident.name : basename) == basename && this->isValidFuncOverload(c, params, &distance,
				exactMatch, &prms))
			{
				finals.push_back({ c, distance });
			}
		}

		return _doTheResolveFunction(this, user, basename, prms, finals);
	}

	Resolved_t CodegenInstance::resolveFunctionFromList(Expr* user, std::vector<FuncDefPair> list, std::string basename,
		std::vector<fir::Type*> params, bool exactMatch)
	{
		std::vector<FuncDefPair> candidates = list;
		if(candidates.size() == 0) return Resolved_t();

		std::vector<std::pair<FuncDefPair, int>> finals;
		for(auto c : candidates)
		{
			int distance = 0;

			// note: if we don't provide the FuncDecl, assume we have everything down, including the basename.
			if((c.funcDecl ? c.funcDecl->ident.name : basename) == basename && this->isValidFuncOverload(c, params, &distance, exactMatch))
				finals.push_back({ c, distance });
		}

		return _doTheResolveFunction(this, user, basename, params, finals);
	}

	Resolved_t CodegenInstance::resolveFunction(Expr* user, std::string basename, std::vector<Expr*> params, bool exactMatch)
	{
		std::vector<FuncDefPair> candidates = this->resolveFunctionName(basename);
		return this->resolveFunctionFromList(user, candidates, basename, params, exactMatch);
	}






	std::vector<Func*> CodegenInstance::findGenericFunctions(std::string basename)
	{
		FunctionTree* curFT = this->currentFuncTree;
		std::vector<Func*> ret;

		while(curFT)
		{
			for(auto f : curFT->genericFunctions)
			{
				iceAssert(f.first->genericTypes.size() > 0);

				if(f.first->ident.name == basename)
					ret.push_back({ f.second });
			}

			curFT = curFT->parent;
		}

		return ret;
	}






	static bool _checkGenericFunctionParameter(CodegenInstance* cgi, fir::FunctionType* gen, fir::FunctionType* given)
	{
		typedef std::vector<pts::TypeTransformer> TrfList;

		// ok, type solving time.
		auto alist = gen->getArgumentTypes();
		auto glist = given->getArgumentTypes();

		auto ft1 = gen;
		auto ft2 = given;

		// basic things
		if(ft1->isVariadicFunc() != ft2->isVariadicFunc() || ft1->isCStyleVarArg() != ft2->isCStyleVarArg())
			return false;

		if(glist.size() != alist.size())
			return false;


		std::map<std::string, fir::Type*> gtm;
		for(size_t k = 0; k < alist.size(); k++)
		{
			// really want structured bindings right about now
			fir::Type* givent = 0; TrfList gtrfs;
			std::tie(givent, gtrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(glist[k]);

			fir::Type* expt = 0; TrfList etrfs;
			std::tie(expt, gtrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(alist[k]);

			if(expt->isParametricType())
			{
				if(!pts::areTransformationsCompatible(etrfs, gtrfs))
					return false;

				if(gtm.find(expt->toParametricType()->getName()) != gtm.end())
				{
					if(givent != gtm[expt->toParametricType()->getName()])
						return false;
				}
				else
				{
					gtm[expt->toParametricType()->getName()] = givent;
				}
			}
			else if(expt->isTupleType() || expt->isFunctionType())
			{
				error("not sup nested");
			}
			else
			{
				// regardless of 'exact' -- function types must always match exactly.
				if(glist[k] != alist[k])
					return false;
			}
		}

		return true;
	}


	static bool _checkFunction(CodegenInstance* cgi, std::vector<Expr*> exprs, std::vector<fir::Type*> funcParams,
		std::vector<fir::Type*> args, int* _dist, bool variadic, bool c_variadic, bool exact)
	{
		iceAssert(_dist);
		*_dist = 0;

		// fuck, we need to be able to check passing generic functions to non-generic functions
		// non-generic functions should be perfectly able to take generic functions, because all the
		// information is there to give solutions.

		// hopefully it'll be much less complex than actual generic function resolution,
		// since we know pretty much all the information immediately, and can return false if it's wrong.

		if(!variadic)
		{
			if(funcParams.size() != args.size() && !c_variadic) return false;
			if(funcParams.size() == 0 && (args.size() == 0 || c_variadic)) return true;

			#define __min(x, y) ((x) > (y) ? (y) : (x))
			for(size_t i = 0; i < __min(args.size(), funcParams.size()); i++)
			{
				fir::Type* t1 = args[i];
				fir::Type* t2 = funcParams[i];

				if(t1->isFunctionType() && t2->isFunctionType() && t1->toFunctionType()->isGenericFunction())
				{
					bool res = _checkGenericFunctionParameter(cgi, t1->toFunctionType(), t2->toFunctionType());
					if(!res) return false;
				}
				else if(t1 != t2)
				{
					if(exact || t1 == 0 || t2 == 0) return false;

					// try to cast.
					int dist = cgi->getAutoCastDistance(t1, t2);
					if(dist == -1) return false;

					*_dist += dist;
				}
			}

			return true;
		}
		else
		{
			// variadic.
			// check until the last parameter.

			// 1. passed parameters must have at least all the fixed parameters, since the varargs can be 0-length.
			if(args.size() < funcParams.size() - 1) return false;

			// 2. check the fixed parameters
			for(size_t i = 0; i < funcParams.size() - 1; i++)
			{
				fir::Type* t1 = args[i];
				fir::Type* t2 = funcParams[i];

				if(t1->isFunctionType() && t2->isFunctionType() && t1->toFunctionType()->isGenericFunction())
				{
					bool res = _checkGenericFunctionParameter(cgi, t1->toFunctionType(), t2->toFunctionType());
					if(!res) return false;
				}
				else if(t1 != t2)
				{
					if(exact || t1 == 0 || t2 == 0) return false;

					// try to cast.
					int dist = cgi->getAutoCastDistance(t1, t2);
					if(dist == -1) return false;

					*_dist += dist;
				}
			}

			// check for direct forwarding case
			if(args.size() == funcParams.size())
			{
				if(args.back()->isDynamicArrayType()
					&& funcParams.back()->isDynamicArrayType() && funcParams.back()->toDynamicArrayType()->isFunctionVariadic()
					&& args.back()->toDynamicArrayType()->getElementType() == funcParams.back()->toDynamicArrayType()->getElementType())
				{
					// yes, do that (where that == nothing)
					*_dist += 0;
					return true;
				}
			}




			// 3. get the type of the vararg array.
			fir::Type* funcLLType = funcParams.back();
			iceAssert(funcLLType->isDynamicArrayType());

			fir::Type* llElmType = funcLLType->toDynamicArrayType()->getElementType();

			// 4. check the variable args.
			for(size_t i = funcParams.size() - 1; i < args.size(); i++)
			{
				fir::Type* argType = args[i];

				if(llElmType != argType)
				{
					if(exact || argType == 0) return false;

					// try to cast.
					int dist = cgi->getAutoCastDistance(argType, llElmType);
					if(dist == -1) return false;

					*_dist += dist;
				}
			}

			return true;
		}
	}






	bool CodegenInstance::isValidFuncOverload(FuncDefPair fp, std::vector<fir::Type*> argTypes, int* castingDistance, bool exactMatch)
	{
		iceAssert(castingDistance);
		std::vector<fir::Type*> funcParams;


		bool iscvar = 0;
		bool isvar = 0;

		if(fp.firFunc)
		{
			for(auto arg : fp.firFunc->getArguments())
				funcParams.push_back(arg->getType());

			iscvar = fp.firFunc->isCStyleVarArg();
			isvar = fp.firFunc->isVariadic();
		}
		else
		{
			iceAssert(fp.funcDecl);

			for(auto arg : fp.funcDecl->params)
			{
				auto t = arg->getType(this, 0, true);
				if(!t) return false;

				funcParams.push_back(t);
			}

			iscvar = fp.funcDecl->isCStyleVarArg;
			isvar = fp.funcDecl->isVariadic;
		}

		std::vector<Expr*> exprl;
		if(fp.funcDecl)
		{
			for(auto p : fp.funcDecl->params)
				exprl.push_back(p);
		}

		return _checkFunction(this, exprl, funcParams, argTypes, castingDistance, isvar, iscvar, exactMatch);
	}



	bool CodegenInstance::isValidFuncOverload(FuncDefPair fp, std::vector<Expr*> params, int* castingDistance, bool exactMatch,
		std::vector<fir::Type*>* resolvedTypes)
	{
		std::vector<fir::Type*> expectedTypes;
		if(fp.firFunc)
		{
			expectedTypes = fp.firFunc->getType()->getArgumentTypes();
		}
		else
		{
			iceAssert(fp.funcDecl);

			for(auto arg : fp.funcDecl->params)
			{
				auto t = arg->getType(this, 0, true);
				if(!t) return false;

				expectedTypes.push_back(t);
			}
		}

		std::vector<fir::Type*> prms;

		for(size_t i = 0; i < params.size(); i++)
			prms.push_back(params[i]->getType(this, i >= expectedTypes.size() ? expectedTypes.back() : expectedTypes[i]));

		*resolvedTypes = prms;

		return this->isValidFuncOverload(fp, prms, castingDistance, exactMatch);
	}


















	void CodegenInstance::clearNamespaceScope()
	{
		this->currentFuncTree = this->rootNode->rootFuncStack;
	}



	fir::Function* CodegenInstance::getOrDeclareLibCFunc(std::string name)
	{
		if(name == ALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(ALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64() }, fir::Type::getInt8Ptr(), false), fir::LinkageType::External);
		}
		else if(name == FREE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(FREE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getVoid(), false), fir::LinkageType::External);
		}
		else if(name == REALLOCATE_MEMORY_FUNC)
		{
			return this->module->getOrCreateFunction(Identifier(REALLOCATE_MEMORY_FUNC, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr(), fir::Type::getInt64() }, fir::Type::getInt8Ptr(), false), fir::LinkageType::External);
		}
		else if(name == "printf")
		{
			return this->module->getOrCreateFunction(Identifier("printf", IdKind::Name),
				fir::FunctionType::getCVariadicFunc({ fir::Type::getInt8Ptr() }, fir::Type::getInt32()), fir::LinkageType::External);
		}
		else if(name == "abort")
		{
			return this->module->getOrCreateFunction(Identifier("abort", IdKind::Name),
				fir::FunctionType::get({ }, fir::Type::getVoid(), false), fir::LinkageType::External);
		}
		else if(name == "strlen")
		{
			return this->module->getOrCreateFunction(Identifier("strlen", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getInt64(), false), fir::LinkageType::External);
		}
		else
		{
			error("enotsup: %s", name.c_str());
		}
	}












	std::vector<std::string> CodegenInstance::unwrapNamespacedType(std::string raw)
	{
		iceAssert(raw.size() > 0);
		if(raw.find(".") == std::string::npos)
		{
			return { raw };
		}
		else if(raw.front() == '(')
		{
			error("enotsup");
		}
		else if(raw.front() == '[')
		{
			error("enotsup");
		}

		// else
		std::vector<std::string> nses;
		while(true)
		{
			size_t pos = raw.find(".");
			if(pos == std::string::npos) break;

			std::string ns = raw.substr(0, pos);
			nses.push_back(ns);

			raw = raw.substr(pos + 1);
		}

		if(raw.size() > 0)
			nses.push_back(raw);

		return nses;
	}




	FunctionTree* CodegenInstance::getFuncTreeFromNS(std::vector<std::string> scope)
	{
		auto ret = this->rootNode->rootFuncStack;
		if(scope.size() > 0 && scope[0] == "")
			scope.erase(scope.begin());

		for(auto s : scope)
		{
			ret = ret->subMap[s];
			if(!ret) return 0;
		}

		return ret;
	}































	ProtocolDef* CodegenInstance::resolveProtocolName(Expr* user, std::string protstr)
	{
		std::vector<std::string> nses = this->unwrapNamespacedType(protstr);
		std::string protname = nses.back();
		nses.pop_back();

		ProtocolDef* prot = 0;
		auto curFT = this->getFuncTreeFromNS(nses);

		while(curFT)
		{
			for(auto& f : curFT->protocols)
			{
				if(f.first == protname)
				{
					prot = f.second;
					break;
				}
			}

			curFT = curFT->parent;
		}

		if(!prot)
			error(user, "Undeclared protocol '%s'", protname.c_str());

		return prot;
	}






















































	ArithmeticOp CodegenInstance::determineArithmeticOp(std::string ch)
	{
		return Parser::mangledStringToOperator(this, ch);
	}







	bool CodegenInstance::isArithmeticOpAssignment(Ast::ArithmeticOp op)
	{
		// note: why is this a switch?
		// answer: because multiple cursor editing wins.
		switch(op)
		{
			case ArithmeticOp::Assign:				return true;
			case ArithmeticOp::PlusEquals:			return true;
			case ArithmeticOp::MinusEquals:			return true;
			case ArithmeticOp::MultiplyEquals:		return true;
			case ArithmeticOp::DivideEquals:		return true;
			case ArithmeticOp::ModEquals:			return true;
			case ArithmeticOp::ShiftLeftEquals:		return true;
			case ArithmeticOp::ShiftRightEquals:	return true;
			case ArithmeticOp::BitwiseAndEquals:	return true;
			case ArithmeticOp::BitwiseOrEquals:		return true;
			case ArithmeticOp::BitwiseXorEquals:	return true;

			default: return false;
		}
	}








	static std::vector<ExtensionDef*> _findExtensionsByNameInScope(std::string name, FunctionTree* ft)
	{
		std::vector<ExtensionDef*> ret;
		iceAssert(ft);

		while(ft)
		{
			auto pair = ft->extensions.equal_range(name);
			for(auto it = pair.first; it != pair.second; it++)
			{
				if((*it).first == name)
					ret.push_back((*it).second);
			}

			ft = ft->parent;
		}

		return ret;
	}

	std::vector<ExtensionDef*> CodegenInstance::getExtensionsWithName(std::string name)
	{
		FunctionTree* ft = this->currentFuncTree;
		iceAssert(ft);

		return _findExtensionsByNameInScope(name, ft);
	}

	std::vector<ExtensionDef*> CodegenInstance::getExtensionsForType(StructBase* cls)
	{
		std::set<ExtensionDef*> ret;

		// 1. look in the current scope
		{
			std::vector<ExtensionDef*> res = _findExtensionsByNameInScope(cls->ident.name, this->currentFuncTree);
			ret.insert(res.begin(), res.end());
		}


		// 2. look in the scope of the type
		{
			auto curFT = this->getFuncTreeFromNS(cls->ident.scope);

			while(curFT)
			{
				std::vector<ExtensionDef*> res = _findExtensionsByNameInScope(cls->ident.name, curFT);
				ret.insert(res.begin(), res.end());

				curFT = curFT->parent;
			}
		}

		return std::vector<ExtensionDef*>(ret.begin(), ret.end());
	}

	std::vector<ExtensionDef*> CodegenInstance::getExtensionsForBuiltinType(fir::Type* type)
	{
		if(fir::Type::fromBuiltin(INT8_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT8_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT16_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT16_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT32_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT32_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT64_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT64_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT128_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT128_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT8_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT8_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT16_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT16_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT32_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT32_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT64_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT64_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT128_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT128_TYPE_STRING);

		else if(fir::Type::fromBuiltin(FLOAT32_TYPE_STRING) == type)
			return this->getExtensionsWithName(FLOAT32_TYPE_STRING);

		else if(fir::Type::fromBuiltin(FLOAT64_TYPE_STRING) == type)
			return this->getExtensionsWithName(FLOAT64_TYPE_STRING);

		else if(fir::Type::fromBuiltin(FLOAT80_TYPE_STRING) == type)
			return this->getExtensionsWithName(FLOAT80_TYPE_STRING);

		else if(fir::Type::fromBuiltin(BOOL_TYPE_STRING) == type)
			return this->getExtensionsWithName(BOOL_TYPE_STRING);

		else if(fir::Type::fromBuiltin(STRING_TYPE_STRING) == type)
			return this->getExtensionsWithName(STRING_TYPE_STRING);

		else if(fir::Type::fromBuiltin(CHARACTER_TYPE_STRING) == type)
			return this->getExtensionsWithName(CHARACTER_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UNICODE_STRING_TYPE_STRING) == type)
			return this->getExtensionsWithName(UNICODE_STRING_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UNICODE_CHARACTER_TYPE_STRING) == type)
			return this->getExtensionsWithName(UNICODE_CHARACTER_TYPE_STRING);

		else
			return { };
	}










	fir::Function* CodegenInstance::getStructInitialiser(Expr* user, TypePair_t* pair, std::vector<fir::Value*> vals)
	{
		// check if this is a builtin type.
		// allow constructor syntax for that
		// eg. let x = Int64(100).
		// sure, this is stupid, but allows for generic 'K' or 'T' that
		// resolves to Int32 or something.

		if(this->isBuiltinType(pair->first))
		{
			iceAssert(pair->second.first == 0);

			if(vals.size() == 1)
			{
				std::vector<fir::Type*> args { pair->first->getPointerTo() };
				fir::FunctionType* ft = fir::FunctionType::get(args, pair->first, false);

				Identifier fnId = Identifier("__builtin_primitive_init_default_" + pair->first->encodedStr(), IdKind::AutoGenFunc);
				fnId.functionArguments = ft->getArgumentTypes();

				fir::Function* fn = this->module->getOrCreateFunction(fnId, ft, fir::LinkageType::Internal);


				if(vals[0]->getType()->isPointerType() && vals[0]->getType()->getPointerElementType()->isStringType())
				{
					if(fn->getBlockList().size() == 0)
					{
						fir::IRBlock* prevBlock = this->irb.getCurrentBlock();

						fir::IRBlock* block = this->irb.addNewBlockInFunction("entry", fn);
						this->irb.setCurrentBlock(block);

						// fir::Value* param = ++fn->arg_begin();
						fir::Value* empty = this->getEmptyString().value;

						this->irb.CreateStore(empty, fn->getArguments()[0]);

						this->irb.CreateReturn(empty);
						this->irb.setCurrentBlock(prevBlock);
					}
				}
				else
				{
					if(fn->getBlockList().size() == 0)
					{
						fir::IRBlock* prevBlock = this->irb.getCurrentBlock();

						fir::IRBlock* block = this->irb.addNewBlockInFunction("entry", fn);
						this->irb.setCurrentBlock(block);

						// fir::Value* param = ++fn->arg_begin();
						this->irb.CreateReturn(fir::ConstantValue::getZeroValue(pair->first));

						this->irb.setCurrentBlock(prevBlock);
					}
				}


				if(vals.size() != fn->getArgumentCount())
					GenError::invalidInitialiser(this, user, pair->first->str(), vals);

				for(size_t i = 0; i < fn->getArgumentCount(); i++)
				{
					if(vals[i]->getType() != fn->getArguments()[i]->getType())
						GenError::invalidInitialiser(this, user, pair->first->str(), vals);
				}

				return fn;
			}
			else if(vals.size() == 2)
			{
				std::vector<fir::Type*> args { pair->first->getPointerTo(), pair->first };
				fir::FunctionType* ft = fir::FunctionType::get(args, pair->first, false);

				Identifier fnId = Identifier("__builtin_primitive_init_" + pair->first->encodedStr(), IdKind::AutoGenFunc);
				fnId.functionArguments = ft->getArgumentTypes();

				fir::Function* fn = this->module->declareFunction(fnId, ft);

				if(fn->getBlockList().size() == 0)
				{
					fir::IRBlock* prevBlock = this->irb.getCurrentBlock();

					fir::IRBlock* block = this->irb.addNewBlockInFunction("entry", fn);
					this->irb.setCurrentBlock(block);

					iceAssert(fn->getArgumentCount() > 1);

					// fir::Value* param = ++fn->arg_begin();
					this->irb.CreateReturn(fn->getArguments()[1]);

					this->irb.setCurrentBlock(prevBlock);
				}


				if(vals.size() != fn->getArgumentCount())
					GenError::invalidInitialiser(this, user, pair->first->str(), vals);

				for(size_t i = 0; i < fn->getArgumentCount(); i++)
				{
					if(vals[i]->getType() != fn->getArguments()[i]->getType())
						GenError::invalidInitialiser(this, user, pair->first->str(), vals);
				}


				return fn;
			}
			else
			{
				error(user, "Constructor syntax for builtin types requires either 0 or 1 parameters.");
			}
		}

		if(pair->second.second == TypeKind::TypeAlias)
		{
			// iceAssert(pair->second.second == TypeKind::TypeAlias);
			// TypeAlias* ta = dynamic_cast<TypeAlias*>(pair->second.first);
			// iceAssert(ta);

			// TypePair_t* tp = this->getTypeByString(ta->origType);
			// iceAssert(tp);

			// return this->getStructInitialiser(user, tp, vals);

			iceAssert(0);
		}
		else if(pair->second.second == TypeKind::Class || pair->second.second == TypeKind::Struct)
		{
			StructBase* sb = dynamic_cast<StructBase*>(pair->second.first);
			iceAssert(sb);

			// use function overload operator for this.

			std::vector<FuncDefPair> fns;
			for(auto f : sb->initFuncs)
				fns.push_back(FuncDefPair(f, 0, 0));

			std::vector<ExtensionDef*> exts = this->getExtensionsForType(sb);
			for(auto ext : exts)
			{
				// note: check that either it's a public extension, *or* we're in the same rootnode.
				// naturally private extensions can still be used in the same file, if not they'd be useless
				for(auto f : ext->initFuncs)
				{
					Func* func = 0;
					for(auto fn : ext->functionMap)
					{
						if(fn.second == f)
							func = fn.first;
					}
					iceAssert(func);

					if(func->decl->attribs & Attr_VisPublic || ext->parentRoot == this->rootNode)
					{
						fns.push_back(FuncDefPair(f, 0, 0));
					}
				}
			}

			std::vector<fir::Type*> argTypes;
			for(auto v : vals)
				argTypes.push_back(v->getType());

			Resolved_t res = this->resolveFunctionFromList(user, fns, "init", argTypes);

			if(!res.resolved)
			{
				std::string argstr;
				for(auto a : argTypes)
					argstr += a->str() + ", ";

				if(argTypes.size() > 0)
					argstr = argstr.substr(0, argstr.size() - 2);

				error(user, "No initialiser for type '%s' taking parameters (%s)", sb->ident.name.c_str(), argstr.c_str());
			}

			auto ret = this->module->getFunction(res.t.firFunc->getName());
			iceAssert(ret);

			return ret;
		}
		else
		{
			error(user, "Invalid expr type");
		}
	}








	fir::Function* CodegenInstance::getFunctionFromModuleWithName(const Identifier& id, Expr* user)
	{
		auto list = this->module->getFunctionsWithName(id);
		if(list.empty()) return 0;

		else if(list.size() > 1)
			error(user, "Searched for ambiguous function by name '%s'", id.str().c_str());

		return list.front();
	}

	fir::Function* CodegenInstance::getFunctionFromModuleWithNameAndType(const Identifier& id, fir::FunctionType* ft, Expr* user)
	{
		auto list = this->module->getFunctionsWithName(id);

		if(list.empty())
		{
			error(user, "Using undeclared function '%s'", id.str().c_str());
		}
		else if(list.size() == 1)
		{
			if(list.front()->getType() == ft)
				return list.front();

			else
				error(user, "Found unambiguous function with name '%s', but mismatched type '%s'", id.str().c_str(), ft->str().c_str());
		}

		// more than one.
		std::vector<fir::Function*> ret;

		for(auto f : list)
		{
			if(f->getType() == ft)
				ret.push_back(f);
		}

		if(ret.size() == 0)
			error(user, "No function with name '%s', matching type '%s'", id.str().c_str(), ft->str().c_str());

		else if(ret.size() > 1)
			error(user, "Ambiguous functions with name '%s', matching type '%s' (HOW??)", id.str().c_str(), ft->str().c_str());

		else
			return ret[0];
	}












	void CodegenInstance::performComplexValueStore(Expr* user, fir::Type* elmType, fir::Value* src, fir::Value* dst, std::string name,
		Parser::Pin pos, ValueKind rhsvk)
	{
		auto cmplxtype = this->getType(elmType);
		if(cmplxtype)
		{
			// todo: this leaks also
			Operators::performActualAssignment(this, user, new VarRef(pos, name),
				0, ArithmeticOp::Assign, this->irb.CreateLoad(dst), dst, ValueKind::LValue, this->irb.CreateLoad(src), src, rhsvk);

			// it's stored already, no need to do shit.
			// iceAssert(res.value);
		}
		else
		{
			// ok, just do it normally
			this->irb.CreateStore(this->irb.CreateLoad(src), dst);
		}

		if(this->isRefCountedType(elmType))
		{
			// (isInit = true, doAssign = false -- we already assigned it above)
			this->assignRefCountedExpression(new VarRef(pos, name), this->irb.CreateLoad(src), src,
				this->irb.CreateLoad(dst), dst, rhsvk, true, false);
		}
	}







	Result_t CodegenInstance::makeStringLiteral(std::string str)
	{
		auto cs = fir::ConstantString::get(str);
		return Result_t(cs, 0);
	}

	Result_t CodegenInstance::getEmptyString()
	{
		return this->makeStringLiteral("");
	}

	Result_t CodegenInstance::getNullString()
	{
		return this->makeStringLiteral("(null)");
	}








	static bool isStructuredAggregate(fir::Type* t)
	{
		return t->isStructType() || t->isClassType() || t->isTupleType();
	}


	template <typename T>
	void doRefCountOfAggregateType(CodegenInstance* cgi, T* type, fir::Value* value, bool incr)
	{
		// iceAssert(cgi->isRefCountedType(type));

		size_t i = 0;
		for(auto m : type->getElements())
		{
			if(cgi->isRefCountedType(m))
			{
				fir::Value* mem = cgi->irb.CreateExtractValue(value, { i });

				if(incr)	cgi->incrementRefCount(mem);
				else		cgi->decrementRefCount(mem);
			}
			else if(isStructuredAggregate(m))
			{
				fir::Value* mem = cgi->irb.CreateExtractValue(value, { i });

				if(m->isStructType())		doRefCountOfAggregateType(cgi, m->toStructType(), mem, incr);
				else if(m->isClassType())	doRefCountOfAggregateType(cgi, m->toClassType(), mem, incr);
				else if(m->isTupleType())	doRefCountOfAggregateType(cgi, m->toTupleType(), mem, incr);
			}

			i++;
		}
	}

	static void _doRefCount(CodegenInstance* cgi, fir::Value* ptr, bool incr)
	{
		if(ptr->getType()->isStringType())
		{
			fir::Function* rf = 0;
			if(incr) rf = RuntimeFuncs::String::getRefCountIncrementFunction(cgi);
			else rf = RuntimeFuncs::String::getRefCountDecrementFunction(cgi);

			cgi->irb.CreateCall1(rf, ptr);
		}
		else if(ptr->getType()->isAnyType())
		{
			fir::Function* rf = 0;
			if(incr) rf = RuntimeFuncs::Any::getRefCountIncrementFunction(cgi);
			else rf = RuntimeFuncs::Any::getRefCountDecrementFunction(cgi);

			cgi->irb.CreateCall1(rf, ptr);
		}
		else if(isStructuredAggregate(ptr->getType()))
		{
			auto ty = ptr->getType();

			if(ty->isStructType())		doRefCountOfAggregateType(cgi, ty->toStructType(), ptr, incr);
			else if(ty->isClassType())	doRefCountOfAggregateType(cgi, ty->toClassType(), ptr, incr);
			else if(ty->isTupleType())	doRefCountOfAggregateType(cgi, ty->toTupleType(), ptr, incr);
		}
		else if(ptr->getType()->isArrayType())
		{
			fir::ArrayType* at = ptr->getType()->toArrayType();
			for(size_t i = 0; i < at->getArraySize(); i++)
			{
				fir::Value* elm = cgi->irb.CreateExtractValue(ptr, { i });
				iceAssert(cgi->isRefCountedType(elm->getType()));

				if(incr) cgi->incrementRefCount(elm);
				else cgi->decrementRefCount(elm);
			}
		}
		else
		{
			error("no: %s", ptr->getType()->str().c_str());
		}
	}

	void CodegenInstance::incrementRefCount(fir::Value* strp)
	{
		_doRefCount(this, strp, true);
	}

	void CodegenInstance::decrementRefCount(fir::Value* strp)
	{
		_doRefCount(this, strp, false);
	}


	void CodegenInstance::assignRefCountedExpression(Expr* user, fir::Value* rhs, fir::Value* rhsptr, fir::Value* lhs, fir::Value* lhsptr,
		ValueKind rhsVK, bool isInit, bool doAssign)
	{
		// if you're doing stupid things:
		if(!this->isRefCountedType(rhs->getType()))
			error(user, "type '%s' is not refcounted", rhs->getType()->str().c_str());

		// ok...
		// if the rhs is an lvalue, it's simple.
		// increment its refcount, decrement the left side refcount, store, return.
		if(rhsVK == ValueKind::LValue)
		{
			iceAssert(rhsptr);
			iceAssert(rhsptr->getType()->getPointerElementType() == rhs->getType());
			this->incrementRefCount(rhs);

			// decrement left side
			if(!isInit)
				this->decrementRefCount(lhs);

			// store
			if(doAssign)
				this->irb.CreateStore(rhs, lhsptr);
		}
		else
		{
			// the rhs has already been evaluated
			// as an rvalue, its refcount *SHOULD* be one
			// so we don't do anything to it
			// instead, decrement the left side

			// to avoid double-freeing, we remove 'val' from the list of refcounted things
			// since it's an rvalue, it can't be "re-referenced", so to speak.

			// the issue of double-free comes up when the variable being assigned to goes out of scope, and is freed
			// since they refer to the same pointer, we get a double free if the temporary expression gets freed as well.

			this->removeRefCountedValueIfExists(rhsptr);

			// now we just store as usual
			if(doAssign)
				this->irb.CreateStore(rhs, lhsptr);


			if(!isInit)
			{
				// info(user, "decr");
				this->decrementRefCount(lhs);
			}
		}
	}


	Result_t CodegenInstance::createParameterPack(fir::Type* type, std::vector<fir::Value*> parameters)
	{
		fir::Type* arrtype = fir::ArrayType::get(type, parameters.size());
		fir::Value* rawArrayPtr = this->getStackAlloc(arrtype);

		for(size_t i = 0; i < parameters.size(); i++)
		{
			auto gep = this->irb.CreateConstGEP2(rawArrayPtr, 0, i);
			this->irb.CreateStore(parameters[i], gep);
		}

		fir::Value* arrPtr = this->irb.CreateConstGEP2(rawArrayPtr, 0, 0);

		fir::DynamicArrayType* packType = fir::DynamicArrayType::getVariadic(type);
		fir::Value* pack = this->irb.CreateStackAlloc(packType);

		this->irb.CreateSetDynamicArrayData(pack, arrPtr);
		this->irb.CreateSetDynamicArrayLength(pack, fir::ConstantInt::getInt64(parameters.size()));
		this->irb.CreateSetDynamicArrayCapacity(pack, fir::ConstantInt::getInt64(0));

		pack->makeImmutable();

		return Result_t(this->irb.CreateLoad(pack), pack);
	}



	Result_t CodegenInstance::createDynamicArrayFromPointer(fir::Value* ptr, fir::Value* length, fir::Value* capacity)
	{
		iceAssert(ptr->getType()->isPointerType() && "ptr is not pointer type");
		iceAssert(length->getType() == fir::Type::getInt64() && "len is not i64");
		iceAssert(capacity->getType() == fir::Type::getInt64() && "cap is not i64");

		fir::DynamicArrayType* dtype = fir::DynamicArrayType::get(ptr->getType()->getPointerElementType());
		fir::Value* arr = this->irb.CreateStackAlloc(dtype);

		this->irb.CreateSetDynamicArrayData(arr, ptr);
		this->irb.CreateSetDynamicArrayLength(arr, length);
		this->irb.CreateSetDynamicArrayCapacity(arr, capacity);

		return Result_t(this->irb.CreateLoad(arr), arr);
	}

	Result_t CodegenInstance::createEmptyDynamicArray(fir::Type* elmType)
	{
		fir::DynamicArrayType* dtype = fir::DynamicArrayType::get(elmType);
		fir::Value* arr = this->irb.CreateStackAlloc(dtype);

		this->irb.CreateSetDynamicArrayData(arr, fir::ConstantValue::getZeroValue(elmType->getPointerTo()));
		this->irb.CreateSetDynamicArrayLength(arr, fir::ConstantInt::getInt64(0));
		this->irb.CreateSetDynamicArrayCapacity(arr, fir::ConstantInt::getInt64(0));

		return Result_t(this->irb.CreateLoad(arr), arr);
	}


































	static bool verifyReturnType(CodegenInstance* cgi, Func* f, BracedBlock* bb, Return* r, fir::Type* retType, bool errorOnFail)
	{
		fir::Type* expected = (retType ? retType : f->decl->getType(cgi));

		if(r)
		{
			fir::Type* have = 0;

			if(r->actualReturnValue)
			{
				have = r->actualReturnValue->getType();
			}

			if(!have)
			{
				have = r->val->getType(cgi);
			}

			if(have->isParametricType())
			{
				if(fir::Type* t = cgi->resolveGenericType(have->toParametricType()->getName()))
					have = t;
			}


			if(retType == 0)
			{
				expected = f->decl->getType(cgi);
			}
			else
			{
				expected = retType;
			}


			int dist = cgi->getAutoCastDistance(have, expected);

			if(dist == -1)
			{
				error(r, "Function has return type '%s', but return statement returned value of type '%s' instead",
					expected->str().c_str(), have->str().c_str());
			}

			return true;
		}
		else if(errorOnFail)
		{
			error(f->decl, "Function has return type '%s', but not all code paths returned a value",
				expected->str().c_str());
		}

		return false;
	}

	static Return* recursiveVerifyBranch(CodegenInstance* cgi, Func* f, IfStmt* ifbranch, bool checkType, fir::Type* retType);
	static Return* recursiveVerifyBlock(CodegenInstance* cgi, Func* f, BracedBlock* bb, bool checkType, fir::Type* retType)
	{
		if(bb->statements.size() == 0)
		{
			// _errorNoReturn(bb);
			return 0;
		}

		Return* r = 0;
		for(Expr* e : bb->statements)
		{
			IfStmt* i = 0;
			BreakableBracedBlock* bbb = 0;

			if((i = dynamic_cast<IfStmt*>(e)))
			{
				Return* tmp = recursiveVerifyBranch(cgi, f, i, checkType, retType);
				if(tmp)
				{
					r = tmp;
					break;
				}
			}
			else if((bbb = dynamic_cast<BreakableBracedBlock*>(e)))
			{
				Return* tmp = recursiveVerifyBlock(cgi, f, bbb->body, checkType, retType);
				if(tmp)
				{
					r = tmp;
					break;
				}
			}
			else if((r = dynamic_cast<Return*>(e)))
			{
				break;
			}
		}

		if(checkType)
		{
			verifyReturnType(cgi, f, bb, r, retType, false);
		}

		return r;
	}

	static Return* recursiveVerifyBranch(CodegenInstance* cgi, Func* f, IfStmt* ib, bool checkType, fir::Type* retType)
	{
		Return* r = 0;
		bool first = true;

		for(auto pair : ib->cases)
		{
			Return* tmp = recursiveVerifyBlock(cgi, f, std::get<1>(pair), checkType, retType);
			if(first)
				r = tmp;

			else if(r != nullptr)
				r = tmp;

			first = false;
		}

		if(ib->final)
		{
			if(r != nullptr)
				r = recursiveVerifyBlock(cgi, f, ib->final, checkType, retType);
		}
		else
		{
			r = nullptr;
		}

		return r;
	}

	// if the function returns void, the return value of verifyAllPathsReturn indicates whether or not
	// all code paths have explicit returns -- if true, Func::codegen is expected to insert a ret void at the end
	// of the body.
	bool CodegenInstance::verifyAllPathsReturn(Func* func, size_t* stmtCounter, bool checkType, fir::Type* retType)
	{
		if(stmtCounter)
			*stmtCounter = 0;

		bool isVoid = (retType == 0 ? (retType = func->getType(this)) : retType)->isVoidType();

		// check the block
		if(func->block->statements.size() == 0 && !isVoid)
		{
			error(func, "Function '%s' has return type '%s', but returns nothing", func->decl->ident.name.c_str(), retType->str().c_str());
		}

		// check for implicit return (only valid for 1 statement in block)
		if(!isVoid && func->block->statements.size() == 1 && !dynamic_cast<Return*>(func->block->statements[0]))
		{
			// todo: make this more robust maybe
			// also check it's not a loop or anything stupid like that
			// (maybe?)

			Expr* ex = func->block->statements.front();
			if(!dynamic_cast<BreakableBracedBlock*>(ex) && !dynamic_cast<IfStmt*>(ex))
			{
				// get the type
				fir::Type* t = ex->getType(this);

				if(checkType)
				{
					if(this->getAutoCastDistance(t, retType) == -1)
					{
						error(func, "Function '%s' missing return statement (implicit return invalid, needed type '%s', got '%s')",
							func->decl->ident.name.c_str(), func->getType(this)->str().c_str(), t ? t->str().c_str() : "(statement)");
					}
				}

				// ok, implicit return
				if(stmtCounter) *stmtCounter = 1;

				return true;
			}
		}




		// now loop through all exprs in the block
		Return* ret = 0;
		bool stopCounting = false;
		for(Expr* e : func->block->statements)
		{
			if(stmtCounter && !stopCounting)
				(*stmtCounter)++;

			if(IfStmt* i = dynamic_cast<IfStmt*>(e))
				ret = recursiveVerifyBranch(this, func, i, !isVoid && checkType, retType);

			else if(BreakableBracedBlock* bbb = dynamic_cast<BreakableBracedBlock*>(e))
				ret = recursiveVerifyBlock(this, func, bbb->body, !isVoid && checkType, retType);

			// "top level" returns we will just accept.
			if(ret || (ret = dynamic_cast<Return*>(e)))
			{
				// do check.
				if(isVoid && ret->getType(this) != fir::Type::getVoid())
				{
					// cry
					error(ret, "Function '%s' returns void, but return statement attempted to return a value of type '%s'",
						func->decl->ident.name.c_str(), ret->getType(this)->str().c_str());
				}

				stopCounting = true;
			}
		}

		if(checkType && !isVoid)
			verifyReturnType(this, func, func->block, ret, retType, true);

		return false;
	}














}



