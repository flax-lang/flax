// GenericTypeResolution.cpp
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "pts.h"
#include "ast.h"
#include "codegen.h"

using namespace Ast;

namespace Codegen
{

	/*
		todo: this needs a rewrite

		limitations:

		1. does not handle arrays, like... at all.
			- indirections only does the pointer stuff, arrays aren't reduced to their base type to be matched
			- eg. U[] isn't treated as an array of U, which would get resolved, but rather as its own type or something
			- this clearly fails.

		2. function types aren't handled, like... at alll
			- calling foo(a: [(T) -> T]) for instance with some f(int) -> int, does not work
			- something something jon blow iterative solver something something

		3. some better diagnostics?
			- something like clang -- candidate 'foo' was rejected because 'bla bla bla'
			- we could just have this function return a string, with its own reason
			- nothing complicated.

		overall this function needs to be a lot smarter about how it solves types
		thankfully it's all in one place.

		also we should move all the generic stuff to its own file


		basic algorithm:

		loop through all function parameters

		build a list of string -> list of types
		each generic type (T, K, etc) would have a list of candidate fir::Types*
		also: possibly need to recursively resolve function types
		eg. if the main function takes an argument of type [(T, [<K>(K, K) -> K]) -> T or something stupid like that

		anyway
		once the list of candidates are built,
		for each generic type, loop through each candidate, make sure they are compatible
		if they are not, die

		if they are, woohooo...?
	*/


	static std::string _makeErrorString(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
	static std::string _makeErrorString(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);

		char* tmp = 0;
		vasprintf(&tmp, fmt, ap);

		std::string ret = tmp;
		free(tmp);

		va_end(ap);
		return ret;
	}

