// classes.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "ast.h"
#include "pts.h"
#include "errors.h"

#include "ir/type.h"
#include "resolver.h"

#include "typecheck.h"

#include "memorypool.h"

// defined in typecheck/structs.cpp
void checkFieldRecursion(sst::TypecheckState* fs, fir::Type* strty, fir::Type* field, const Location& floc);




TCResult ast::ClassDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());


	auto [ success, ret ] = this->checkForExistingDeclaration(fs, gmaps);
	if(!success)    return TCResult::getParametric();
	else if(ret)    return TCResult(ret);

	auto defnname = util::typeParamMapToString(this->name, gmaps);

	auto defn = util::pool<sst::ClassDefn>(this->loc);
	defn->bareName = this->name;
	defn->attrs = this->attrs;

	defn->id = Identifier(defnname, IdKind::Type);
	defn->id.scope = this->realScope;
	defn->visibility = this->visibility;
	defn->original = this;
	defn->enclosingScope = this->enclosingScope;
	defn->innerScope = this->enclosingScope.appending(defnname);


	// make all our methods be methods
	for(auto m : this->methods)
	{
		m->parentType = this;
		m->realScope = this->realScope + defn->id.name;
		m->enclosingScope = defn->innerScope;
	}

	for(auto m : this->initialisers)
	{
		m->parentType = this;
		m->realScope = this->realScope + defn->id.name;
		m->enclosingScope = defn->innerScope;
	}

	for(auto m : this->staticMethods)
	{
		m->realScope = this->realScope + defn->id.name;
		m->enclosingScope = defn->innerScope;
	}


	auto cls = fir::ClassType::createWithoutBody(defn->id);
	defn->type = cls;


	// why do we do this when generating the declaration instead of only when we typecheck?
	// as it currently stands, this means that our base class + any traits must appear before
	// this class definition in the source code, which is kinda dumb.
	for(auto base : zfu::map(this->bases, [fs](auto t) -> auto { return fs->convertParserTypeToFIR(t); }))
	{
		if(base->isClassType())
		{
			if(defn->baseClass)
				error(this, "cannot inherit from more than one class (already inherited from '%s')", defn->baseClass->id.name);

			else if(!defn->traits.empty())
				error(this, "base class must come before any traits in the inheritance list");

			auto basedef = dcast(sst::ClassDefn, fs->typeDefnMap[base]);
			iceAssert(basedef);

			defn->baseClass = basedef;
			cls->setBaseClass(base->toClassType());
		}
		else if(base->isTraitType())
		{
			auto tdef = dcast(sst::TraitDefn, fs->typeDefnMap[base]);
			iceAssert(tdef);

			defn->traits.push_back(tdef);
			cls->addTraitImpl(tdef->type->toTraitType());
		}
		else
		{
			error(this, "invalid type '%s' in inheritance list of class", base);
		}
	}

	if(auto err = fs->checkForShadowingOrConflictingDefinition(defn, [](auto, auto) -> bool { return true; }))
		return TCResult(err);

	// add it first so we can use it in the method bodies,
	// and make pointers to it
	{
		// fs->getTreeOfScope(this->realScope)->addDefinition(defnname, defn, gmaps);
		defn->enclosingScope.stree->addDefinition(defnname, defn, gmaps);
		fs->typeDefnMap[cls] = defn;
	}

	// auto oldscope = fs->getCurrentScope();
	// fs->teleportToScope(defn->id.scope);
	// fs->pushTree(defn->id.name);
	fs->teleportInto(defn->innerScope);
	{
		for(auto t : this->nestedTypes)
		{
			t->realScope = this->realScope + defn->id.name;
			t->enclosingScope = defn->innerScope;
			t->generateDeclaration(fs, 0, { });
		}
	}
	fs->teleportOut();

	// fs->popTree();
	// fs->teleportToScope(oldscope);


	this->genericVersions.push_back({ defn, fs->getGenericContextStack() });
	return TCResult(defn);
}

