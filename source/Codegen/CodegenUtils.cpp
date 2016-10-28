// LlvmCodeGen.cpp
// Copyright (c) 2014 - 2015, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>

#include <map>
#include <set>

#include "parser.h"
#include "codegen.h"
#include "compiler.h"

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
			cgi->execTarget = fir::ExecutionTarget::getLP64();

		else if(sizeof(void*) == 4)
			cgi->execTarget = fir::ExecutionTarget::getILP32();

		else
			error("enotsup: ptrsize = %zu", sizeof(void*));

		cgi->pushScope();


		cgi->rootNode->rootFuncStack->nsName = "__#root_" + cgi->module->getModuleName();

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

	void CodegenInstance::popScope()
	{
		this->symTabStack.pop_back();
		this->refCountingStack.pop_back();
	}

	void CodegenInstance::clearScope()
	{
		this->symTabStack.clear();
		this->refCountingStack.clear();
		this->clearNamespaceScope();
	}

	void CodegenInstance::pushScope()
	{
		this->symTabStack.push_back(SymTab_t());
		this->refCountingStack.push_back({ });
	}

	void CodegenInstance::addRefCountedValue(fir::Value* ptr)
	{
		iceAssert(ptr->getType()->isPointerType() && "refcounted value must be a pointer");
		this->refCountingStack.back().push_back(ptr);
	}

	void CodegenInstance::removeRefCountedValue(fir::Value* ptr)
	{
		auto it = std::find(this->refCountingStack.back().begin(), this->refCountingStack.back().end(), ptr);
		if(it == this->refCountingStack.back().end())
			error("ptr does not exist in refcounting stack, cannot remove");

		this->refCountingStack.back().erase(it);
	}

	void CodegenInstance::removeRefCountedValueIfExists(fir::Value* ptr)
	{
		auto it = std::find(this->refCountingStack.back().begin(), this->refCountingStack.back().end(), ptr);
		if(it == this->refCountingStack.back().end())
			return;

		this->refCountingStack.back().erase(it);
	}

	std::deque<fir::Value*> CodegenInstance::getRefCountedValues()
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
				else if(possibleGeneric->isParametricType())
				{
					// todo(leak): this leaks too
					return new TypePair_t(possibleGeneric, std::make_pair(nullptr, TypeKind::Parametric));
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















	void CodegenInstance::importOtherCgi(CodegenInstance* othercgi)
	{
		auto p = prof::Profile("importOtherCgi");
		this->importFunctionTreeInto(this->rootNode->rootFuncStack, othercgi->rootNode->rootFuncStack);
	}




	void CodegenInstance::importFunctionTreeInto(FunctionTree* ftree, FunctionTree* other)
	{
		// other is the source
		// ftree is the target

		ftree->nsName = other->nsName;
		{
			auto p = prof::Profile("import funcs");

			for(auto pair : other->funcs)
			{
				if(pair.funcDecl->attribs & Attr_VisPublic)
				{
					bool existing = ftree->funcSet.find(pair.firFunc->getName()) != ftree->funcSet.end();

					if(!existing)
					{
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
							ftree->funcSet.insert(pair.firFunc->getName());
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

				if(!existing && t.first != "Type" && t.first != "Any")
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

			for(auto sub : other->subs)
			{
				FunctionTree* found = ftree->subMap[sub->nsName];

				if(found)
				{
					this->importFunctionTreeInto(found, sub);
				}
				else
				{
					auto nft = new FunctionTree();
					this->importFunctionTreeInto(nft, sub);

					ftree->subMap[sub->nsName] = nft;
					ftree->subs.push_back(nft);
				}
			}
		}
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
			if(existing->subMap.find(namespc) != existing->subMap.end())
				found = true;


			// for(auto s : existing->subs)
			// {
			// 	if(s->nsName == namespc)
			// 	{
			// 		found = true;
			// 		break;
			// 	}
			// }

			if(!found)
			{
				FunctionTree* ft = new FunctionTree();
				ft->nsName = namespc;

				existing->subs.push_back(ft);
				existing->subMap[namespc] = ft;
			}
		}

		this->namespaceStack.push_back(namespc);
	}

	void CodegenInstance::popNamespaceScope()
	{
		this->namespaceStack.pop_back();
	}

	void CodegenInstance::addFunctionToScope(FuncDefPair func, FunctionTree* root)
	{
		FunctionTree* cur = root;
		if(!cur)
			cur = this->getCurrentFuncTree();

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
		FunctionTree* cur = this->getCurrentFuncTree();
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











	std::deque<FuncDefPair> CodegenInstance::resolveFunctionName(std::string basename)
	{
		std::deque<std::string> curDepth = this->namespaceStack;
		std::deque<FuncDefPair> candidates;


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
					if(a.funcDecl->params[i]->getType(this, true) != b.funcDecl->params[i]->getType(this, true))
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



		for(size_t i = 0; i <= this->namespaceStack.size(); i++)
		{
			FunctionTree* ft = this->getCurrentFuncTree(&curDepth, this->rootNode->rootFuncStack);
			if(!ft) break;

			for(auto f : ft->funcs)
			{
				auto isDupe = [this, f, _isDupe](FuncDefPair fp) -> bool {
					return _isDupe(f, fp);
				};

				if(f.funcDecl->genericTypes.size() == 0
					&& (f.funcDecl ? f.funcDecl->ident.name : f.firFunc->getName().str()) == basename)
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



	Resolved_t CodegenInstance::resolveFunctionFromList(Expr* user, std::deque<FuncDefPair> list, std::string basename,
		std::deque<Expr*> params, bool exactMatch)
	{
		std::deque<fir::Type*> argTypes;
		for(auto e : params)
			argTypes.push_back(e->getType(this, true));

		return this->resolveFunctionFromList(user, list, basename, argTypes, exactMatch);
	}



	Resolved_t CodegenInstance::resolveFunctionFromList(Expr* user, std::deque<FuncDefPair> list, std::string basename,
		std::deque<fir::Type*> params, bool exactMatch)
	{
		std::deque<FuncDefPair> candidates = list;
		if(candidates.size() == 0) return Resolved_t();

		std::deque<std::pair<FuncDefPair, int>> finals;
		for(auto c : candidates)
		{
			int distance = 0;

			// note: if we don't provide the FuncDecl, assume we have everything down, including the basename.
			if((c.funcDecl ? c.funcDecl->ident.name : basename) == basename && this->isValidFuncOverload(c, params, &distance, exactMatch))
				finals.push_back({ c, distance });
		}

		// disambiguate this.
		// with casting distance.
		if(finals.size() > 1)
		{
			// go through each.
			std::deque<std::pair<FuncDefPair, int>> mostViable;
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
					if(c.first.funcDef)
						cstr += this->printAst(c.first.funcDecl) + "\n";
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
		std::deque<FuncDefPair> candidates = this->resolveFunctionName(basename);
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

			// check for direct forwarding case
			if(args.size() == funcParams.size())
			{
				if(args.back()->isLLVariableArrayType() && funcParams.back()->isLLVariableArrayType()
					&& args.back()->toLLVariableArrayType()->getElementType() == funcParams.back()->toLLVariableArrayType()->getElementType())
				{
					// yes, do that (where that == nothing)
					*_dist += 0;
					return true;
				}
			}




			// 3. get the type of the vararg array.
			fir::Type* funcLLType = funcParams.back();
			iceAssert(funcLLType->isLLVariableArrayType());

			fir::Type* llElmType = funcLLType->toLLVariableArrayType()->getElementType();

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

	bool CodegenInstance::isValidFuncOverload(FuncDefPair fp, std::deque<fir::Type*> argTypes, int* castingDistance, bool exactMatch)
	{
		iceAssert(castingDistance);
		std::deque<fir::Type*> funcParams;


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
				auto t = arg->getType(this, true);
				if(!t) return false;

				funcParams.push_back(t);
			}

			iscvar = fp.funcDecl->isCStyleVarArg;
			isvar = fp.funcDecl->isVariadic;
		}

		return _checkFunction(this, funcParams, argTypes, castingDistance, isvar, iscvar, exactMatch);
	}























	void CodegenInstance::clearNamespaceScope()
	{
		this->namespaceStack.clear();
	}



	fir::Function* CodegenInstance::getOrDeclareLibCFunc(std::string name)
	{
		if(name == "malloc")
		{
			return this->module->getOrCreateFunction(Identifier("malloc", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt64() }, fir::Type::getInt8Ptr(), false), fir::LinkageType::External);
		}
		else if(name == "free")
		{
			return this->module->getOrCreateFunction(Identifier("free", IdKind::Name),
				fir::FunctionType::get({ fir::Type::getInt8Ptr() }, fir::Type::getVoid(), false), fir::LinkageType::External);
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
			fir::Type* atype = arg->getType(this, true);	// same as mangleFunctionName, but allow failures.

			// if there is no proper type, go ahead with the raw type: T or U or something.
			if(!atype)
			{
				std::string st = arg->ptype->str();
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
			fir::Type* atype = arg->getType(this, true);	// same as mangleFunctionName, but allow failures.

			// if there is no proper type, go ahead with the raw type: T or U or something.
			if(!atype)
			{
				std::string st = arg->ptype->str();
				iceAssert(uniqueGenericTypes.find(st) != uniqueGenericTypes.end());

				std::string s = "GT" + std::to_string(uniqueGenericTypes[st]);
				strs.push_back(std::to_string(s.length()) + s);
			}
			else
			{
				std::string mangled = atype->encodedStr();

				if(atype->isLLVariableArrayType())
				{
					mangled = "V" + atype->toLLVariableArrayType()->encodedStr();
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


















	#if 0
	bool CodegenInstance::isDuplicateFuncDecl(FuncDecl* decl)
	{
		if(decl->isFFI) return false;

		std::deque<Expr*> es;
		for(auto p : decl->params) es.push_back(p);

		Resolved_t res = this->resolveFunction(decl, decl->ident.name, es, true);
		if(res.resolved && res.t.firFunc != 0 && res.t.funcDecl != decl)
		{
			fprintf(stderr, "Duplicate function: %s\n", this->printAst(res.t.funcDecl).c_str());
			for(size_t i = 0; i < __min(decl->params.size(), res.t.funcDecl->params.size()); i++)
			{
				info(res.t.funcDecl, "%zu: %s, %s", i, decl->params[i]->getType(this)->str().c_str(),
					res.t.funcDecl->params[i]->getType(this)->str().c_str());
			}

			return true;
		}
		else
		{
			return false;
		}
	}
	#endif

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


		// get rid of this stupid literal nonsense
		for(size_t i = 0; i < args.size(); i++)
		{
			if(args[i]->isPrimitiveType() && args[i]->toPrimitiveType()->isLiteralType())
				args[i] = args[i]->toPrimitiveType()->getUnliteralType();
		}


		// now check if we *can* instantiate it.
		// first check the number of arguments.

		bool didSelfParam = false;
		fir::Type* originalFirstParam = 0;

		if(candidate->params.size() != args.size())
		{
			// if it's not variadic, and it's either a normal function (no parent class) or is a static method,
			// then there's no reason for the parameters to mismatch.
			if(!candidate->isVariadic && (!candidate->parentClass || candidate->isStatic))
				return false;

			else if(candidate->parentClass && !candidate->isStatic)
			{
				// make sure it's only one off
				if(args.size() < candidate->params.size() || args.size() - candidate->params.size() > 1)
					return false;

				didSelfParam = true;
				iceAssert(args.front()->isPointerType() && args.front()->getPointerElementType()->isClassType() && "what, no.");

				originalFirstParam = args.front();
				args.pop_front();

				iceAssert(candidate->params.size() == args.size());
			}
		}



		// param count matches...
		// do a similar thing as the actual mangling -- build a list of
		// uniquely named types.

		// string is name, map is a map from position to indirs.
		std::map<std::string, std::map<int, int>> typePositions;
		std::map<int, std::pair<std::string, int>> revTypePositions;

		std::vector<int> nonGenericTypes;



		// if this is variadic, remove the last parameter from the candidate (we'll add it back later)
		// so we only check the first n - 1 parameters.
		VarDecl* varParam = 0;
		if(candidate->isVariadic)
		{
			varParam = candidate->params.back();
			candidate->params.pop_back();
		}



		int pos = 0;
		for(auto p : candidate->params)
		{
			int indirs = 0;
			std::string s = p->ptype->str();
			s = unwrapPointerType(s, &indirs);

			if(candidate->genericTypes.find(s) != candidate->genericTypes.end())
			{
				typePositions[s][pos] = indirs;
				revTypePositions[pos] = { s, indirs };
			}
			else
			{
				nonGenericTypes.push_back(pos);
			}

			pos++;
		}


		// since this is bound by the size of the candidates, we'll only check the non-var parameters
		// this ensures we don't array out of bounds.
		std::map<std::string, fir::Type*> checked;
		for(size_t i = 0; i < candidate->params.size(); i++)
		{
			if(revTypePositions.find(i) != revTypePositions.end())
			{
				// check that the generic types match (ie. all the Ts are the same type, pointerness, etc)
				std::string s = revTypePositions[i].first;
				int indirs = revTypePositions[i].second;

				fir::Type* ftype = args[i];

				int givenindrs = 0;
				while(ftype->isPointerType())
				{
					ftype = ftype->getPointerElementType();
					givenindrs++;
				}

				// check that the base types match
				// eg. if we have (T*, T*), make sure the T is the same, don't be having like int8* and String*.
				// ftype here has been reduced.

				if(checked.find(s) != checked.end())
				{
					if(ftype != checked[s])
						return false;
				}
				else
				{
					checked[s] = ftype;
				}


				// check that we have 'enough' pointerness
				// eg. if we have T**, make sure we pass at least a double-indirected thing, or more (triple, etc.)
				if(givenindrs < indirs)
					return false;
			}
			else
			{
				// check normal types.
				fir::Type* a = args[i];
				fir::Type* b = candidate->params[i]->getType(cgi);

				if(a != b) return false;
			}
		}





		// fill in the typemap.
		// note that it's okay if we just have one -- if we did this loop more
		// than once and screwed up the tm, that means we have more than one
		// candidate, and will error anyway.

		for(auto pair : typePositions)
		{
			int pos = pair.second.begin()->first;
			int indrs = pair.second.begin()->second;

			fir::Type* t = args[pos];
			for(int i = 0; i < indrs; i++)
				t = t->getPointerElementType();

			thistm[pair.first] = t;
		}


		// last phase: ensure the type constraints are met
		for(auto cst : thistm)
		{
			TypeConstraints_t constr = candidate->genericTypes[cst.first];

			for(auto protstr : constr.protocols)
			{
				ProtocolDef* prot = cgi->resolveProtocolName(candidate, protstr);
				iceAssert(prot);

				bool doesConform = prot->checkTypeConformity(cgi, cst.second);

				if(!doesConform)
					return false;
			}
		}


		// if we're variadic -- check it.
		if(varParam != 0)
		{
			// add it back first.
			candidate->params.push_back(varParam);

			// get the type.
			int indirs = 0;
			std::string base = varParam->ptype->toVariadicArrayType()->base->str();
			base = unwrapPointerType(base, &indirs);


			if(typePositions.find(base) != typePositions.end())
			{
				// already have it
				// ensure it matches with the ones we've already found.

				fir::Type* resolved = thistm[base];
				iceAssert(resolved);



				if(args.size() >= candidate->params.size())
				{
					// again, check the direct forwarding case
					if(args.size() == candidate->params.size() && args.back()->isLLVariableArrayType()
						&& args.back()->toLLVariableArrayType()->getElementType() == resolved)
					{
						// direct.
						// do nothing.
					}
					else
					{
						for(size_t i = candidate->params.size() - 1; i < args.size(); i++)
						{
							if(args[i] != resolved)
								return false;
						}

						// should be fine now.
					}
				}
				else
				{
					// no varargs were even given
					// since we have already inferred the type from the other parameters,
					// we can give this a free pass.
				}
			}
			else if(candidate->genericTypes.find(base) != candidate->genericTypes.end())
			{
				// ok, we need to be able to deduce the type from the vararg only.
				// so if none were provided, then give up.

				if(args.size() < candidate->params.size())
					return false;


				// great, now just deduce it.
				// we just need to make sure all the Ts match, and the number of indirections
				// match *and* are greater than or equal to the specified level.

				fir::Type* first = args[candidate->params.size() - 1];

				// first, make sure the indirections tally
				int givenindrs = 0;
				if(first->isPointerType())
					givenindrs = first->toPointerType()->getIndirections();

				if(givenindrs < indirs)
					return false;


				// ok, check the type itself
				for(size_t i = candidate->params.size() - 1; i < args.size(); i++)
				{
					if(args[i] != first)
						return false;
				}

				// ok now.
				fir::Type* reduced = first;
				while(reduced->isPointerType())
					reduced = reduced->getPointerElementType();

				thistm[base] = reduced;
			}
		}







		// check that we actually have an entry for every type
		for(auto t : candidate->genericTypes)
		{
			if(thistm.find(t.first) == thistm.end())
				return false;
		}


		*gtm = thistm;
		return true;
	}



	FuncDefPair CodegenInstance::instantiateGenericFunctionUsingParameters(Expr* user, std::map<std::string, fir::Type*> _gtm,
		Func* func, std::deque<fir::Type*> params)
	{
		iceAssert(func);
		iceAssert(func->decl);

		FuncDecl* fnDecl = func->decl;

		std::map<std::string, fir::Type*> gtm = _gtm;
		if(gtm.empty())
		{
			bool res = _checkGenericFunction(this, &gtm, func->decl, params);
			if(!res) return FuncDefPair::empty();
		}


		bool needToCodegen = true;
		if(this->reifiedGenericFunctions.find({ func, gtm }) != this->reifiedGenericFunctions.end())
			needToCodegen = false;




		// we need to push a new "generic type stack", and add the types that we resolved into it.
		// todo: might be inefficient.
		// todo: look into creating a version of pushGenericTypeStack that accepts a std::map<string, fir::Type*>
		// so we don't have to iterate etc etc.
		// I don't want to access cgi->instantiatedGenericTypeStack directly.



		fir::Function* ffunc = nullptr;
		if(needToCodegen)
		{
			Result_t res = fnDecl->generateDeclForGenericFunction(this, gtm);
			ffunc = (fir::Function*) res.value;

			this->reifiedGenericFunctions[{ func, gtm }] = ffunc;
		}
		else
		{
			ffunc = this->reifiedGenericFunctions[{ func, gtm }];
			iceAssert(ffunc);
		}

		iceAssert(ffunc);

		this->pushGenericTypeStack();
		for(auto pair : gtm)
			this->pushGenericType(pair.first, pair.second);

		if(needToCodegen)
		{
			// dirty: use 'lhsPtr' to pass the version we want.
			func->codegen(this, ffunc);
		}

		this->removeFunctionFromScope(FuncDefPair(0, func->decl, func));
		this->popGenericTypeStack();

		return FuncDefPair(ffunc, func->decl, func);
	}


	FuncDefPair CodegenInstance::tryResolveGenericFunctionCallUsingCandidates(FuncCall* fc, std::deque<Func*> candidates)
	{
		// try and resolve shit
		std::map<std::string, fir::Type*> gtm;

		if(candidates.size() == 0)
		{
			return FuncDefPair::empty();	// just fail
		}

		std::deque<fir::Type*> fargs;
		for(auto p : fc->params)
			fargs.push_back(p->getType(this));

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
			return FuncDefPair::empty();
		}
		else if(candidates.size() > 1)
		{
			std::string cands;
			for(auto c : candidates)
				cands += this->printAst(c->decl) + "\n";

			error(fc, "Ambiguous instantiation of parametric function %s, have %zd candidates:\n%s\n", fc->name.c_str(),
				candidates.size(), cands.c_str());
		}

		return this->instantiateGenericFunctionUsingParameters(fc, gtm, candidates[0], fargs);
	}

	FuncDefPair CodegenInstance::tryResolveGenericFunctionCall(FuncCall* fc)
	{
		std::deque<Func*> candidates = this->findGenericFunctions(fc->name);
		return this->tryResolveGenericFunctionCallUsingCandidates(fc, candidates);
	}


	FuncDefPair CodegenInstance::tryResolveGenericFunctionFromCandidatesUsingFunctionType(Expr* user, std::deque<Func*> candidates,
		fir::FunctionType* ft)
	{
		std::deque<FuncDefPair> ret;
		for(auto fn : candidates)
		{
			auto fp = this->instantiateGenericFunctionUsingParameters(user, { }, fn, ft->getArgumentTypes());
			if(fp.firFunc && fp.funcDef)
				ret.push_back(fp);
		}

		if(ret.empty())
		{
			return FuncDefPair::empty();
		}
		else if(candidates.size() > 1)
		{
			std::string cands;
			for(auto c : candidates)
				cands += this->printAst(c->decl) + "\n";

			error(user, "Ambiguous instantiation of parametric function %s, have %zd candidates:\n%s\n",
				ret.front().funcDecl->ident.name.c_str(), candidates.size(), cands.c_str());
		}

		return ret.front();
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
						this->irb.CreateReturn(fir::ConstantValue::getNullValue(pair->first));

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

			std::deque<FuncDefPair> fns;
			for(auto f : sb->initFuncs)
				fns.push_back(FuncDefPair(f, 0, 0));

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
						fns.push_back(FuncDefPair(f, 0, 0));
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

			auto ret = this->module->getFunction(res.t.firFunc->getName());
			iceAssert(ret);

			return ret;
		}
		else
		{
			error(user, "Invalid expr type (%s)", typeid(*pair->second.first).name());
		}
	}








	fir::Function* CodegenInstance::getFunctionFromModuleWithName(Identifier id, Expr* user)
	{
		auto list = this->module->getFunctionsWithName(id);
		if(list.empty()) return 0;

		else if(list.size() > 1)
			error(user, "Searched for ambiguous function by name '%s'", id.str().c_str());

		return list.front();
	}

	fir::Function* CodegenInstance::getFunctionFromModuleWithNameAndType(Identifier id, fir::FunctionType* ft, Expr* user)
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
		std::deque<fir::Function*> ret;

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












	Result_t CodegenInstance::assignValueToAny(fir::Value* lhsPtr, fir::Value* rhs, fir::Value* rhsPtr)
	{
		fir::Value* typegep = this->irb.CreateStructGEP(lhsPtr, 0, "anyGEP");	// Any
		typegep = this->irb.CreateStructGEP(typegep, 0, "any_TypeGEP");			// Type

		size_t index = TypeInfo::getIndexForType(this, rhs->getType());
		iceAssert(index > 0);

		fir::Value* constint = fir::ConstantInt::get(typegep->getType()->getPointerElementType(), index);
		this->irb.CreateStore(constint, typegep);



		fir::Value* valgep = this->irb.CreateStructGEP(lhsPtr, 1);
		if(rhsPtr)
		{
			fir::Value* casted = this->irb.CreatePointerTypeCast(rhsPtr, valgep->getType()->getPointerElementType());
			this->irb.CreateStore(casted, valgep);
		}
		else
		{
			fir::Type* targetType = rhs->getType()->isIntegerType() ? valgep->getType()->getPointerElementType() :
				fir::Type::getInt64(this->getContext());


			if(rhs->getType()->isIntegerType())
			{
				fir::Value* casted = this->irb.CreateIntToPointerCast(rhs, targetType);
				this->irb.CreateStore(casted, valgep);
			}
			else if(rhs->getType()->isFloatingPointType())
			{
				fir::Value* casted = this->irb.CreateBitcast(rhs, fir::PrimitiveType::getUintN(rhs->getType()->toPrimitiveType()->getFloatingPointBitWidth()));

				casted = this->irb.CreateIntSizeCast(casted, targetType);
				casted = this->irb.CreateIntToPointerCast(casted, valgep->getType()->getPointerElementType());
				this->irb.CreateStore(casted, valgep);
			}
			else
			{
				fir::Value* casted = this->irb.CreateBitcast(rhs, targetType);
				casted = this->irb.CreateIntToPointerCast(casted, valgep->getType()->getPointerElementType());
				this->irb.CreateStore(casted, valgep);
			}
		}

		return Result_t(this->irb.CreateLoad(lhsPtr), lhsPtr);
	}


	Result_t CodegenInstance::extractValueFromAny(fir::Type* type, fir::Value* ptr)
	{
		fir::Value* valgep = this->irb.CreateStructGEP(ptr, 1);
		fir::Value* loadedval = this->irb.CreateLoad(valgep);

		if(type->isStructType() || type->isClassType())
		{
			// use pointer stuff
			fir::Value* valptr = this->irb.CreatePointerTypeCast(loadedval, type->getPointerTo());
			fir::Value* loaded = this->irb.CreateLoad(valptr);

			return Result_t(loaded, valptr);
		}
		else
		{
			// the pointer is actually a literal
			fir::Type* targetType = type->isIntegerType() ? type : fir::Type::getInt64(this->getContext());
			fir::Value* val = this->irb.CreatePointerToIntCast(loadedval, targetType);

			if(val->getType() != type)
			{
				if(type->isFloatingPointType()
					&& type->toPrimitiveType()->getFloatingPointBitWidth() != val->getType()->toPrimitiveType()->getIntegerBitWidth())
				{
					val = this->irb.CreateIntSizeCast(val, fir::PrimitiveType::getUintN(type->toPrimitiveType()->getFloatingPointBitWidth()));
				}

				val = this->irb.CreateBitcast(val, type);
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
			// this->irb.CreateStore(value, valuePtr);
		}

		fir::Value* anyptr = this->getStackAlloc(anyt->first, "anyPtr");
		return this->assignValueToAny(anyptr, value, valuePtr);
	}




	Result_t CodegenInstance::makeStringLiteral(std::string str)
	{
		iceAssert(str.length() < INT32_MAX && "wtf? 4gb string?");
		fir::Value* strp = this->irb.CreateStackAlloc(fir::Type::getStringType());

		// note(portability): this isn't going to work on non-2's-complement platforms
		// where 0xFFFFFFFFFFFFFFFF != -1.

		// if this basic assumption fails however, then everything is out the window, and IDGAF anymore.

		// basically, this inserts a "-1" where the refcount for heap-strings would normally go
		// this simplifies code a lot (generated code, too)

		std::string s = str;
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);
		s.insert(s.begin(), 0xFF);

		fir::Value* empty = this->module->createGlobalString(s);

		empty = this->irb.CreatePointerAdd(this->irb.CreateConstGEP2(empty, 0, 0), fir::ConstantInt::getInt64(8));

		fir::Value* len = fir::ConstantInt::getInt64(str.length());

		this->irb.CreateSetStringData(strp, empty);
		this->irb.CreateSetStringLength(strp, len);

		strp->makeImmutable();
		return Result_t(this->irb.CreateLoad(strp), strp);
	}



	Result_t CodegenInstance::getEmptyString()
	{
		return this->makeStringLiteral("");
	}

	Result_t CodegenInstance::getNullString()
	{
		return this->makeStringLiteral("(null)");
	}






	#define BUILTIN_STRINGREF_INCR_FUNC_NAME		"__.stringref_incr"
	#define BUILTIN_STRINGREF_DECR_FUNC_NAME		"__.stringref_decr"
	#define BUILTIN_STRING_APPEND_FUNC_NAME			"__.string_append"
	#define BUILTIN_STRING_APPEND_CHAR_FUNC_NAME	"__.string_appendchar"
	#define BUILTIN_STRING_CMP_FUNC_NAME			"__.string_cmp"

	#define BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME	"__.string_checkliteralmodify"
	#define BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME	"__.string_boundscheck"

	#define DEBUG_ARC 0

	fir::Function* CodegenInstance::getStringAppendFunction()
	{
		fir::Function* appendf = this->module->getFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getStringType()->getPointerTo() },
				fir::Type::getStringType(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];



			// add two strings
			// steps:
			//
			// 1. get the size of the left string
			// 2. get the size of the right string
			// 3. add them together
			// 4. malloc a string of that size + 1
			// 5. make a new string
			// 6. set the buffer to the malloced buffer
			// 7. set the length to a + b
			// 8. return.

			// get an empty string
			fir::Value* newstrp = this->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = this->irb.CreateGetStringLength(s1, "l1");
			fir::Value* rhslen = this->irb.CreateGetStringLength(s2, "l2");

			fir::Value* lhsbuf = this->irb.CreateGetStringData(s1, "d1");
			fir::Value* rhsbuf = this->irb.CreateGetStringData(s2, "d2");

			// ok. combine the lengths
			fir::Value* newlen = this->irb.CreateAdd(lhslen, rhslen);

			// space for null + refcount
			size_t i64Size = this->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = this->irb.CreateAdd(newlen, fir::ConstantInt::getInt64(1 + i64Size));

			// now malloc.
			fir::Function* mallocf = this->module->getFunction(this->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = this->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = this->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = this->module->getIntrinsicFunction("memcpy");
			this->irb.CreateCall(memcpyf, { buf, lhsbuf, this->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = this->irb.CreatePointerAdd(buf, lhslen);
			this->irb.CreateCall(memcpyf, { offsetbuf, rhsbuf, this->irb.CreateIntSizeCast(rhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			// null terminator
			fir::Value* nt = this->irb.CreateGetPointer(offsetbuf, rhslen);
			this->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if 0
			{
				fir::Value* tmpstr = this->module->createGlobalString("malloc: %p / %p (%s)\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, buf, tmp, buf });
			}
			#endif

			// ok, now fix it
			this->irb.CreateSetStringData(newstrp, buf);
			this->irb.CreateSetStringLength(newstrp, newlen);
			this->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));

			this->irb.CreateReturn(this->irb.CreateLoad(newstrp));

			appendf = func;
			this->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}

	fir::Function* CodegenInstance::getStringCharAppendFunction()
	{
		fir::Function* appendf = this->module->getFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name));

		if(!appendf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_APPEND_CHAR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getCharType() },
				fir::Type::getStringType(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			// add a char to a string
			// steps:
			//
			// 1. get the size of the left string
			// 2. malloc a string of that size + 1=2
			// 3. make a new string
			// 4. set the buffer to the malloced buffer
			// 5. memcpy.
			// 6. set the length to a + b
			// 7. return.

			// get an empty string
			fir::Value* newstrp = this->irb.CreateStackAlloc(fir::Type::getStringType());
			newstrp->setName("newstrp");

			iceAssert(s1);
			iceAssert(s2);

			fir::Value* lhslen = this->irb.CreateGetStringLength(s1, "l1");
			fir::Value* lhsbuf = this->irb.CreateGetStringData(s1, "d1");


			// space for null (1) + refcount (i64size) + the char (another 1)
			size_t i64Size = this->execTarget->getTypeSizeInBytes(fir::Type::getInt64());
			fir::Value* malloclen = this->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(2 + i64Size));

			// now malloc.
			fir::Function* mallocf = this->module->getFunction(this->getOrDeclareLibCFunc("malloc")->getName());
			iceAssert(mallocf);

			fir::Value* buf = this->irb.CreateCall1(mallocf, malloclen);

			// move it forward (skip the refcount)
			buf = this->irb.CreatePointerAdd(buf, fir::ConstantInt::getInt64(i64Size));

			// now memcpy
			fir::Function* memcpyf = this->module->getIntrinsicFunction("memcpy");
			this->irb.CreateCall(memcpyf, { buf, lhsbuf, this->irb.CreateIntSizeCast(lhslen, fir::Type::getInt64()),
				fir::ConstantInt::getInt32(0), fir::ConstantInt::getBool(0) });

			fir::Value* offsetbuf = this->irb.CreatePointerAdd(buf, lhslen);

			// store the char.
			fir::Value* ch = this->irb.CreateBitcast(s2, fir::Type::getInt8());
			this->irb.CreateStore(ch, offsetbuf);

			// null terminator
			fir::Value* nt = this->irb.CreateGetPointer(offsetbuf, fir::ConstantInt::getInt64(1));
			this->irb.CreateStore(fir::ConstantInt::getInt8(0), nt);

			#if 0
			{
				fir::Value* tmpstr = this->module->createGlobalString("malloc: %p / %p (%s)\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, buf, tmp, buf });
			}
			#endif

			// ok, now fix it
			this->irb.CreateSetStringData(newstrp, buf);
			this->irb.CreateSetStringLength(newstrp, this->irb.CreateAdd(lhslen, fir::ConstantInt::getInt64(1)));
			this->irb.CreateSetStringRefCount(newstrp, fir::ConstantInt::getInt64(1));

			this->irb.CreateReturn(this->irb.CreateLoad(newstrp));

			appendf = func;
			this->irb.setCurrentBlock(restore);
		}

		iceAssert(appendf);
		return appendf;
	}







	fir::Function* CodegenInstance::getStringCompareFunction()
	{
		fir::Function* cmpf = this->module->getFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name));

		if(!cmpf)
		{
			// great.
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CMP_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getStringType()->getPointerTo() },
				fir::Type::getInt64(), false), fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			this->irb.setCurrentBlock(entry);

			fir::Value* s1 = func->getArguments()[0];
			fir::Value* s2 = func->getArguments()[1];

			/*
				int strcmp(const char* s1, const char* s2)
				{
					while(*s1 && (*s1 == *s2))
						s1++, s2++;

					return *(const unsigned char*) s1 - *(const unsigned char*) s2;
				}
			*/

			{
				fir::Value* str1p = this->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				this->irb.CreateStore(this->irb.CreateGetStringData(s1, "s1"), str1p);

				fir::Value* str2p = this->irb.CreateStackAlloc(fir::Type::getInt8Ptr());
				this->irb.CreateStore(this->irb.CreateGetStringData(s2, "s2"), str2p);


				fir::IRBlock* loopcond = this->irb.addNewBlockInFunction("cond1", func);
				fir::IRBlock* loopincr = this->irb.addNewBlockInFunction("loopincr", func);
				fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

				this->irb.CreateUnCondBranch(loopcond);
				this->irb.setCurrentBlock(loopcond);
				{
					fir::IRBlock* cond2 = this->irb.addNewBlockInFunction("cond2", func);

					fir::Value* str1 = this->irb.CreateLoad(str1p);
					fir::Value* str2 = this->irb.CreateLoad(str2p);

					// make sure ptr1 is not null
					fir::Value* cnd = this->irb.CreateICmpNEQ(this->irb.CreateLoad(str1), fir::ConstantInt::getInt8(0));
					this->irb.CreateCondBranch(cnd, cond2, merge);

					this->irb.setCurrentBlock(cond2);
					{
						// check that they are equal
						fir::Value* iseq = this->irb.CreateICmpEQ(this->irb.CreateLoad(str1), this->irb.CreateLoad(str2));
						this->irb.CreateCondBranch(iseq, loopincr, merge);
					}


					this->irb.setCurrentBlock(loopincr);
					{
						// increment str1 and str2
						fir::Value* v1 = this->irb.CreatePointerAdd(str1, fir::ConstantInt::getInt64(1));
						fir::Value* v2 = this->irb.CreatePointerAdd(str2, fir::ConstantInt::getInt64(1));

						this->irb.CreateStore(v1, str1p);
						this->irb.CreateStore(v2, str2p);

						this->irb.CreateUnCondBranch(loopcond);
					}
				}

				this->irb.setCurrentBlock(merge);
				fir::Value* ret = this->irb.CreateSub(this->irb.CreateLoad(this->irb.CreateLoad(str1p)),
					this->irb.CreateLoad(this->irb.CreateLoad(str2p)));

				ret = this->irb.CreateIntSizeCast(ret, func->getReturnType());

				this->irb.CreateReturn(ret);
			}

			cmpf = func;
			this->irb.setCurrentBlock(restore);
		}

		iceAssert(cmpf);
		return cmpf;
	}






	fir::Function* CodegenInstance::getStringRefCountIncrementFunction()
	{
		fir::Function* incrf = this->module->getFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name));

		if(!incrf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_INCR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* getref = this->irb.addNewBlockInFunction("getref", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);
			this->irb.setCurrentBlock(entry);

			// if ptr is 0, we exit early.
			{
				fir::Value* ptr = this->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = this->irb.CreateICmpEQ(ptr, fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));

				this->irb.CreateCondBranch(cond, merge, getref);
			}


			this->irb.setCurrentBlock(getref);
			fir::Value* curRc = this->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// never increment the refcount if this is a string literal
			// how do we know? the refcount was -1 to begin with.

			// check.
			fir::IRBlock* doadd = this->irb.addNewBlockInFunction("doref", func);
			{
				fir::Value* cond = this->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				this->irb.CreateCondBranch(cond, merge, doadd);
			}

			this->irb.setCurrentBlock(doadd);
			fir::Value* newRc = this->irb.CreateAdd(curRc, fir::ConstantInt::getInt64(1));
			this->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_ARC
			{
				fir::Value* tmpstr = this->module->createGlobalString("(incr) new rc of %p (%s) = %d\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				auto bufp = this->irb.CreateGetStringData(func->getArguments()[0]);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, bufp, bufp,
					newRc });
			}
			#endif

			this->irb.CreateUnCondBranch(merge);
			this->irb.setCurrentBlock(merge);
			this->irb.CreateReturnVoid();

			this->irb.setCurrentBlock(restore);

			incrf = func;
		}

		iceAssert(incrf);
		return incrf;
	}


	fir::Function* CodegenInstance::getStringRefCountDecrementFunction()
	{
		fir::Function* decrf = this->module->getFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name));

		if(!decrf)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRINGREF_DECR_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			func->setAlwaysInline();

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* checkneg = this->irb.addNewBlockInFunction("checkneg", func);
			fir::IRBlock* dotest = this->irb.addNewBlockInFunction("dotest", func);
			fir::IRBlock* dealloc = this->irb.addNewBlockInFunction("deallocate", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);


			// note:
			// if the ptr is 0, we exit immediately
			// if the refcount is -1, we exit as well.


			this->irb.setCurrentBlock(entry);
			{
				fir::Value* ptr = this->irb.CreateGetStringData(func->getArguments()[0]);
				fir::Value* cond = this->irb.CreateICmpEQ(ptr, fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));

				this->irb.CreateCondBranch(cond, merge, checkneg);
			}


			// needs to handle freeing the thing.
			this->irb.setCurrentBlock(checkneg);
			fir::Value* curRc = this->irb.CreateGetStringRefCount(func->getArguments()[0]);

			// check.
			{
				fir::Value* cond = this->irb.CreateICmpLT(curRc, fir::ConstantInt::getInt64(0));
				this->irb.CreateCondBranch(cond, merge, dotest);
			}


			this->irb.setCurrentBlock(dotest);
			fir::Value* newRc = this->irb.CreateSub(curRc, fir::ConstantInt::getInt64(1));
			this->irb.CreateSetStringRefCount(func->getArguments()[0], newRc);

			#if DEBUG_ARC
			{
				fir::Value* tmpstr = this->module->createGlobalString("(decr) new rc of %p (%s) = %d\n");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				auto bufp = this->irb.CreateGetStringData(func->getArguments()[0]);

				this->irb.CreateCall(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), { tmpstr, bufp, bufp,
					newRc });
			}
			#endif

			{
				fir::Value* cond = this->irb.CreateICmpEQ(newRc, fir::ConstantInt::getInt64(0));
				this->irb.CreateCondBranch(cond, dealloc, merge);

				this->irb.setCurrentBlock(dealloc);

				// call free on the buffer.
				fir::Value* bufp = this->irb.CreateGetStringData(func->getArguments()[0]);


				#if DEBUG_ARC
				{
					fir::Value* tmpstr = this->module->createGlobalString("free %p (%s)\n");
					tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);
					this->irb.CreateCall3(this->module->getFunction(this->getOrDeclareLibCFunc("printf").firFunc->getName()), tmpstr,
						bufp, bufp);
				}
				#endif



				fir::Function* freefn = this->module->getFunction(this->getOrDeclareLibCFunc("free")->getName());
				iceAssert(freefn);

				this->irb.CreateCall1(freefn, this->irb.CreatePointerAdd(bufp, fir::ConstantInt::getInt64(-8)));

				this->irb.CreateSetStringData(func->getArguments()[0], fir::ConstantValue::getNullValue(fir::Type::getInt8Ptr()));
				this->irb.CreateUnCondBranch(merge);
			}

			this->irb.setCurrentBlock(merge);
			this->irb.CreateReturnVoid();

			this->irb.setCurrentBlock(restore);

			decrf = func;
		}

		iceAssert(decrf);
		return decrf;
	}




	fir::Function* CodegenInstance::getStringBoundsCheckFunction()
	{
		fir::Function* fn = this->module->getFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = this->irb.getCurrentBlock();

			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_BOUNDS_CHECK_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = this->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

			this->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* len = this->irb.CreateGetStringLength(arg1);
			fir::Value* res = this->irb.CreateICmpGEQ(arg2, len);

			this->irb.CreateCondBranch(res, failb, merge);
			this->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() },
					fir::Type::getInt32()), fir::LinkageType::External);

				fir::Function* fdopenf = this->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = this->module->createGlobalString("w");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = this->module->createGlobalString("Tried to index string at index '%zu'; length is only '%zu'! (max index is thus '%zu')\n");
				fmtstr = this->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				this->irb.CreateCall(fprintfn, { err, fmtstr, arg2, len, this->irb.CreateSub(len, fir::ConstantInt::getInt64(1)) });

				this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
				this->irb.CreateUnreachable();
			}

			this->irb.setCurrentBlock(merge);
			{
				this->irb.CreateReturnVoid();
			}

			fn = func;


			this->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






	fir::Function* CodegenInstance::getStringCheckLiteralWriteFunction()
	{
		fir::Function* fn = this->module->getFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name));

		if(!fn)
		{
			auto restore = this->irb.getCurrentBlock();


			fir::Function* func = this->module->getOrCreateFunction(Identifier(BUILTIN_STRING_CHECK_LITERAL_FUNC_NAME, IdKind::Name),
				fir::FunctionType::get({ fir::Type::getStringType()->getPointerTo(), fir::Type::getInt64() }, fir::Type::getVoid(), false),
				fir::LinkageType::Internal);

			fir::IRBlock* entry = this->irb.addNewBlockInFunction("entry", func);
			fir::IRBlock* failb = this->irb.addNewBlockInFunction("fail", func);
			fir::IRBlock* merge = this->irb.addNewBlockInFunction("merge", func);

			this->irb.setCurrentBlock(entry);

			fir::Value* arg1 = func->getArguments()[0];
			fir::Value* arg2 = func->getArguments()[1];

			fir::Value* rc = this->irb.CreateGetStringRefCount(arg1);
			fir::Value* res = this->irb.CreateICmpLT(rc, fir::ConstantInt::getInt64(0));

			this->irb.CreateCondBranch(res, failb, merge);
			this->irb.setCurrentBlock(failb);
			{
				fir::Function* fprintfn = this->module->getOrCreateFunction(Identifier("fprintf", IdKind::Name),
					fir::FunctionType::getCVariadicFunc({ fir::Type::getVoidPtr(), fir::Type::getInt8Ptr() }, fir::Type::getInt32()),
					fir::LinkageType::External);

				fir::Function* fdopenf = this->module->getOrCreateFunction(Identifier("fdopen", IdKind::Name),
					fir::FunctionType::get({ fir::Type::getInt32(), fir::Type::getInt8Ptr() }, fir::Type::getVoidPtr(), false),
					fir::LinkageType::External);

				// basically:
				// void* stderr = fdopen(2, "w")
				// fprintf(stderr, "", bla bla)

				fir::Value* tmpstr = this->module->createGlobalString("w");
				tmpstr = this->irb.CreateConstGEP2(tmpstr, 0, 0);

				fir::Value* fmtstr = this->module->createGlobalString("Tried to write to immutable string literal '%s' at index '%zu'!\n");
				fmtstr = this->irb.CreateConstGEP2(fmtstr, 0, 0);

				fir::Value* err = this->irb.CreateCall2(fdopenf, fir::ConstantInt::getInt32(2), tmpstr);

				fir::Value* dp = this->irb.CreateGetStringData(arg1);
				this->irb.CreateCall(fprintfn, { err, fmtstr, dp, arg2 });

				this->irb.CreateCall0(this->getOrDeclareLibCFunc("abort"));
				this->irb.CreateUnreachable();
			}

			this->irb.setCurrentBlock(merge);
			{
				this->irb.CreateReturnVoid();
			}

			fn = func;


			this->irb.setCurrentBlock(restore);
		}

		iceAssert(fn);
		return fn;
	}






























	static bool isStructuredAggregate(fir::Type* t)
	{
		return t->isStructType() || t->isClassType() || t->isTupleType();
	}


	template <typename T>
	void doRefCountOfAggregateType(CodegenInstance* cgi, T* type, fir::Value* value, bool incr)
	{
		iceAssert(cgi->isRefCountedType(type));
		iceAssert(value->getType()->isPointerType());

		size_t i = 0;
		for(auto m : type->getElements())
		{
			if(cgi->isRefCountedType(m))
			{
				fir::Value* mem = cgi->irb.CreateStructGEP(value, i);

				if(incr)
					cgi->incrementRefCount(mem);

				else
					cgi->decrementRefCount(mem);
			}
			else if(isStructuredAggregate(m))
			{
				fir::Value* mem = cgi->irb.CreateStructGEP(value, i);

				if(m->isStructType())
					doRefCountOfAggregateType(cgi, m->toStructType(), mem, incr);

				else if(m->isClassType())
					doRefCountOfAggregateType(cgi, m->toClassType(), mem, incr);

				else if(m->isTupleType())
					doRefCountOfAggregateType(cgi, m->toTupleType(), mem, incr);
			}

			i++;
		}
	}

	void CodegenInstance::incrementRefCount(fir::Value* strp)
	{
		iceAssert(strp->getType()->isPointerType());
		if(strp->getType()->getPointerElementType()->isStringType())
		{
			iceAssert(strp->getType()->isPointerType() && strp->getType()->getPointerElementType()->isStringType());

			fir::Function* incrf = this->getStringRefCountIncrementFunction();
			this->irb.CreateCall1(incrf, strp);
		}
		else if(isStructuredAggregate(strp->getType()->getPointerElementType()))
		{
			auto ty = strp->getType()->getPointerElementType();
			if(ty->isStructType())
				doRefCountOfAggregateType(this, ty->toStructType(), strp, true);

			else if(ty->isClassType())
				doRefCountOfAggregateType(this, ty->toClassType(), strp, true);

			else if(ty->isTupleType())
				doRefCountOfAggregateType(this, ty->toTupleType(), strp, true);
		}
		else
		{
			error("no");
		}
	}

	void CodegenInstance::decrementRefCount(fir::Value* strp)
	{
		iceAssert(strp->getType()->isPointerType());
		if(strp->getType()->getPointerElementType()->isStringType())
		{
			iceAssert(strp->getType()->isPointerType() && strp->getType()->getPointerElementType()->isStringType());

			fir::Function* decrf = this->getStringRefCountDecrementFunction();
			this->irb.CreateCall1(decrf, strp);
		}
		else if(isStructuredAggregate(strp->getType()->getPointerElementType()))
		{
			auto ty = strp->getType()->getPointerElementType();
			if(ty->isStructType())
				doRefCountOfAggregateType(this, ty->toStructType(), strp, false);

			else if(ty->isClassType())
				doRefCountOfAggregateType(this, ty->toClassType(), strp, false);

			else if(ty->isTupleType())
				doRefCountOfAggregateType(this, ty->toTupleType(), strp, false);
		}
		else
		{
			error("no");
		}
	}


	void CodegenInstance::assignRefCountedExpression(Expr* user, fir::Value* val, fir::Value* ptr, fir::Value* target,
		ValueKind rhsVK, bool isInit)
	{
		// if you're doing stupid things:
		if(!this->isRefCountedType(val->getType()))
		{
			error(user, "not refcounted");
		}

		// ok...
		// if the rhs is an lvalue, it's simple.
		// increment its refcount, decrement the left side refcount, store, return.
		if(rhsVK == ValueKind::LValue)
		{
			iceAssert(ptr->getType()->getPointerElementType() == val->getType());
			this->incrementRefCount(ptr);

			// decrement left side
			if(!isInit)
				this->decrementRefCount(target);

			// store
			this->irb.CreateStore(this->irb.CreateLoad(ptr), target);
		}
		else
		{
			// the rhs has already been evaluated
			// as an rvalue, its refcount *SHOULD* be one
			// so we don't do anything to it
			// instead, decrement the left side

			if(!isInit)
				this->decrementRefCount(target);

			// to avoid double-freeing, we remove 'val' from the list of refcounted things
			// since it's an rvalue, it can't be "re-referenced", so to speak.

			// the issue of double-free comes up when the variable being assigned to goes out of scope, and is freed
			// since they refer to the same pointer, we get a double free if the temporary expression gets freed as well.

			if(ptr)
				this->removeRefCountedValueIfExists(ptr);

			// now we just store as usual
			this->irb.CreateStore(val, target);
		}
	}










	Result_t CodegenInstance::createLLVariableArray(fir::Value* ptr, fir::Value* length)
	{
		iceAssert(ptr->getType()->isPointerType());
		iceAssert(length->getType()->isIntegerType());

		fir::LLVariableArrayType* arrType = fir::LLVariableArrayType::get(ptr->getType()->getPointerElementType());
		fir::Value* arr = this->getStackAlloc(arrType, "Any_Variadic_Array");

		fir::Value* ptrGEP = this->irb.CreateStructGEP(arr, 0);
		fir::Value* lenGEP = this->irb.CreateStructGEP(arr, 1);

		this->irb.CreateStore(ptr, ptrGEP);
		this->irb.CreateStore(length, lenGEP);

		return Result_t(this->irb.CreateLoad(arr), arr);
	}

	Result_t CodegenInstance::indexLLVariableArray(fir::Value* arr, fir::Value* index)
	{
		iceAssert(arr->getType()->isLLVariableArrayType());
		iceAssert(index->getType()->isIntegerType());

		fir::Value* ptrGEP = this->irb.CreateStructGEP(arr, 0);
		fir::Value* ptr = this->irb.CreateLoad(ptrGEP);

		// todo: bounds checking?

		fir::Value* gep = this->irb.CreateGEP2(ptr, fir::ConstantInt::getUint64(0), index);
		return Result_t(this->irb.CreateLoad(gep), gep);
	}

	Result_t CodegenInstance::getLLVariableArrayDataPtr(fir::Value* arrPtr)
	{
		iceAssert(arrPtr->getType()->isPointerType() && "not a pointer type");
		iceAssert(arrPtr->getType()->getPointerElementType()->isLLVariableArrayType());

		fir::Value* ptrGEP = this->irb.CreateStructGEP(arrPtr, 0);
		return Result_t(this->irb.CreateLoad(ptrGEP), ptrGEP);
	}

	Result_t CodegenInstance::getLLVariableArrayLength(fir::Value* arrPtr)
	{
		iceAssert(arrPtr->getType()->isPointerType() && "not a pointer type");
		iceAssert(arrPtr->getType()->getPointerElementType()->isLLVariableArrayType());

		fir::Value* lenGEP = this->irb.CreateStructGEP(arrPtr, 1);
		return Result_t(this->irb.CreateLoad(lenGEP), lenGEP);
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


		bool isVoid = (retType == 0 ? func->getType(this) : retType)->isVoidType();

		// check the block
		if(func->block->statements.size() == 0 && !isVoid)
		{
			error(func, "Function '%s' has return type '%s', but returns nothing", func->decl->ident.name.c_str(),
				func->decl->ptype->str().c_str());
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

		if(!ret && (isVoid || !checkType || this->getAutoCastDistance(final->getType(this), func->getType(this)) != -1))
			return true;

		if(!ret)
		{
			error(func, "Function '%s' missing return statement (implicit return invalid, need %s, got %s)", func->decl->ident.name.c_str(),
				func->getType(this)->str().c_str(), final->getType(this)->str().c_str());
		}

		if(checkType)
		{
			verifyReturnType(this, func, func->block, ret, retType);
		}

		return false;
	}














}