	static bool _checkGenericFunction2(CodegenInstance* cgi, std::map<std::string, fir::Type*>* gtm,
		FuncDecl* candidate, std::deque<fir::Type*> args, std::string* errorString, Expr** failedExpr)
	{
		// get rid of this stupid literal nonsense
		for(size_t i = 0; i < args.size(); i++)
		{
			if(args[i]->isPrimitiveType() && args[i]->toPrimitiveType()->isLiteralType())
				args[i] = args[i]->toPrimitiveType()->getUnliteralType();
		}


		if(candidate->params.size() != args.size())
		{
			// if it's not variadic, and it's either a normal function (no parent class) or is a static method,
			// then there's no reason for the parameters to mismatch.
			if(!candidate->isVariadic && (!candidate->parentClass || candidate->isStatic))
			{
				*errorString = _makeErrorString("Mismatched argument count; expected %zu, have %zu", candidate->params.size(), args.size());
				*failedExpr = candidate;
				return false;
			}
			else if(candidate->parentClass && !candidate->isStatic)
			{
				// make sure it's only one off
				if(args.size() < candidate->params.size() || args.size() - candidate->params.size() > 1)
				{
					*errorString = _makeErrorString("Mismatched argument count; expected %zu, have %zu", candidate->params.size(), args.size());
					*failedExpr = candidate;
					return false;
				}

				// didSelfParam = true;
				iceAssert(args.front()->isPointerType() && args.front()->getPointerElementType()->isClassType() && "what, no.");

				// originalFirstParam = args.front();
				args.pop_front();

				iceAssert(candidate->params.size() == args.size());
			}
		}


		// if the candidate is variadic, the number of parameters must be *at least* the number of fixed parameters
		VarDecl* varParam = 0;
		if(candidate->isVariadic)
		{
			if(args.size() < candidate->params.size() - 1)
			{
				*errorString = _makeErrorString("Mismatched argument count; expected %zu, have %zu", candidate->params.size(), args.size());
				*failedExpr = candidate;
				return false;
			}

			// if this is variadic, remove the last parameter from the candidate (we'll add it back later)
			// so we only check the first n - 1 parameters.

			varParam = candidate->params.back();
			candidate->params.pop_back();
		}


		std::map<std::string, std::deque<std::pair<size_t, fir::Type*>>> candidateGenerics;
		std::map<size_t, std::string> genericPositions;
		std::map<size_t, fir::Type*> nonGenericTypes;

		for(size_t i = 0; i < candidate->params.size(); i++)
		{
			pts::Type* pt = 0; std::deque<pts::TypeTransformer> trfs;
			std::tie(pt, trfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(candidate->params[i]->ptype);

			if(pt->isNamedType())
			{
				if(candidate->genericTypes.find(pt->toNamedType()->name) != candidate->genericTypes.end())
				{
					// a bit cumbersome, but whatever
					candidateGenerics[pt->toNamedType()->name].push_back({ i, args[i] });
					genericPositions[i] = pt->toNamedType()->name;
				}
				else
				{
					fir::Type* resolved = cgi->getTypeFromParserType(candidate, pt);
					iceAssert(resolved);

					nonGenericTypes[i] = pts::applyTransformationsOnType(resolved, trfs);
				}
			}
			else if(pt->isTupleType())
			{
				error("notsup");
			}
			else if(pt->isFunctionType())
			{
				error("notsup");
			}
		}



		// first check non-generic types
		for(auto t : nonGenericTypes)
		{
			iceAssert(t.first < args.size());
			fir::Type* a = t.second;
			fir::Type* b = args[t.first];

			if(a != b && cgi->getAutoCastDistance(b, a) == -1)
			{
				*errorString = _makeErrorString("No conversion from given type '%s' to expected type '%s' in argument %zu",
					b->str().c_str(), a->str().c_str(), t.first);
				*failedExpr = candidate->params[t.first];
				return false;
			}
		}



		// ok, now check the generic types

		// check that the transformations are compatible
		// eg. we don't try to pass a parameter T to the function expecting K -- even if the base types match

		#if 0
		for(size_t i = 0; i < candidate->params.size(); i++)
		{
			// we already checked the non-generic ones
			if(genericPositions.find(i) != genericPositions.end())
			{
				pts::Type* p = 0; std::deque<pts::TypeTransformer> ctrfs;
				std::tie(p, ctrfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(candidate->params[i]->ptype);

				fir::Type* a = 0; std::deque<pts::TypeTransformer> ptrfs;
				std::tie(a, ptrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(args[i]);

				if(!pts::areTransformationsCompatible(ctrfs, ptrfs))
				{
					*errorString = _makeErrorString("Incompatible types in solution for parametric type '%s' in argument %zu:"
						" expected '%s', have '%s' (No valid transformations)", genericPositions[i].c_str(), i,
						candidate->params[i]->ptype->str().c_str(), args[i]->str().c_str());

					*failedExpr = candidate->params[i];
					return false;
				}

				// ok.
			}
		}
		#endif


		// check the actual types, now that we know the transformations match
		std::map<std::string, fir::Type*> thistm;

		for(auto gt : candidateGenerics)
		{
			// first make sure all the types match up
			fir::Type* res = 0;
			fir::Type* baseres = 0;
			for(auto t : gt.second)
			{
				fir::Type* base = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(t.second).first;

				if(baseres == 0)
				{
					baseres = base;
					res = t.second;
					continue;
				}
				else if(base != baseres)
				{
					// no casting here, generic types need to be exact.

					*errorString = _makeErrorString("Conflicting solutions for parametric type '%s' in argument %zu: '%s' and '%s'",
						gt.first.c_str(), t.first, res->str().c_str(), t.second->str().c_str());
					*failedExpr = candidate->params[t.first];
					return false;
				}
			}


			// loop again
			fir::Type* sol = 0;
			for(auto t : gt.second)
			{
				size_t i = t.first;

				// check that the transformations are compatible
				// eg. we don't try to pass a parameter T to the function expecting K -- even if the base types match

				fir::Type* arg = args[i]; fir::Type* _ = 0; std::deque<pts::TypeTransformer> argtrfs;
				std::tie(_, argtrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(arg);

				pts::Type* g = 0; std::deque<pts::TypeTransformer> gtrfs;
				std::tie(g, gtrfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(candidate->params[i]->ptype);

				if(!pts::areTransformationsCompatible(gtrfs, argtrfs))
				{
					*errorString = _makeErrorString("Incompatible types in solution for parametric type '%s' in argument %zu:"
						" expected '%s', have '%s' (No valid transformations)", genericPositions[i].c_str(), i,
						candidate->params[i]->ptype->str().c_str(), args[i]->str().c_str());

					*failedExpr = candidate->params[i];
					return false;
				}

				fir::Type* soln = pts::reduceMaximallyWithSubset(arg, gtrfs, argtrfs);
				if(sol == 0)
				{
					sol = soln;
				}
				else if(sol != soln)
				{
					*errorString = _makeErrorString("Incompatible types in solution for parametric type '%s' in argument %zu:"
						" expected '%s', have '%s' (No valid transformations)", genericPositions[i].c_str(), i,
						candidate->params[i]->ptype->str().c_str(), args[i]->str().c_str());

					*failedExpr = candidate->params[i];
					return false;
				}
			}

			iceAssert(sol);
			thistm[gt.first] = sol;
		}


		// check variadic

		if(varParam != 0)
		{
			// add it back first.
			candidate->params.push_back(varParam);

			// get the type.
			pts::Type* pt = 0; std::deque<pts::TypeTransformer> _trfs;
			std::tie(pt, _trfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(candidate->params.back()->ptype->toVariadicArrayType()->base);

			std::string baset;
			if(pt->isNamedType())
			{
				baset = pt->toNamedType()->name;
			}
			else
			{
				error("notsup");
			}


			if(thistm.find(baset) != thistm.end())
			{
				// already have it
				// ensure it matches with the ones we've already found.

				fir::Type* resolved = thistm[baset];
				iceAssert(resolved);

				if(args.size() >= candidate->params.size())
				{
					iceAssert(candidate->params.back()->ptype->isDynamicArrayType());
					pts::Type* vbase = candidate->params.back()->ptype->toDynamicArrayType()->base;

					pts::Type* _ = 0; std::deque<pts::TypeTransformer> trfs;
					std::tie(_, trfs) = pts::decomposeTypeIntoBaseTypeWithTransformations(vbase);
					fir::Type* resolvedvbase = pts::applyTransformationsOnType(resolved, trfs);


					if(args.size() == candidate->params.size() && args.back()->isParameterPackType())
					{
						// check transformations.
						fir::Type* fvbase = args.back()->toParameterPackType()->getElementType();

						fir::Type* _ = 0; std::deque<pts::TypeTransformer> argtrfs;
						std::tie(_, argtrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(fvbase);

						if(resolvedvbase != fvbase && cgi->getAutoCastDistance(fvbase, resolvedvbase) == -1)
						{
							// damn, this is a verbose error message.
							*errorString = _makeErrorString("Incompatible base types for direct parameter pack-forwarding; no conversion from given base type '%s' to expected base type '%s' (in solution for generic type '%s')", _->str().c_str(),
								resolved->str().c_str(), baset.c_str());

							*failedExpr = candidate->params.back();
							return false;
						}

						if(!pts::areTransformationsCompatible(trfs, argtrfs))
						{
							// damn, this is a verbose error message.
							*errorString = _makeErrorString("Incompatible base types for direct parameter pack-forwarding; no transformations to get from given base type '%s' to generic type '%s' with existing solution '%s' (ie. expected base type '%s')",
								fvbase->str().c_str(), baset.c_str(), resolved->str().c_str(), resolvedvbase->str().c_str());

							*failedExpr = candidate->params.back();
							return false;
						}
					}
					else
					{
						for(size_t i = candidate->params.size() - 1; i < args.size(); i++)
						{
							if(cgi->getAutoCastDistance(args[i], resolvedvbase) == -1)
							{
								// damn, this is a verbose error message.
								*errorString = _makeErrorString("Incompatible parameter in variadic argument to function; no conversion from given type '%s' to expected type '%s', in variadic argument %zu", args[i]->str().c_str(),
									resolvedvbase->str().c_str(), i + 2 - candidate->params.size());

								*failedExpr = candidate->params.back();
								return false;
							}
						}
					}
				}
				else
				{
					// no varargs were even given
					// since we have already inferred the type from the other parameters,
					// we can give this a free pass.
				}
			}
			else if(candidate->genericTypes.find(baset) != candidate->genericTypes.end())
			{
				// ok, we need to be able to deduce the type from the vararg only.
				// so if none were provided, then give up.

				if(args.size() < candidate->params.size())
				{
					*errorString = _makeErrorString("No variadic parameters were given, so no solution for generic type '%s' could be determined (it is only used in the variadic parameter pack)", baset.c_str());

					*failedExpr = candidate->params.back();
					return false;
				}


				// great, now just deduce it.
				// we just need to make sure all the Ts match, and the number of indirections
				// match *and* are greater than or equal to the specified level.

				fir::Type* first = args[candidate->params.size() - 1];

				// ok, check the type itself
				for(size_t i = candidate->params.size() - 1; i < args.size(); i++)
				{
					if(args[i] != first && cgi->getAutoCastDistance(first, args[i]))
					{
						*errorString = _makeErrorString("Conflicting types in variadic arguments -- '%s' and '%s' (in solution for generic type '%s')", args[i]->str().c_str(), first->str().c_str(), baset.c_str());

						*failedExpr = candidate->params.back();
						return false;
					}

					// ok check the transformations
					fir::Type* base = 0; std::deque<pts::TypeTransformer> trfs;
					std::tie(base, trfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(args[i]);

					if(!pts::areTransformationsCompatible(_trfs, trfs))
					{
						*errorString = _makeErrorString("Incompatible base types for variadic argument %zu; no transformations to get from given base type '%s' to expected type '%s'  (in solution for generic type '%s')", i, args[i]->str().c_str(),
							candidate->params.back()->ptype->toDynamicArrayType()->base->str().c_str(), baset.c_str());

						*failedExpr = candidate->params.back();
						return false;
					}
				}


				// everything checked out -- resolve.
				fir::Type* _ = 0; std::deque<pts::TypeTransformer> argtrfs;
				std::tie(_, argtrfs) = pts::decomposeFIRTypeIntoBaseTypeWithTransformations(first);
				fir::Type* soln = pts::reduceMaximallyWithSubset(first, _trfs, argtrfs);
				thistm[baset] = soln;
			}
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
				{
					*errorString = _makeErrorString("Solution for generic type '%s' ('%s') does not conform to protocol '%s'",
						cst.first.c_str(), cst.second->str().c_str(), protstr.c_str());

					*failedExpr = candidate;
					return false;
				}
			}
		}



		// check that we actually have an entry for every type
		for(auto t : candidate->genericTypes)
		{
			if(thistm.find(t.first) == thistm.end())
			{
				*errorString = _makeErrorString("No solution for generic type '%s' was found", t.first.c_str());
				*failedExpr = candidate;
				return false;
			}
		}


		*gtm = thistm;
		return true;
	}













	static bool _checkGenericFunction(CodegenInstance* cgi, std::map<std::string, fir::Type*>* gtm,
		FuncDecl* candidate, std::deque<fir::Type*> args)
	{
		std::string err;
		Expr* fe = 0;

		return _checkGenericFunction2(cgi, gtm, candidate, args, &err, &fe);

		#if 0
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
			{
				return false;
			}
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
				std::string s;
				int indirs = 0;

				std::tie(s, indirs) = revTypePositions[i];

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
			int pos = 0;
			int indrs = 0;

			std::tie(pos, indrs) = *pair.second.begin();

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
					if(args.size() == candidate->params.size() && args.back()->isParameterPackType()
						&& args.back()->toParameterPackType()->getElementType() == resolved)
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
		#endif
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
			auto fp = this->instantiateGenericFunctionUsingParameters(user, std::map<std::string, fir::Type*>(), fn, ft->getArgumentTypes());
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

				if(atype->isParameterPackType())
				{
					mangled = "V" + atype->toParameterPackType()->encodedStr();
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
}