TCResult ast::ClassDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	fs->pushLoc(this);
	defer(fs->popLoc());

	auto tcr = this->generateDeclaration(fs, infer, gmaps);
	if(tcr.isParametric())  return tcr;
	else if(!tcr.isDefn())  error(this, "failed to generate declaration for function '%s'", this->name);

	auto defn = dcast(sst::ClassDefn, tcr.defn());
	iceAssert(defn);

	if(this->finishedTypechecking.find(defn) != this->finishedTypechecking.end())
		return TCResult(defn);

	auto cls = defn->type->toClassType();
	iceAssert(cls);

	// auto oldscope = fs->getCurrentScope();
	// fs->teleportToScope(defn->id.scope);
	// fs->pushTree(defn->id.name);

	fs->teleportInto(defn->innerScope);

	if(this->initialisers.empty())
		error(this, "class must have at least one initialiser");




	for(auto t : this->nestedTypes)
	{
		auto tcr = t->typecheck(fs);
		if(tcr.isParametric())  continue;
		if(tcr.isError())       error(t, "failed to generate declaration for nested type '%s' in struct '%s'", t->name, this->name);

		auto st = dcast(sst::TypeDefn, tcr.defn());
		iceAssert(st);

		defn->nestedTypes.push_back(st);
	}



	fs->pushSelfContext(cls);
	{
		std::vector<std::pair<std::string, fir::Type*>> tys;
		for(auto f : this->fields)
		{
			auto v = dcast(sst::StructFieldDefn, f->typecheck(fs).defn());
			iceAssert(v);

			defn->fields.push_back(v);
			tys.push_back({ v->id.name, v->type });

			checkFieldRecursion(fs, cls, v->type, v->loc);

			std::function<void (sst::ClassDefn*, sst::StructFieldDefn*)> checkDupe = [](sst::ClassDefn* cls, sst::StructFieldDefn* fld) -> auto {
				while(cls)
				{
					for(auto bf : cls->fields)
					{
						if(bf->id.name == fld->id.name)
						{
							SimpleError::make(fld->loc, "redefinition of field '%s' (with type '%s'), that exists in the base class '%s'",
								fld->id.name, fld->type, cls->id)
								->append(SimpleError::make(MsgType::Note, bf->loc, "'%s' was previously defined in the base class here:", fld->id.name))
								->append(SimpleError::make(MsgType::Note, cls->loc, "base class '%s' was defined here:", cls->id))
								->postAndQuit();
						}
					}

					cls = cls->baseClass;
				}
			};

			checkDupe(defn->baseClass, v);
		}
		cls->setMembers(tys);


		{
			//* check for what would be called 'method hiding' in c++, and also valid overrides.
			// TODO: make an error note about co/contra-variance for param/return types. right now it just complains and it's vague af.
			auto checkAgainstBaseClasses = [](sst::ClassDefn* cls, sst::FunctionDefn* meth) -> auto {

				auto checkSingleMethod = [](sst::ClassDefn* cls, sst::FunctionDefn* self, sst::FunctionDefn* bf, bool* matchedName) -> bool {

					if(bf->id.name == self->id.name)
					{
						*matchedName |= true;

						if(!fir::areMethodsVirtuallyCompatible(bf->type->toFunctionType(), self->type->toFunctionType(),
							/* trait checking: */ false))
						{
							return false;
						}

						// check for virtual functions.
						//* note: we don't need to care if 'bf' is the base method, because if we are 'isOverride', then we are also
						//* 'isVirtual'.

						// nice comprehensive error messages, I hope.
						if(!self->isOverride)
						{
							auto err = SimpleError::make(self->loc, "redefinition of method '%s' (with type '%s'), that exists in"
								" the base class '%s'", self->id.name, self->type, cls->id);

							if(bf->isVirtual)
							{
								err->append(SimpleError::make(MsgType::Note, bf->loc, "'%s' was defined as a virtual method; to override it, use the 'override' keyword", bf->id.name));
							}
							else
							{
								err->append(
									SimpleError::make(MsgType::Note, bf->loc, "'%s' was previously defined in the base class '%s'"
										" as a non-virtual method here:", bf->id.name, cls->id.name
										)->append(BareError::make(MsgType::Note, "to override it, define '%s' as a virtual method",
											bf->id.name)
									)
								);
							}

							err->postAndQuit();
						}
						else if(!bf->isVirtual)
						{
							SimpleError::make(self->loc, "cannot override non-virtual method '%s'", bf->id.name)
								->append(SimpleError::make(MsgType::Note, bf->loc,
									"'%s' was previously defined in the base class '%s' as a non-virtual method here:", bf->id.name, cls->id.name)
								)->append(BareError::make(MsgType::Note, "to override it, define '%s' as a virtual method", bf->id.name))
								->postAndQuit();
						}

						return true;
					}

					return false;
				};

				bool matchedSig = false;
				bool matchedName = false;
				while(cls)
				{
					for(auto bf : cls->methods)
						matchedSig |= checkSingleMethod(cls, meth, bf, &matchedName);

					cls = cls->baseClass;
				}

				if(meth->isOverride && !matchedSig)
				{
					if(matchedName && !matchedSig)
					{
						error(meth, "invalid override: no method named '%s' in any base class with a signature matching"
							" (or compatible with) '%s'", meth->id.name, meth->type->str());
					}
					else if(!matchedName)
					{
						error(meth, "invalid override: no method in any base class named '%s'", meth->id.name);
					}
				}
			};

			for(auto m : this->methods)
			{
				if(m->name == "init")
					error(m, "cannot have methods named 'init' in a class; to create an initialiser, omit the 'fn' keyword.");

				auto res = m->generateDeclaration(fs, cls, { });
				if(res.isParametric())
					continue;

				auto decl = dcast(sst::FunctionDefn, res.defn());
				iceAssert(decl);

				defn->methods.push_back(decl);

				checkAgainstBaseClasses(defn->baseClass, decl);
			}
		}


		{
			// make the constructors
			for(auto it : this->initialisers)
			{
				auto decl = dcast(sst::FunctionDefn, it->generateDeclaration(fs, cls, { }).defn());
				iceAssert(decl);

				defn->methods.push_back(decl);
				defn->initialisers.push_back(decl);
			}

			// and the destructor
			if(this->deinitialiser)
			{
				auto decl = dcast(sst::FunctionDefn, this->deinitialiser->generateDeclaration(fs, cls, { }).defn());
				iceAssert(decl);

				defn->methods.push_back(decl);
				defn->deinitialiser = decl;
			}
			if(this->copyInitialiser)
			{
				auto decl = dcast(sst::FunctionDefn, this->copyInitialiser->generateDeclaration(fs, cls, { }).defn());
				iceAssert(decl);

				defn->methods.push_back(decl);
				defn->copyInitialiser = decl;
			}
			if(this->moveInitialiser)
			{
				auto decl = dcast(sst::FunctionDefn, this->moveInitialiser->generateDeclaration(fs, cls, { }).defn());
				iceAssert(decl);

				defn->methods.push_back(decl);
				defn->moveInitialiser = decl;
			}
		}



		// copy all the things from the superclass into ourselves.
		if(defn->baseClass)
		{
			// basically, the only things we want to import from the base class are fields and methods -- not initialisers.
			// base-class-constructors must be called using `super(...)` syntax.

			auto scp = defn->baseClass->id.scope + defn->baseClass->id.name;
			auto tree = fs->getTreeOfScope(scp);

			std::function<void (sst::StateTree*, sst::StateTree*)> recursivelyImport = [&](sst::StateTree* from, sst::StateTree* to) -> void {

				for(auto [ file, defs ] : from->getAllDefinitions())
				{
					for(auto def : defs)
					{
						if(!dcast(sst::ClassInitialiserDefn, def))
							to->addDefinition(file, def->id.name, def);
					}
				}

				for(auto sub : from->subtrees)
				{
					if(to->subtrees.find(sub.first) == to->subtrees.end())
						to->subtrees[sub.first] = util::pool<sst::StateTree>(sub.first, sub.second->topLevelFilename, to);

					recursivelyImport(sub.second, to->subtrees[sub.first]);
				}
			};

			recursivelyImport(tree, fs->stree);
		}

		for(auto f : this->staticFields)
		{
			auto v = dcast(sst::VarDefn, f->typecheck(fs).defn());
			iceAssert(v);

			defn->staticFields.push_back(v);
		}

		for(auto m : this->staticMethods)
		{
			// infer is 0 because this is a static thing
			auto res = m->generateDeclaration(fs, 0, { });
			if(res.isParametric())
				continue;

			auto decl = dcast(sst::FunctionDefn, res.defn());
			iceAssert(decl);

			defn->staticMethods.push_back(decl);
		}




		// once we get all the proper declarations and such, create the function bodies.
		if(!cls->containsPlaceholders())
		{
			for(auto m : this->methods)
				m->typecheck(fs, cls, { });

			for(auto m : this->initialisers)
				m->typecheck(fs, cls, { });

			for(auto m : this->staticMethods)
				m->typecheck(fs, 0, { });

			if(this->deinitialiser)     this->deinitialiser->typecheck(fs, cls, { });
			if(this->copyInitialiser)   this->copyInitialiser->typecheck(fs, cls, { });
			if(this->moveInitialiser)   this->moveInitialiser->typecheck(fs, cls, { });
		}
	}
	fs->popSelfContext();


	fs->teleportOut();

	// fs->popTree();
	// fs->teleportToScope(oldscope);

	this->finishedTypechecking.insert(defn);
	return TCResult(defn);
}












