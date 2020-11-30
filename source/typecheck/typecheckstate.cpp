// typecheckstate.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "defs.h"
#include "errors.h"
#include "typecheck.h"

#include "ir/type.h"
#include "memorypool.h"
#include "zfu.h"

#include <algorithm>
#include <deque>
#include <iterator>

namespace sst
{

	void TypecheckState::pushLoc(ast::Stmt* stmt)
	{
		this->locationStack.push_back(stmt->loc);
	}

	void TypecheckState::pushLoc(const Location& l)
	{
		this->locationStack.push_back(l);
	}



	Location TypecheckState::popLoc()
	{
		iceAssert(this->locationStack.size() > 0);
		auto last = this->locationStack.back();
		this->locationStack.pop_back();

		return last;
	}

	Location TypecheckState::loc()
	{
		iceAssert(this->locationStack.size() > 0);
		return this->locationStack.back();
	}

	#define BODY_FUNC       1
	#define BODY_STRUCT     2
	void TypecheckState::enterFunctionBody(FunctionDefn* fn)
	{
		this->currentFunctionStack.push_back(fn);
		this->bodyStack.push_back(BODY_FUNC);
	}

	void TypecheckState::leaveFunctionBody()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "not inside function");

		this->currentFunctionStack.pop_back();

		iceAssert(this->bodyStack.back() == BODY_FUNC);
		this->bodyStack.pop_back();
	}

	FunctionDefn* TypecheckState::getCurrentFunction()
	{
		if(this->currentFunctionStack.empty())
			error(this->loc(), "not inside function");

		return this->currentFunctionStack.back();
	}

	bool TypecheckState::isInFunctionBody()
	{
		return this->currentFunctionStack.size() > 0 && this->bodyStack.back() == BODY_FUNC;
	}



	fir::Type* TypecheckState::getCurrentSelfContext()
	{
		if(this->selfContextStack.empty())
			error(this->loc(), "not inside struct body");

		return this->selfContextStack.back();
	}

	bool TypecheckState::hasSelfContext()
	{
		return this->selfContextStack.size() > 0;
	}

	void TypecheckState::pushSelfContext(fir::Type* str)
	{
		this->selfContextStack.push_back(str);
		this->bodyStack.push_back(BODY_STRUCT);
	}

	void TypecheckState::popSelfContext()
	{
		if(this->selfContextStack.empty())
			error(this->loc(), "not inside struct body");

		this->selfContextStack.pop_back();

		iceAssert(this->bodyStack.back() == BODY_STRUCT);
		this->bodyStack.pop_back();
	}





	void TypecheckState::enterSubscript(Expr* arr)
	{
		this->subscriptArrayStack.push_back(arr);
	}

	Expr* TypecheckState::getCurrentSubscriptArray()
	{
		iceAssert(this->subscriptArrayStack.size() > 0);
		return this->subscriptArrayStack.back();
	}

	void TypecheckState::leaveSubscript()
	{
		iceAssert(this->subscriptArrayStack.size() > 0);
		this->subscriptArrayStack.pop_back();
	}

	bool TypecheckState::isInSubscript()
	{
		return this->subscriptArrayStack.size() > 0;
	}








	void TypecheckState::pushTree(const std::string& name, bool createAnonymously)
	{
		iceAssert(this->stree);
		this->stree = this->stree->findOrCreateSubtree(name, createAnonymously);
	}

	StateTree* TypecheckState::popTree()
	{
		iceAssert(this->stree);
		auto ret = this->stree;
		this->stree = this->stree->parent;

		return ret;
	}

	void TypecheckState::enterBreakableBody()
	{
		this->breakableBodyNest++;
	}

	void TypecheckState::leaveBreakableBody()
	{
		iceAssert(this->breakableBodyNest > 0);
		this->breakableBodyNest--;
	}

	bool TypecheckState::isInBreakableBody()
	{
		return this->breakableBodyNest > 0;
	}

	void TypecheckState::enterDeferBlock()
	{
		this->deferBlockNest++;
	}

	void TypecheckState::leaveDeferBlock()
	{
		iceAssert(this->deferBlockNest > 0);
		this->deferBlockNest--;
	}

	bool TypecheckState::isInDeferBlock()
	{
		return this->deferBlockNest > 0;
	}

	Scope TypecheckState::getCurrentScope2()
	{
		return this->stree->getScope();
	}

	std::string TypecheckState::serialiseCurrentScope()
	{
		std::deque<std::string> scope;
		StateTree* tree = this->stree;

		while(tree)
		{
			scope.push_front(tree->name);
			tree = tree->parent;
		}

		return zfu::join(std::vector<std::string>(scope.begin(), scope.end()), "::");
	}

	std::vector<Defn*> StateTree::getAllDefinitions()
	{
		std::vector<Defn*> ret;
		for(const auto& [ n, ds ] : this->definitions2)
			for(auto d : ds)
				ret.push_back(d);

		return ret;
	}

	static void fetchDefinitionsFrom(const std::string& name, StateTree* tree, bool recursively, bool includePrivate, std::vector<Defn*>& out)
	{
		if(auto it = tree->definitions2.find(name); it != tree->definitions2.end())
		{
			std::copy_if(it->second.begin(), it->second.end(), std::back_inserter(out), [includePrivate](Defn* defn) -> bool {
				return (includePrivate ? true : defn->visibility == VisibilityLevel::Public);
			});
		}

		auto sameOrigin = [](const StateTree* a, const StateTree* b) -> bool {
			auto p1 = a; while(p1->parent) p1 = p1->parent;
			auto p2 = b; while(p2->parent) p2 = p2->parent;

			return p1 == p2;
		};

		for(auto import : tree->imports)
		{
			if(recursively)
			{
				// only include private things if we're in the same file.
				bool priv = sameOrigin(tree, import);
				fetchDefinitionsFrom(name, import, /* recursively: */ false, /* includePrivate: */ priv, out);
			}

			// in theory we should never include the private definitions from re-exports
			for(auto reexp : import->reexports)
				fetchDefinitionsFrom(name, reexp, /* recursively: */ false, /* includePrivate: */ false, out);
		}
	}

	std::vector<Defn*> StateTree::getDefinitionsWithName(const std::string& name)
	{
		std::vector<Defn*> ret;
		fetchDefinitionsFrom(name, this, /* recursively: */ true, /* includePrivate: */ true, ret);

		return ret;
	}

	std::vector<ast::Parameterisable*> StateTree::getUnresolvedGenericDefnsWithName(const std::string& name)
	{
		if(auto it = this->unresolvedGenericDefs.find(name); it != this->unresolvedGenericDefs.end())
			return it->second;

		else
			return { };
	}

	void StateTree::addDefinition(const std::string& name, Defn* def, const TypeParamMap_t& gmaps)
	{
		this->definitions2[name].push_back(def);
	}


	std::string Scope::string() const
	{
		return zfu::join(this->components(), "::");
	}

	const std::vector<std::string>& Scope::components() const
	{
		if(!this->cachedComponents.empty())
			return this->cachedComponents;

		auto& ret = this->cachedComponents;

		const Scope* s = this;
		while(s && s->stree)
		{
			ret.push_back(s->stree->name);
			s = s->prev;
		}

		std::reverse(ret.begin(), ret.end());
		return ret;
	}

	const Scope& StateTree::getScope()
	{
		if(!this->cachedScope.stree)
		{
			this->cachedScope.stree = this;

			if(this->parent)
				this->cachedScope.prev = &this->parent->getScope();
		}

		return this->cachedScope;
	}

	const Scope& Scope::appending(const std::string& name) const
	{
		return this->stree->findOrCreateSubtree(name)->getScope();
	}

	void TypecheckState::teleportInto(const Scope& scope)
	{
		this->teleportationStack.push_back(this->stree);
		this->stree = scope.stree;
	}

	void TypecheckState::teleportOut()
	{
		this->stree = this->teleportationStack.back();
		this->teleportationStack.pop_back();
	}

	StateTree* StateTree::findSubtree(const std::string& name)
	{
		if(auto it = this->subtrees.find(name); it != this->subtrees.end())
			return it->second;

		// check our imports, and our imports' reexports.
		for(const auto imp : this->imports)
		{
			if(imp->name == name)
				return imp;

			for(const auto exp : imp->reexports)
				if(exp->name == name)
					return exp;
		}

		return nullptr;
	}

	StateTree* StateTree::findOrCreateSubtree(const std::string& name, bool anonymous)
	{
		if(auto it = this->subtrees.find(name); it != this->subtrees.end())
		{
			return it->second;
		}
		else
		{
			auto newtree = util::pool<StateTree>(name, this, anonymous);
			this->subtrees[name] = newtree;
			return newtree;
		}
	}







	std::vector<Defn*> TypecheckState::getDefinitionsWithName(const std::string& name, StateTree* tree)
	{
		if(tree == 0)
			tree = this->stree;

		std::vector<Defn*> ret;

		iceAssert(tree);
		while(tree)
		{
			auto fns = tree->getDefinitionsWithName(name);

			if(fns.size() > 0)
				return fns;

			tree = tree->parent;
		}

		return ret;
	}

	ErrorMsg* TypecheckState::checkForShadowingOrConflictingDefinition(Defn* defn,
		std::function<bool (TypecheckState* fs, Defn* other)> conflictCheckCallback, StateTree* tree)
	{
		if(tree == 0)
			tree = this->stree;

		// first, check for shadowing
		bool didWarnAboutShadow = false;

		auto _tree = tree->parent;
		while(_tree)
		{
			if(auto defs = _tree->getDefinitionsWithName(defn->id.name); defs.size() > 0)
			{
				if(false && !didWarnAboutShadow)
				{
					didWarnAboutShadow = true;
					warn(defn, "definition of %s '%s' shadows one or more previous definitions", defn->getKind(), defn->id.name);

					for(auto d : defs)
						info(d, "previously defined here:");
				}
			}

			_tree = _tree->parent;
		}

		auto makeTheError = [](Locatable* a, const std::string& n, const std::string& ak,
			const std::vector<std::pair<Locatable*, std::string>>& conflicts) -> SimpleError* {

			auto err = SimpleError::make(a->loc, "duplicate definition of '%s'", n);

			bool first = true;

			for(const auto& [ l, kind ] : conflicts)
			{
				err->append(SimpleError::make(MsgType::Note, l->loc, "%shere%s:", first ? strprintf("conflicting %s ",
					zfu::plural("definition", conflicts.size())) : "and ", ak == kind ? "" : strprintf(" (as a %s)", kind)));

				first = false;
			}

			return err;
		};


		// ok, now check only the current scope
		auto defs = tree->getDefinitionsWithName(defn->id.name);

		for(auto otherdef : defs)
		{
			if(!otherdef->type->containsPlaceholders() && conflictCheckCallback(this, otherdef))
			{
				auto errs = makeTheError(defn, defn->id.name, defn->getKind(), { std::make_pair(otherdef, otherdef->getKind()) });

				// TODO: be more intelligent about when we give this informative tidbit
				if(dcast(sst::FunctionDecl, otherdef) && dcast(sst::FunctionDecl, defn))
				{
					auto a = dcast(sst::FunctionDecl, defn);
					auto b = dcast(sst::FunctionDecl, otherdef);
					if(fir::Type::areTypeListsEqual(zfu::map(a->params, [](const auto& p) -> fir::Type* { return p.type; }),
						zfu::map(b->params, [](const auto& p) -> fir::Type* { return p.type; })))
					{
						errs->append(BareError::make(MsgType::Note, "functions cannot be overloaded over argument names or"
							" return types alone"));
					}
				}

				return errs;
			}
		}

		// while in the interests of flexibility we provide a predicate for users to specify whether or not the duplicate definition is
		// actually conflicting, for generics i couldn't be damned.
		//? to know for certain that a definition will conflict with a generic thing, either we are:
		// A: variable & generic anything
		// B: function & generic type
		// C: type & generic anything

		if(auto gdefs = tree->getUnresolvedGenericDefnsWithName(defn->id.name); gdefs.size() > 0)
		{
			if(auto fn = dcast(sst::FunctionDecl, defn))
			{
				// honestly we can't know if we will conflict with other functions.
				// filter out by kind.

				auto newgds = zfu::filterMap(gdefs,
					[](ast::Parameterisable* d) -> bool {
						return dcast(ast::FuncDefn, d) == nullptr;
					},
					[](ast::Parameterisable* d) -> std::pair<Locatable*, std::string> {
						return std::make_pair(d, d->getKind());
					}
				);

				if(newgds.size() > 0)
					return makeTheError(fn, fn->id.name, fn->getKind(), newgds);
			}
			else
			{
				// assume everything conflicts, since functions are the only thing that can overload.
				return makeTheError(defn, defn->id.name, defn->getKind(),
					zfu::map(gdefs, [](ast::Parameterisable* d) -> std::pair<Locatable*, std::string> {
						return std::make_pair(d, d->getKind());
					})
				);
			}
		}

		// no error.
		return nullptr;
	}

	void TypecheckState::pushAnonymousTree()
	{
		static size_t _anonId = 0;
		this->pushTree(std::to_string(_anonId++), /* createAnonymously: */ true);
	}


	Scope::Scope(StateTree* st)
	{
		this->prev = 0;
		this->stree = st;
	}
}

























