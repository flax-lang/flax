// LlvmCodeGen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <map>
#include <set>
#include <vector>
#include <memory>
#include <cfloat>
#include <utility>
#include <fstream>
#include <stdint.h>
#include <typeinfo>
#include <iostream>
#include <cinttypes>
#include "parser.h"
#include "codegen.h"
#include "compiler.h"

using namespace Ast;
using namespace Codegen;


namespace Codegen
{
	void doCodegen(std::string filename, Root* root, CodegenInstance* cgi)
	{
		cgi->rootNode = root;
		cgi->module = new fir::Module(Parser::getModuleName(filename));


		// todo: proper.
		if(sizeof(void*) == 8)
			cgi->execTarget = fir::ExecutionTarget::getLP64();

		else if(sizeof(void*) == 4)
			cgi->execTarget = fir::ExecutionTarget::getILP32();

		else
			error("enotsup: ptrsize = %zu", sizeof(void*));

		cgi->pushScope();


		cgi->rootNode->rootFuncStack->nsName = "__#root_" + cgi->module->getModuleName();
		cgi->rootNode->publicFuncTree->nsName = "__#rootPUB_" + cgi->module->getModuleName();


		// rootFuncStack should really be empty, except we know that there should be
		// stuff inside from imports.
		// thus, solidify the insides of these, by adding the function to fir::Module.

		cgi->cloneFunctionTree(cgi->rootNode->rootFuncStack, cgi->rootNode->rootFuncStack, true);

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

	void CodegenInstance::popScope()
	{
		this->symTabStack.pop_back();
	}

	void CodegenInstance::clearScope()
	{
		this->symTabStack.clear();
		this->clearNamespaceScope();
	}

	void CodegenInstance::pushScope()
	{
		this->symTabStack.push_back(SymTab_t());
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

		FunctionTree* ftree = this->getCurrentFuncTree();
		iceAssert(ftree);

		if(ftree->types.find(atype->ident.name) != ftree->types.end())
		{
			// only if there's an actual, fir::Type* there.
			if(ftree->types[atype->ident.name].first)
				error(atype, "Duplicate type %s (in ftree %s:%d)", atype->ident.name.c_str(), ftree->nsName.c_str(), ftree->id);
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


	TypePair_t* CodegenInstance::getType(Identifier id)
	{
		return this->getTypeByString(id.str());
	}


	TypePair_t* CodegenInstance::getTypeByString(std::string name)
	{
		#if 0

			fprintf(stderr, "finding %s\n{\n", name.c_str());
			for(auto p : this->typeMap)
				fprintf(stderr, "\t%s\n", p.first.c_str());

			fprintf(stderr, "}\n");

		#endif

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

	bool CodegenInstance::isDuplicateType(Identifier id)
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

	std::deque<std::string> CodegenInstance::getFullScope()
	{
		std::deque<std::string> full = this->namespaceStack;
		for(auto s : this->nestedTypeStack)
			full.push_back(s->ident.name);

		return full;
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
		if(this->resolveGenericType(id) != 0)
			error(0, "Error: generic type %s already exists; types cannot be shadowed", id.c_str());

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












	// funcs
	void CodegenInstance::addPublicFunc(FuncPair_t fp)
	{
		this->addFunctionToScope(fp, this->rootNode->publicFuncTree);
	}



	static fir::Function* cloneFunctionIntoCurrentTree(CodegenInstance* cgi, fir::Function* func, FuncDecl* decl, FunctionTree* target)
	{
		fir::Function* ret = func;
		if(func && func->linkageType == fir::LinkageType::External)
		{
			iceAssert(func);

			// add to the func table
			auto lf = cgi->module->getFunction(func->getName());
			if(!lf)
			{
				cgi->module->declareFunction(func->getName(), func->getType());
				lf = cgi->module->getFunction(func->getName());
			}

			fir::Function* f = dynamic_cast<fir::Function*>(lf);

			f->deleteBody();
			cgi->addFunctionToScope(FuncPair_t(f, decl));

			ret = f;
		}
		else if(!func)
		{
			// note: generic functions are not instantiated
			if(decl->genericTypes.size() == 0)
				error(decl, "!func (%s)", decl->ident.str().c_str());
		}

		return ret;
	}

	void CodegenInstance::cloneFunctionTree(FunctionTree* ft, FunctionTree* clone, bool deep)
	{
		clone->nsName = ft->nsName;

		for(auto pair : ft->funcs)
		{
			bool existing = false;
			for(auto f : clone->funcs)
			{
				if((f.first && pair.first && (f.first == pair.first))
					|| (f.second && pair.second && (f.second->ident == pair.second->ident)))
				{
					existing = true;
					break;
				}
			}


			if(deep)
			{
				pair.first = cloneFunctionIntoCurrentTree(this, pair.first, pair.second, clone);
			}

			if(!existing)
			{
				clone->funcs.push_back(pair);
			}
		}

		for(auto t : ft->types)
		{
			bool found = false;
			for(auto tt : clone->types)
			{
				if(tt.first == t.first)
				{
					if(!(deep && this->typeMap.find(t.first) == this->typeMap.end()))
					{
						found = true;
						break;
					}
				}
			}

			if(!found && t.first != "Type" && t.first != "Any")
			{
				if(StructBase* sb = dynamic_cast<StructBase*>(t.second.second.first))
				{
					clone->types[sb->ident.name] = t.second;

					if(deep)
					{
						this->typeMap[sb->ident.str()] = t.second;

						// check what kind of struct.
						if(StructDef* str = dynamic_cast<StructDef*>(sb))
						{
							if(str->attribs & Attr_VisPublic)
							{
								for(auto f : str->initFuncs)
									this->module->declareFunction(f->getName(), f->getType());
							}
						}
						else if(ClassDef* cls = dynamic_cast<ClassDef*>(sb))
						{
							if(cls->attribs & Attr_VisPublic)
							{
								for(auto f : cls->initFuncs)
									this->module->declareFunction(f->getName(), f->getType());

								for(auto ao : cls->assignmentOverloads)
									this->module->declareFunction(ao->lfunc->getName(), ao->lfunc->getType());

								for(auto so : cls->subscriptOverloads)
								{
									this->module->declareFunction(so->getterFunc->getName(), so->getterFunc->getType());

									if(so->setterFunc)
										this->module->declareFunction(so->setterFunc->getName(), so->setterFunc->getType());
								}
							}
						}
						else if(dynamic_cast<Tuple*>(sb))
						{
							// ignore
						}
						else
						{
							iceAssert(0);
						}
					}
				}
			}
		}

		for(auto ext : ft->extensions)
		{
			bool found = false;
			for(auto e : clone->extensions)
			{
				if(e.second == ext.second)
				{
					found = true;
					break;
				}
			}

			if(ext.second->attribs & Attr_VisPublic)
			{
				if(!found && deep)
				{
					clone->extensions.insert(std::make_pair(ext.first, ext.second));

					ExtensionDef* ed = ext.second;
					for(auto& ff : ed->functionMap)
					{
						if(ff.first->decl->attribs & Attr_VisPublic)
						{
							auto newff = cloneFunctionIntoCurrentTree(this, ff.second, ff.first->decl, clone);
							std::replace(ed->lfuncs.begin(), ed->lfuncs.end(), ff.second, newff);

							ff.second = newff;
						}
						else
						{
							// do nothing
						}
					}
				}
				else if(!found)
				{
					clone->extensions.insert(std::make_pair(ext.first, ext.second));
				}
			}
		}

		for(auto oo : ft->operators)
		{
			bool found = false;
			for(auto ooc : clone->operators)
			{
				if(oo->func->decl->ident == ooc->func->decl->ident)
				{
					if(!deep)
					{
						found = true;
						break;
					}
				}
			}

			if(oo->attribs & Attr_VisPublic)
			{
				if(deep)
				{
					// todo: do we need this?
					oo->lfunc = cloneFunctionIntoCurrentTree(this, oo->lfunc, oo->func->decl, clone);
				}
				if(!found)
				{
					clone->operators.push_back(oo);
				}
			}
		}

		for(auto var : ft->vars)
		{
			if(var.second.second->attribs & Attr_VisPublic)
			{
				bool found = false;
				for(auto v : clone->vars)
				{
					if(v.second.second->ident == var.second.second->ident)
					{
						found = true;
						break;
					}
				}

				if(!found && !deep)
				{
					clone->vars[var.first] = var.second;
				}
				else if(deep)
				{
					// add to the module list
					// note: we're getting the ptr element type since the Value* stored is the allocated storage, which is a ptr.

					iceAssert(this->module);
					iceAssert(clone->vars.find(var.first) != clone->vars.end());

					fir::GlobalVariable* potentialGV = this->module->tryGetGlobalVariable(var.second.second->ident);

					if(potentialGV == 0)
					{
						auto gv = this->module->declareGlobalVariable(var.second.second->ident,
							var.second.first->getType()->getPointerElementType(), var.second.second->immutable);

						clone->vars[var.first] = SymbolPair_t(gv, var.second.second);
					}
					else
					{
						if(potentialGV->getType() != var.second.first->getType())
						{
							error(var.second.second, "Conflicting types for global variable %s: %s vs %s.",
								var.second.second->ident.str().c_str(), var.second.first->getType()->getPointerElementType()->str().c_str(),
								potentialGV->getType()->getPointerElementType()->str().c_str());
						}

						clone->vars[var.first] = SymbolPair_t(potentialGV, var.second.second);
					}
				}
			}
		}


		for(auto gf : ft->genericFunctions)
		{
			bool found = false;
			for(auto cgf : clone->genericFunctions)
			{
				if(cgf.first == gf.first || cgf.second == gf.second)
				{
					found = true;
					break;
				}
			}

			if(!found/* && gf.first->attribs & Attr_VisPublic*/)
				clone->genericFunctions.push_back(gf);
		}



		for(auto prot : ft->protocols)
		{
			for(auto cprot : clone->protocols)
			{
				if(prot.first == cprot.first && prot.second != cprot.second)
					error(prot.second, "conflicting protocols with the same name");
			}

			clone->protocols[prot.first] = prot.second;
		}


		for(auto sub : ft->subs)
		{
			FunctionTree* found = 0;
			for(auto s : clone->subs)
			{
				if(s->nsName == sub->nsName)
				{
					found = s;
					break;
				}
			}

			if(found)
			{
				this->cloneFunctionTree(sub, found, deep);
			}
			else
			{
				clone->subs.push_back(this->cloneFunctionTree(sub, deep));
			}
		}
	}

	FunctionTree* CodegenInstance::cloneFunctionTree(FunctionTree* ft, bool deep)
	{
		FunctionTree* clone = new FunctionTree();
		this->cloneFunctionTree(ft, clone, deep);
		return clone;
	}


	FunctionTree* CodegenInstance::getCurrentFuncTree(std::deque<std::string>* nses, FunctionTree* root)
	{
		if(root == 0) root = this->rootNode->rootFuncStack;
		if(nses == 0) nses = &this->namespaceStack;

		iceAssert(root);
		iceAssert(nses);

		std::deque<FunctionTree*> ft = root->subs;

		if(nses->size() == 0) return root;

		size_t i = 0;
		size_t max = nses->size();

		for(auto ns : *nses)
		{
			i++;

			bool found = false;
			for(auto f : ft)
			{
				if(f->nsName == ns)
				{
					ft = f->subs;

					if(i == max)
						return f;

					found = true;
					break;
				}
			}

			if(!found)
			{
				return 0;
			}
		}

		return 0;
	}

	std::pair<TypePair_t*, int> CodegenInstance::findTypeInFuncTree(std::deque<std::string> scope, std::string name)
	{
		auto curDepth = scope;

		// this thing handles pointers properly.
		if(this->getExprTypeOfBuiltin(name) != 0)
			return { 0, 0 };

		// not this though.
		int indirections = 0;
		name = unwrapPointerType(name, &indirections);

		for(size_t i = 0; i <= scope.size(); i++)
		{
			FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
			if(!ft) break;

			for(auto& f : ft->types)
			{
				if(f.first == name)
					return { &f.second, indirections };
			}

			if(curDepth.size() > 0)
				curDepth.pop_back();
		}

		return { 0, -1 };
	}











	void CodegenInstance::pushNamespaceScope(std::string namespc, bool doFuncTree)
	{
		if(doFuncTree)
		{
			bool found = false;
			FunctionTree* existing = this->getCurrentFuncTree();
			for(auto s : existing->subs)
			{
				if(s->nsName == namespc)
				{
					found = true;
					break;
				}
			}

			if(!found)
			{
				FunctionTree* ft = new FunctionTree();
				ft->nsName = namespc;

				existing->subs.push_back(ft);

				FunctionTree* pub = this->getCurrentFuncTree(0, this->rootNode->publicFuncTree);
				pub->subs.push_back(ft);
			}
		}

		this->namespaceStack.push_back(namespc);
	}

	void CodegenInstance::popNamespaceScope()
	{
		this->namespaceStack.pop_back();
	}

	void CodegenInstance::addFunctionToScope(FuncPair_t func, FunctionTree* root)
	{
		FunctionTree* cur = root;
		if(!cur)
			cur = this->getCurrentFuncTree();

		iceAssert(cur);

		for(FuncPair_t& fp : cur->funcs)
		{
			if(fp.first == 0 && fp.second == func.second)
			{
				fp.first = func.first;
				return;
			}
			else if(fp.first == func.first && fp.second == func.second)
			{
				return;
			}
		}

		cur->funcs.push_back(func);
	}

	void CodegenInstance::removeFunctionFromScope(FuncPair_t func)
	{
		FunctionTree* cur = this->getCurrentFuncTree();
		iceAssert(cur);

		auto it = std::find(cur->funcs.begin(), cur->funcs.end(), func);
		if(it != cur->funcs.end())
			cur->funcs.erase(it);
	}

	std::deque<FuncPair_t> CodegenInstance::resolveFunctionName(std::string basename)
	{
		std::deque<std::string> curDepth = this->namespaceStack;
		std::deque<FuncPair_t> candidates;


		auto _isDupe = [this](FuncPair_t a, FuncPair_t b) -> bool {

			if(a.first == 0 || b.first == 0)
			{
				iceAssert(a.second);
				iceAssert(b.second);

				if(a.second->params.size() != b.second->params.size()) return false;
				if(a.second->genericTypes.size() > 0 && b.second->genericTypes.size() > 0)
				{
					if(a.second->genericTypes != b.second->genericTypes) return false;
				}

				for(size_t i = 0; i < a.second->params.size(); i++)
				{
					// allowFail = true
					if(this->getExprType(a.second->params[i], true) != this->getExprType(b.second->params[i], true))
						return false;
				}

				return true;
			}
			else if(a.first == b.first || a.second == b.second)
			{
				return true;
			}
			else
			{
				return a.first->getType() == b.first->getType();
			}
		};



		for(size_t i = 0; i <= this->namespaceStack.size(); i++)
		{
			FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
			if(!ft) break;

			for(auto f : ft->funcs)
			{
				auto isDupe = [this, f, _isDupe](FuncPair_t fp) -> bool {
					return _isDupe(f, fp);
				};

				if((f.second ? f.second->ident.name : f.first->getName().str()) == basename)
				{
					if(std::find_if(candidates.begin(), candidates.end(), isDupe) == candidates.end())
					{
						// printf("FOUND (1) %s in search of %s\n", this->printAst(f.second).c_str(), basename.c_str());
						candidates.push_back(f);
					}
				}
			}



			if(curDepth.size() > 0)
				curDepth.pop_back();
		}

		return candidates;
	}



	Resolved_t CodegenInstance::resolveFunctionFromList(Expr* user, std::deque<FuncPair_t> list, std::string basename,
		std::deque<Expr*> params, bool exactMatch)
	{
		std::deque<fir::Type*> argTypes;
		for(auto e : params)
			argTypes.push_back(this->getExprType(e, true));

		return this->resolveFunctionFromList(user, list, basename, argTypes, exactMatch);
	}



	Resolved_t CodegenInstance::resolveFunctionFromList(Expr* user, std::deque<FuncPair_t> list, std::string basename,
		std::deque<fir::Type*> params, bool exactMatch)
	{
		std::deque<FuncPair_t> candidates = list;
		if(candidates.size() == 0) return Resolved_t();

		std::deque<std::pair<FuncPair_t, int>> finals;
		for(auto c : candidates)
		{
			int distance = 0;

			// note: if we don't provide the FuncDecl, assume we have everything down, including the basename.
			if((c.second ? c.second->ident.name : basename) == basename
				&& this->isValidFuncOverload(c, params, &distance, exactMatch))
			{
				finals.push_back({ c, distance });
			}
		}

		// disambiguate this.
		// with casting distance.
		if(finals.size() > 1)
		{
			// go through each.
			std::deque<std::pair<FuncPair_t, int>> mostViable;
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
				for(auto e : params)
					pstr += e->str() + ", ";

				if(params.size() > 0)
					pstr = pstr.substr(0, pstr.size() - 2);

				// candidates
				std::string cstr;
				for(auto c : finals)
				{
					if(c.first.second)
						cstr += this->printAst(c.first.second) + "\n";
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

	Resolved_t CodegenInstance::resolveFunction(Expr* user, std::string basename, std::deque<Expr*> params, bool exactMatch)
	{
		std::deque<FuncPair_t> candidates = this->resolveFunctionName(basename);
		return this->resolveFunctionFromList(user, candidates, basename, params, exactMatch);
	}






	std::deque<Func*> CodegenInstance::findGenericFunctions(std::string basename)
	{
		std::deque<std::string> curDepth = this->namespaceStack;
		std::deque<Func*> ret;

		for(size_t i = 0; i <= this->namespaceStack.size(); i++)
		{
			FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
			if(!ft) break;

			for(auto f : ft->genericFunctions)
			{
				iceAssert(f.first->genericTypes.size() > 0);

				if(f.first->ident.name == basename)
					ret.push_back({ f.second });
			}

			if(curDepth.size() > 0)
				curDepth.pop_back();
		}

		return ret;
	}








	static bool _checkFunction(CodegenInstance* cgi, std::deque<fir::Type*> funcParams, std::deque<fir::Type*> args,
		int* _dist, bool variadic, bool c_variadic, bool exact)
	{
		iceAssert(_dist);
		*_dist = 0;

		if(!variadic)
		{
			if(funcParams.size() != args.size() && !c_variadic) return false;
			if(funcParams.size() == 0 && (args.size() == 0 || c_variadic)) return true;

			#define __min(x, y) ((x) > (y) ? (y) : (x))
			for(size_t i = 0; i < __min(args.size(), funcParams.size()); i++)
			{
				fir::Type* t1 = args[i];
				fir::Type* t2 = funcParams[i];

				// fprintf(stderr, "%zu: %s vs %s\n", i, t1->str().c_str(), t2->str().c_str());

				if(t1 != t2)
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

				if(t1 != t2)
				{
					if(exact || t1 == 0 || t2 == 0) return false;

					// try to cast.
					int dist = cgi->getAutoCastDistance(t1, t2);
					if(dist == -1) return false;

					*_dist += dist;
				}
			}


			// 3. get the type of the vararg array.
			fir::Type* funcLLType = funcParams.back();
			iceAssert(funcLLType->isLLVariableArrayType());

			fir::Type* llElmType = funcLLType->toLLVariableArray()->getElementType();

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

	bool CodegenInstance::isValidFuncOverload(FuncPair_t fp, std::deque<fir::Type*> argTypes, int* castingDistance, bool exactMatch)
	{
		iceAssert(castingDistance);
		std::deque<fir::Type*> funcParams;


		bool iscvar = 0;
		bool isvar = 0;

		if(fp.second)
		{
			for(auto arg : fp.second->params)
				funcParams.push_back(this->getExprType(arg, true));

			iscvar = fp.second->isCStyleVarArg;
			isvar = fp.second->isVariadic;
		}
		else
		{
			iceAssert(fp.first);

			for(auto arg : fp.first->getArguments())
				funcParams.push_back(arg->getType());

			iscvar = fp.first->isCStyleVarArg();
			isvar = fp.first->isVariadic();
		}

		return _checkFunction(this, funcParams, argTypes, castingDistance, isvar, iscvar, exactMatch);
	}























	void CodegenInstance::clearNamespaceScope()
	{
		this->namespaceStack.clear();
	}



	FuncPair_t* CodegenInstance::getOrDeclareLibCFunc(std::string name)
	{
		std::deque<FuncPair_t> fps = this->resolveFunctionName(name);
		FuncPair_t* fp = 0;

		for(auto f : fps)
		{
			iceAssert(f.second->ident.name == name);
			if(f.second->isFFI)
			{
				fp = &f;
				break;
			}
		}

		if(!fp)
		{
			std::string retType;
			std::deque<VarDecl*> params;
			if(name == "malloc")
			{
				VarDecl* fakefdmvd = new VarDecl(Parser::Pin(), "size", false);
				fakefdmvd->type = UINT64_TYPE_STRING;
				params.push_back(fakefdmvd);

				retType = std::string(INT8_TYPE_STRING) + "*";
			}
			else if(name == "free")
			{
				VarDecl* fakefdmvd = new VarDecl(Parser::Pin(), "ptr", false);
				fakefdmvd->type = std::string(INT8_TYPE_STRING) + "*";
				params.push_back(fakefdmvd);

				retType = std::string(INT8_TYPE_STRING) + "*";
			}
			else if(name == "strlen")
			{
				VarDecl* fakefdmvd = new VarDecl(Parser::Pin(), "str", false);
				fakefdmvd->type = std::string(INT8_TYPE_STRING) + "*";
				params.push_back(fakefdmvd);

				retType = INT64_TYPE_STRING;
			}
			else if(name == "memset")
			{
				VarDecl* fakefdmvd1 = new VarDecl(Parser::Pin(), "ptr", false);
				fakefdmvd1->type = std::string(INT8_TYPE_STRING) + "*";
				params.push_back(fakefdmvd1);

				VarDecl* fakefdmvd2 = new VarDecl(Parser::Pin(), "val", false);
				fakefdmvd2->type = std::string(INT8_TYPE_STRING) + "*";
				params.push_back(fakefdmvd2);

				VarDecl* fakefdmvd3 = new VarDecl(Parser::Pin(), "size", false);
				fakefdmvd3->type = UINT64_TYPE_STRING;
				params.push_back(fakefdmvd3);

				retType = std::string(INT8_TYPE_STRING) + "*";
			}
			else
			{
				error("enotsup: %s", name.c_str());
			}

			FuncDecl* fakefm = new FuncDecl(Parser::Pin(), name, params, retType);
			fakefm->isFFI = true;
			fakefm->codegen(this);
		}

		return fp;
	}















	std::deque<std::string> CodegenInstance::unwrapNamespacedType(std::string raw)
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
		std::deque<std::string> nses;
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



















	std::string CodegenInstance::mangleGenericParameters(std::deque<VarDecl*> args)
	{
		std::deque<std::string> strs;
		std::map<std::string, int> uniqueGenericTypes;	// only a map because it's easier to .find().

		// TODO: this is very suboptimal
		int runningTypeIndex = 0;
		for(auto arg : args)
		{
			fir::Type* atype = this->getExprType(arg, true);	// same as mangleFunctionName, but allow failures.

			// if there is no proper type, go ahead with the raw type: T or U or something.
			if(!atype)
			{
				std::string st = arg->type.strType;
				if(uniqueGenericTypes.find(st) == uniqueGenericTypes.end())
				{
					uniqueGenericTypes[st] = runningTypeIndex;
					runningTypeIndex++;
				}
			}
		}

		// very very suboptimal.

		for(auto arg : args)
		{
			fir::Type* atype = this->getExprType(arg, true);	// same as mangleFunctionName, but allow failures.

			// if there is no proper type, go ahead with the raw type: T or U or something.
			if(!atype)
			{
				std::string st = arg->type.strType;
				iceAssert(uniqueGenericTypes.find(st) != uniqueGenericTypes.end());

				std::string s = "GT" + std::to_string(uniqueGenericTypes[st]);
				strs.push_back(std::to_string(s.length()) + s);
			}
			else
			{
				std::string mangled = atype->encodedStr();

				if(atype->isLLVariableArrayType())
				{
					mangled = "V" + atype->toLLVariableArray()->encodedStr();
				}

				while(atype->isPointerType())
					mangled += "P", atype = atype->getPointerElementType();

				strs.push_back(mangled);
			}
		}

		std::string ret;
		for(auto s : strs)
			ret += "_" + s;

		return ret;
	}



















	bool CodegenInstance::isDuplicateFuncDecl(FuncDecl* decl)
	{
		if(decl->isFFI) return false;

		std::deque<Expr*> es;
		for(auto p : decl->params) es.push_back(p);

		Resolved_t res = this->resolveFunction(decl, decl->ident.name, es, true);
		if(res.resolved && res.t.first != 0)
		{
			fprintf(stderr, "Duplicate function: %s\n", this->printAst(res.t.second).c_str());
			for(size_t i = 0; i < __min(decl->params.size(), res.t.second->params.size()); i++)
			{
				info(res.t.second, "%zu: %s, %s", i, this->getReadableType(decl->params[i]).c_str(),
					this->getReadableType(res.t.second->params[i]).c_str());
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	ProtocolDef* CodegenInstance::resolveProtocolName(Expr* user, std::string protstr)
	{
		std::deque<std::string> nses = this->unwrapNamespacedType(protstr);
		std::string protname = nses.back();
		nses.pop_back();

		auto curDepth = nses;
		ProtocolDef* prot = 0;
		for(size_t i = 0; i <= nses.size(); i++)
		{
			FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
			if(!ft) break;

			for(auto& f : ft->protocols)
			{
				if(f.first == protname)
				{
					prot = f.second;
					break;
				}
			}

			if(curDepth.size() > 0)
				curDepth.pop_back();
		}

		if(!prot)
			error(user, "Undeclared protocol '%s'", protname.c_str());

		return prot;
	}















	static bool _checkGenericFunction(CodegenInstance* cgi, std::map<std::string, fir::Type*>* gtm,
		FuncDecl* candidate, std::deque<fir::Type*> args)
	{
		std::map<std::string, fir::Type*> thistm;

		// now check if we *can* instantiate it.
		// first check the number of arguments.
		if(candidate->params.size() != args.size())
		{
			return false;
		}
		else
		{
			// param count matches...
			// do a similar thing as the actual mangling -- build a list of
			// uniquely named types.

			std::map<std::string, std::vector<int>> typePositions;
			std::vector<int> nonGenericTypes;

			int pos = 0;
			for(auto p : candidate->params)
			{
				std::string s = p->type.strType;
				if(candidate->genericTypes.find(s) != candidate->genericTypes.end())
				{
					typePositions[s].push_back(pos);
				}
				else
				{
					nonGenericTypes.push_back(pos);
				}

				pos++;
			}


			// this needs to be basically a fully manual check.
			// 1. check that the generic types match (ie. all the Ts are the same type, all the Ks are the same, etc.)
			for(auto pair : typePositions)
			{
				fir::Type* ftype = args[pair.second[0]];
				for(int k : pair.second)
				{
					if(args[k] != ftype)
						return false;
				}
			}


			// 2. check that the concrete types match.
			for(int k : nonGenericTypes)
			{
				fir::Type* a = args[k];
				fir::Type* b = cgi->getExprType(candidate->params[k]);

				if(a != b) return false;
			}



			// fill in the typemap.
			// note that it's okay if we just have one -- if we did this loop more
			// than once and screwed up the tm, that means we have more than one
			// candidate, and will error anyway.

			for(auto pair : typePositions)
			{
				thistm[pair.first] = args[pair.second[0]];
			}


			// last phase: ensure the type constraints are met
			for(auto cst : thistm)
			{
				TypeConstraints_t constr = candidate->genericTypes[cst.first];
				if((constr.pointerDegree > 0 && !cst.second->isPointerType())
					&& cst.second->isPointerType() && (size_t) constr.pointerDegree != cst.second->toPointerType()->getIndirections())
				{
					return false;
				}

				for(auto protstr : constr.protocols)
				{
					ProtocolDef* prot = cgi->resolveProtocolName(candidate, protstr);
					iceAssert(prot);

					bool doesConform = prot->checkTypeConformity(cgi, cst.second);

					if(!doesConform)
						return false;
				}
			}


			*gtm = thistm;
			return true;
		}
	}



	FuncPair_t CodegenInstance::instantiateGenericFunctionUsingParameters(Expr* user, std::map<std::string, fir::Type*> _gtm,
		Func* func, std::deque<fir::Type*> params)
	{
		iceAssert(func);
		iceAssert(func->decl);

		FuncDecl* fnDecl = func->decl;


		std::map<std::string, fir::Type*> gtm = _gtm;
		if(gtm.empty())
		{
			bool res = _checkGenericFunction(this, &gtm, func->decl, params);
			if(!res) return FuncPair_t(0, 0);
		}




		bool needToCodegen = true;
		if(this->reifiedGenericFunctions.find({ func, gtm }) != this->reifiedGenericFunctions.end())
			needToCodegen = false;


		// we need to push a new "generic type stack", and add the types that we resolved into it.
		// todo: might be inefficient.
		// todo: look into creating a version of pushGenericTypeStack that accepts a std::map<string, fir::Type*>
		// so we don't have to iterate etc etc.
		// I don't want to access cgi->instantiatedGenericTypeStack directly.
		this->pushGenericTypeStack();
		for(auto pair : gtm)
			this->pushGenericType(pair.first, pair.second);


		fir::Function* ffunc = nullptr;
		if(needToCodegen)
		{
			Result_t res = fnDecl->generateDeclForGenericFunction(this, gtm);
			ffunc = (fir::Function*) res.result.first;

			this->reifiedGenericFunctions[{ func, gtm }] = ffunc;
		}
		else
		{
			// std::deque<Expr*> es;
			// for(auto p : candidate->params) es.push_back(p);

			// Resolved_t rt = this->resolveFunction(fc, candidate->ident.name, es, true); // exact match
			// iceAssert(rt.resolved);

			// FuncPair_t fp = rt.t;

			// ffunc = fp.first;
			// iceAssert(ffunc);

			ffunc = this->reifiedGenericFunctions[{ func, gtm }];
			iceAssert(ffunc);
		}

		iceAssert(ffunc);


		if(needToCodegen)
		{
			// dirty: use 'lhsPtr' to pass the version we want.
			func->codegen(this, ffunc);
		}


		this->removeFunctionFromScope({ 0, fnDecl });
		this->popGenericTypeStack();

		return { ffunc, fnDecl };
	}

	FuncPair_t CodegenInstance::tryResolveGenericFunctionCall(FuncCall* fc)
	{
		// try and resolve shit
		std::map<std::string, fir::Type*> gtm;
		std::deque<Func*> candidates = this->findGenericFunctions(fc->name);


		std::deque<fir::Type*> fargs;
		for(auto p : fc->params)
			fargs.push_back(this->getExprType(p));


		if(candidates.size() == 0)
		{
			return { 0, 0 };	// just fail
		}


		auto it = candidates.begin();
		while(it != candidates.end())
		{
			bool result = _checkGenericFunction(this, &gtm, (*it)->decl, fargs);

			if(!result)
			{
				it = candidates.erase(it);
			}
			else
			{
				it++;
			}
		}

		if(candidates.size() == 0)
		{
			return { 0, 0 };
		}
		else if(candidates.size() > 1)
		{
			std::string cands;
			for(auto c : candidates)
				cands += this->printAst(c) + "\n";

			error(fc, "Ambiguous call to generic function %s, have %zd candidates:\n%s\n", fc->name.c_str(),
				candidates.size(), cands.c_str());
		}

		return this->instantiateGenericFunctionUsingParameters(fc, gtm, candidates[0], fargs);
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








	static std::deque<ExtensionDef*> _findExtensionsByNameInScope(std::string name, FunctionTree* ft)
	{
		std::deque<ExtensionDef*> ret;
		iceAssert(ft);

		auto pair = ft->extensions.equal_range(name);
		for(auto it = pair.first; it != pair.second; it++)
		{
			if((*it).first == name)
				ret.push_back((*it).second);
		}

		return ret;
	}

	std::deque<ExtensionDef*> CodegenInstance::getExtensionsWithName(std::string name)
	{
		FunctionTree* ft = this->getCurrentFuncTree();
		iceAssert(ft);

		return _findExtensionsByNameInScope(name, ft);
	}

	std::deque<ExtensionDef*> CodegenInstance::getExtensionsForType(StructBase* cls)
	{
		std::set<ExtensionDef*> ret;

		// 1. look in the current scope
		{
			FunctionTree* ft = this->getCurrentFuncTree();
			iceAssert(ft);

			std::deque<ExtensionDef*> res = _findExtensionsByNameInScope(cls->ident.name, ft);
			ret.insert(res.begin(), res.end());
		}


		// 2. look in the scope of the type
		{
			std::deque<std::string> curDepth = cls->ident.scope;

			for(size_t i = 0; i <= this->namespaceStack.size(); i++)
			{
				FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
				if(!ft) break;

				std::deque<ExtensionDef*> res = _findExtensionsByNameInScope(cls->ident.name, ft);

				ret.insert(res.begin(), res.end());

				if(curDepth.size() > 0)
					curDepth.pop_back();
			}
		}

		return std::deque<ExtensionDef*>(ret.begin(), ret.end());
	}

	std::deque<ExtensionDef*> CodegenInstance::getExtensionsForBuiltinType(fir::Type* type)
	{
		if(fir::Type::fromBuiltin(INT8_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT8_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT16_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT16_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT32_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT32_TYPE_STRING);

		else if(fir::Type::fromBuiltin(INT64_TYPE_STRING) == type)
			return this->getExtensionsWithName(INT64_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT8_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT8_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT16_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT16_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT32_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT32_TYPE_STRING);

		else if(fir::Type::fromBuiltin(UINT64_TYPE_STRING) == type)
			return this->getExtensionsWithName(UINT64_TYPE_STRING);

		else if(fir::Type::fromBuiltin(FLOAT32_TYPE_STRING) == type)
			return this->getExtensionsWithName(FLOAT32_TYPE_STRING);

		else if(fir::Type::fromBuiltin(FLOAT64_TYPE_STRING) == type)
			return this->getExtensionsWithName(FLOAT64_TYPE_STRING);

		else if(fir::Type::fromBuiltin(BOOL_TYPE_STRING) == type)
			return this->getExtensionsWithName(BOOL_TYPE_STRING);

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

				this->module->declareFunction(fnId, ft);
				fir::Function* fn = this->module->getFunction(fnId);

				if(fn->getBlockList().size() == 0)
				{
					fir::IRBlock* prevBlock = this->builder.getCurrentBlock();

					fir::IRBlock* block = this->builder.addNewBlockInFunction("entry", fn);
					this->builder.setCurrentBlock(block);

					// fir::Value* param = ++fn->arg_begin();
					this->builder.CreateReturn(fir::ConstantValue::getNullValue(pair->first));

					this->builder.setCurrentBlock(prevBlock);
				}

				if(vals.size() != fn->getArgumentCount())
					GenError::invalidInitialiser(this, user, this->getReadableType(pair->first), vals);

				for(size_t i = 0; i < fn->getArgumentCount(); i++)
				{
					if(vals[i]->getType() != fn->getArguments()[i]->getType())
						GenError::invalidInitialiser(this, user, this->getReadableType(pair->first), vals);
				}

				return fn;
			}
			else if(vals.size() == 2)
			{
				std::vector<fir::Type*> args { pair->first->getPointerTo(), pair->first };
				fir::FunctionType* ft = fir::FunctionType::get(args, pair->first, false);

				Identifier fnId = Identifier("__builtin_primitive_init_" + pair->first->encodedStr(), IdKind::AutoGenFunc);
				fnId.functionArguments = ft->getArgumentTypes();


				this->module->declareFunction(fnId, ft);
				fir::Function* fn = this->module->getFunction(fnId);

				if(fn->getBlockList().size() == 0)
				{
					fir::IRBlock* prevBlock = this->builder.getCurrentBlock();

					fir::IRBlock* block = this->builder.addNewBlockInFunction("entry", fn);
					this->builder.setCurrentBlock(block);

					iceAssert(fn->getArgumentCount() > 1);

					// fir::Value* param = ++fn->arg_begin();
					this->builder.CreateReturn(fn->getArguments()[1]);

					this->builder.setCurrentBlock(prevBlock);
				}


				if(vals.size() != fn->getArgumentCount())
					GenError::invalidInitialiser(this, user, this->getReadableType(pair->first), vals);

				for(size_t i = 0; i < fn->getArgumentCount(); i++)
				{
					if(vals[i]->getType() != fn->getArguments()[i]->getType())
						GenError::invalidInitialiser(this, user, this->getReadableType(pair->first), vals);
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
			iceAssert(pair->second.second == TypeKind::TypeAlias);
			TypeAlias* ta = dynamic_cast<TypeAlias*>(pair->second.first);
			iceAssert(ta);

			TypePair_t* tp = this->getTypeByString(ta->origType);
			iceAssert(tp);

			return this->getStructInitialiser(user, tp, vals);
		}
		else if(pair->second.second == TypeKind::Class || pair->second.second == TypeKind::Struct)
		{
			StructBase* sb = dynamic_cast<StructBase*>(pair->second.first);
			iceAssert(sb);

			// use function overload operator for this.

			std::deque<FuncPair_t> fns;
			for(auto f : sb->initFuncs)
				fns.push_back({ f, 0 });

			std::deque<ExtensionDef*> exts = this->getExtensionsForType(sb);
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
						fns.push_back({ f, 0 });
					}
				}
			}

			std::deque<fir::Type*> argTypes;
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

			auto ret = this->module->getFunction(res.t.first->getName());
			iceAssert(ret);

			return ret;
		}
		else
		{
			error(user, "Invalid expr type (%s)", typeid(*pair->second.first).name());
		}
	}















	Result_t CodegenInstance::assignValueToAny(fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr)
	{
		fir::Value* typegep = this->builder.CreateStructGEP(lhsPtr, 0, "anyGEP");	// Any
		typegep = this->builder.CreateStructGEP(typegep, 0, "any_TypeGEP");			// Type

		size_t index = TypeInfo::getIndexForType(this, rhs->getType());
		iceAssert(index > 0);

		fir::Value* constint = fir::ConstantInt::getUnsigned(typegep->getType()->getPointerElementType(), index);
		this->builder.CreateStore(constint, typegep);



		fir::Value* valgep = this->builder.CreateStructGEP(lhsPtr, 1);
		if(rhsPtr)
		{
			fir::Value* casted = this->builder.CreatePointerTypeCast(rhsPtr, valgep->getType()->getPointerElementType());
			this->builder.CreateStore(casted, valgep);
		}
		else
		{
			fir::Type* targetType = rhs->getType()->isIntegerType() ? valgep->getType()->getPointerElementType() :
				fir::PrimitiveType::getInt64(this->getContext());


			if(rhs->getType()->isIntegerType())
			{
				fir::Value* casted = this->builder.CreateIntToPointerCast(rhs, targetType);
				this->builder.CreateStore(casted, valgep);
			}
			else if(rhs->getType()->isFloatingPointType())
			{
				fir::Value* casted = this->builder.CreateBitcast(rhs, fir::PrimitiveType::getUintN(rhs->getType()->toPrimitiveType()->getFloatingPointBitWidth()));

				casted = this->builder.CreateIntSizeCast(casted, targetType);
				casted = this->builder.CreateIntToPointerCast(casted, valgep->getType()->getPointerElementType());
				this->builder.CreateStore(casted, valgep);
			}
			else
			{
				fir::Value* casted = this->builder.CreateBitcast(rhs, targetType);
				casted = this->builder.CreateIntToPointerCast(casted, valgep->getType()->getPointerElementType());
				this->builder.CreateStore(casted, valgep);
			}
		}

		return Result_t(this->builder.CreateLoad(lhsPtr), lhsPtr);
	}


	Result_t CodegenInstance::extractValueFromAny(fir::Type* type, fir::Value* ptr)
	{
		fir::Value* valgep = this->builder.CreateStructGEP(ptr, 1);
		fir::Value* loadedval = this->builder.CreateLoad(valgep);

		if(type->isStructType() || type->isClassType())
		{
			// use pointer stuff
			fir::Value* valptr = this->builder.CreatePointerTypeCast(loadedval, type->getPointerTo());
			fir::Value* loaded = this->builder.CreateLoad(valptr);

			return Result_t(loaded, valptr);
		}
		else
		{
			// the pointer is actually a literal
			fir::Type* targetType = type->isIntegerType() ? type : fir::PrimitiveType::getInt64(this->getContext());
			fir::Value* val = this->builder.CreatePointerToIntCast(loadedval, targetType);

			if(val->getType() != type)
			{
				if(type->isFloatingPointType()
					&& type->toPrimitiveType()->getFloatingPointBitWidth() != val->getType()->toPrimitiveType()->getIntegerBitWidth())
				{
					val = builder.CreateIntSizeCast(val, fir::PrimitiveType::getUintN(type->toPrimitiveType()->getFloatingPointBitWidth()));
				}

				val = this->builder.CreateBitcast(val, type);
			}

			return Result_t(val, 0);
		}
	}

	Result_t CodegenInstance::makeAnyFromValue(fir::Value* value, fir::Value* valuePtr)
	{
		TypePair_t* anyt = this->getTypeByString("Any");
		iceAssert(anyt);

		if(!valuePtr)
		{
			// valuePtr = this->getStackAlloc(value->getType(), "tempAlloca");
			// this->builder.CreateStore(value, valuePtr);
		}

		fir::Value* anyptr = this->getStackAlloc(anyt->first, "anyPtr");
		return this->assignValueToAny(anyptr, value, valuePtr);
	}




	Result_t CodegenInstance::createLLVariableArray(fir::Value* ptr, fir::Value* length)
	{
		iceAssert(ptr->getType()->isPointerType());
		iceAssert(length->getType()->isIntegerType());

		fir::LLVariableArrayType* arrType = fir::LLVariableArrayType::get(ptr->getType()->getPointerElementType());
		fir::Value* arr = this->getStackAlloc(arrType, "Any_Variadic_Array");

		fir::Value* ptrGEP = this->builder.CreateStructGEP(arr, 0);
		fir::Value* lenGEP = this->builder.CreateStructGEP(arr, 1);

		this->builder.CreateStore(ptr, ptrGEP);
		this->builder.CreateStore(length, lenGEP);

		return Result_t(this->builder.CreateLoad(arr), arr);
	}

	Result_t CodegenInstance::indexLLVariableArray(fir::Value* arr, fir::Value* index)
	{
		iceAssert(arr->getType()->isLLVariableArrayType());
		iceAssert(index->getType()->isIntegerType());

		fir::Value* ptrGEP = this->builder.CreateStructGEP(arr, 0);
		fir::Value* ptr = this->builder.CreateLoad(ptrGEP);

		// todo: bounds checking?

		fir::Value* gep = this->builder.CreateGEP2(ptr, fir::ConstantInt::getUint64(0), index);
		return Result_t(this->builder.CreateLoad(gep), gep);
	}

	Result_t CodegenInstance::getLLVariableArrayDataPtr(fir::Value* arrPtr)
	{
		iceAssert(arrPtr->getType()->isPointerType() && "not a pointer type");
		iceAssert(arrPtr->getType()->getPointerElementType()->isLLVariableArrayType());

		fir::Value* ptrGEP = this->builder.CreateStructGEP(arrPtr, 0);
		return Result_t(this->builder.CreateLoad(ptrGEP), ptrGEP);
	}

	Result_t CodegenInstance::getLLVariableArrayLength(fir::Value* arrPtr)
	{
		iceAssert(arrPtr->getType()->isPointerType() && "not a pointer type");
		iceAssert(arrPtr->getType()->getPointerElementType()->isLLVariableArrayType());

		fir::Value* lenGEP = this->builder.CreateStructGEP(arrPtr, 1);
		return Result_t(this->builder.CreateLoad(lenGEP), lenGEP);
	}









































	Result_t CodegenInstance::doPointerArithmetic(ArithmeticOp op, fir::Value* lhs, fir::Value* lhsPtr, fir::Value* rhs)
	{
		iceAssert(lhs->getType()->isPointerType() && rhs->getType()->isIntegerType()
		&& (op == ArithmeticOp::Add || op == ArithmeticOp::Subtract || op == ArithmeticOp::PlusEquals || op == ArithmeticOp::MinusEquals));

		// first, multiply the RHS by the number of bits the pointer type is, divided by 8
		// eg. if int16*, then +4 would be +4 int16s, which is (4 * (8 / 4)) = 4 * 2 = 8 bytes


		uint64_t ptrWidth = this->execTarget->getPointerWidthInBits();
		uint64_t typesize = this->execTarget->getTypeSizeInBits(lhs->getType()->getPointerElementType()) / 8;

		fir::Value* intval = fir::ConstantInt::getUnsigned(fir::PrimitiveType::getUintN(ptrWidth, this->getContext()), typesize);

		if(rhs->getType()->toPrimitiveType() != lhs->getType()->toPrimitiveType())
		{
			rhs = this->builder.CreateIntSizeCast(rhs, intval->getType());
		}


		// this is the properly adjusted int to add/sub by
		fir::Value* newrhs = this->builder.CreateMul(rhs, intval);

		// convert the lhs pointer to an int value, so we can add/sub on it
		fir::Value* ptrval = this->builder.CreatePointerToIntCast(lhs, newrhs->getType());

		// create the add/sub
		fir::Value* res = this->builder.CreateBinaryOp(op, ptrval, newrhs);

		// turn the int back into a pointer, so we can store it back into the var.
		fir::Value* tempRes = (lhsPtr && (op == ArithmeticOp::PlusEquals || op == ArithmeticOp::MinusEquals)) ?
			lhsPtr : this->getStackAlloc(lhs->getType());


		fir::Value* properres = this->builder.CreateIntToPointerCast(res, lhs->getType());
		this->builder.CreateStore(properres, tempRes);
		return Result_t(properres, tempRes);
	}


	static void _errorNoReturn(Expr* e)
	{
		error(e, "Not all code paths return a value");
	}

	static bool verifyReturnType(CodegenInstance* cgi, Func* f, BracedBlock* bb, Return* r, fir::Type* retType)
	{
		if(r)
		{
			fir::Type* expected = 0;
			fir::Type* have = 0;

			if(r->actualReturnValue)
			{
				have = r->actualReturnValue->getType();
			}

			if(!have)
			{
				have = cgi->getExprType(r->val);
			}


			if(retType == 0)
			{
				expected = cgi->getExprType(f->decl);
			}
			else
			{
				expected = retType;
			}


			int dist = cgi->getAutoCastDistance(have, expected);

			if(dist == -1)
				error(r, "Function has return type '%s', but return statement returned value of type '%s' instead",
					cgi->getReadableType(expected).c_str(), cgi->getReadableType(have).c_str());


			return true;
		}
		else
		{
			return false;
		}
	}

	static Return* recursiveVerifyBranch(CodegenInstance* cgi, Func* f, IfStmt* ifbranch, bool checkType, fir::Type* retType);
	static Return* recursiveVerifyBlock(CodegenInstance* cgi, Func* f, BracedBlock* bb, bool checkType, fir::Type* retType)
	{
		if(bb->statements.size() == 0)
			_errorNoReturn(bb);

		Return* r = nullptr;
		for(Expr* e : bb->statements)
		{
			IfStmt* i = nullptr;
			if((i = dynamic_cast<IfStmt*>(e)))
			{
				Return* tmp = recursiveVerifyBranch(cgi, f, i, checkType, retType);
				if(tmp)
				{
					r = tmp;
					break;
				}
			}

			else if((r = dynamic_cast<Return*>(e)))
				break;
		}

		if(checkType)
		{
			verifyReturnType(cgi, f, bb, r, retType);
		}

		return r;
	}

	static Return* recursiveVerifyBranch(CodegenInstance* cgi, Func* f, IfStmt* ib, bool checkType, fir::Type* retType)
	{
		Return* r = 0;
		bool first = true;
		for(std::pair<Expr*, BracedBlock*> pair : ib->_cases)	// use the preserved one
		{
			Return* tmp = recursiveVerifyBlock(cgi, f, pair.second, checkType, retType);
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


		bool isVoid = (retType == 0 ? this->getExprType(func) : retType)->isVoidType();

		// check the block
		if(func->block->statements.size() == 0 && !isVoid)
		{
			error(func, "Function %s has return type '%s', but returns nothing:\n%s", func->decl->ident.name.c_str(),
				func->decl->type.strType.c_str(), this->printAst(func->decl).c_str());
		}
		else if(isVoid)
		{
			return true;
		}


		// now loop through all exprs in the block
		Return* ret = 0;
		Expr* final = 0;
		for(Expr* e : func->block->statements)
		{
			if(stmtCounter)
				(*stmtCounter)++;

			IfStmt* i = dynamic_cast<IfStmt*>(e);
			final = e;

			if(i)
				ret = recursiveVerifyBranch(this, func, i, !isVoid && checkType, retType);

			// "top level" returns we will just accept.
			if(ret || (ret = dynamic_cast<Return*>(e)))
				break;
		}

		if(!ret && (isVoid || !checkType || this->getAutoCastDistance(this->getExprType(final), this->getExprType(func)) != -1))
			return true;

		if(!ret)
		{
			error(func, "Function '%s' missing return statement (implicit return invalid, need %s, got %s)", func->decl->ident.name.c_str(),
				this->getExprType(func)->str().c_str(), this->getExprType(final)->str().c_str());
		}

		if(checkType)
		{
			verifyReturnType(this, func, func->block, ret, retType);
		}

		return false;
	}














}