TCResult ast::InitFunctionDefn::typecheck(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	iceAssert(infer && infer->isClassType());
	auto cls = infer->toClassType();

	auto ret = dcast(sst::FunctionDefn, this->actualDefn->typecheck(fs, cls, gmaps).defn());

	// if the initialiser was polymorphic, then don't generate bodies!
	if(ret->type->containsPlaceholders())
		return TCResult::getParametric();

	// only check this stuff for the real constructor.
	if(this->name == "init")
	{
		if(cls->getBaseClass() && !this->didCallSuper)
		{
			error(this, "initialiser for class '%s' must explicitly call an initialiser of the base class '%s'", cls->getTypeName().name,
				cls->getBaseClass()->getTypeName().name);
		}
		else if(!cls->getBaseClass() && this->didCallSuper)
		{
			error(this, "cannot call base class initialiser for class '%s' when it does not inherit from a base class",
				cls->getTypeName().name);
		}
		else if(cls->getBaseClass() && this->didCallSuper)
		{
			auto base = cls->getBaseClass();
			auto call = util::pool<sst::BaseClassConstructorCall>(this->loc, base);

			call->classty = dcast(sst::ClassDefn, fs->typeDefnMap[base]);
			iceAssert(call->classty);

			std::vector<FnCallArgument> baseargs;
			{
				auto restore = fs->stree;
				fs->stree = ret->insideTree;

				baseargs = sst::resolver::misc::typecheckCallArguments(fs, this->superArgs);

				fs->stree = restore;
			}

			auto constr = sst::resolver::resolveConstructorCall(fs, this->loc, call->classty, baseargs, PolyArgMapping_t::none());

			call->arguments = baseargs;
			call->target = dcast(sst::FunctionDefn, constr.defn());
			iceAssert(call->target);

			// insert it as the first thing.
			ret->body->statements.insert(ret->body->statements.begin(), call);
		}
	}

	return TCResult(ret);
}


TCResult ast::InitFunctionDefn::generateDeclaration(sst::TypecheckState* fs, fir::Type* infer, const TypeParamMap_t& gmaps)
{
	//* so here's the thing
	//* basically this init function thingy is just a normal function definition
	//* but due to the way the AST was built, and because it's actually slightly less messy IMO,
	//* we return a separate AST type that does not inherit from FuncDefn.

	//* so, to reduce code dupe and make it less stupid, we actually make a fake FuncDefn from ourselves,
	//* and typecheck that, returning that as the result.

	//* we don't want to be carrying too many distinct types around in SST nodes.

	iceAssert(infer);

	this->actualDefn = util::pool<ast::FuncDefn>(this->loc);

	this->actualDefn->name = this->name;
	this->actualDefn->body = this->body;
	this->actualDefn->params = this->params;
	this->actualDefn->parentType = this->parentType;
	this->actualDefn->returnType = pts::NamedType::create(this->loc, VOID_TYPE_STRING);

	this->actualDefn->realScope = this->realScope;
	this->actualDefn->enclosingScope = this->enclosingScope;

	//* note: constructors will always mutate, definitely.
	this->actualDefn->isMutating = true;

	auto ret = this->actualDefn->generateDeclaration(fs, infer, gmaps);
	if(ret.isDefn())
	{
		auto def = dcast(sst::FunctionDefn, ret.defn());
		iceAssert(def);

		// do some checks.
		if(this->name == "copy")
		{
			if(def->params.size() != 2)
			{
				error(def, "copy initialiser must take exactly one argument, %d were found", def->params.size() - 1);
			}
			else if(auto ty = def->params[1].type; !ty->isImmutablePointer() || ty->getPointerElementType() != def->parentTypeForMethod)
			{
				error(def->params[1].loc, "parameter of copy initialiser must have type '&self' (aka '%s'), found '%s' instead",
					def->parentTypeForMethod->getPointerTo(), ty);
			}
		}
		else if(this->name == "move")
		{
			if(def->params.size() != 2)
			{
				error(def, "move initialiser must take exactly one argument, %d were found", def->params.size() - 1);
			}
			else if(auto ty = def->params[1].type; !ty->isMutablePointer() || ty->getPointerElementType() != def->parentTypeForMethod)
			{
				error(def->params[1].loc, "parameter of move initialiser must have type '&mut self' (aka '%s'), found '%s' instead",
					def->parentTypeForMethod->getMutablePointerTo(), ty);
			}
		}
	}

	return ret;
}






























