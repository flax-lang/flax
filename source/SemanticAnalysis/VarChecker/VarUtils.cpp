// VarUtils.cpp
// Copyright (c) 2014 - The Foreseeable Future, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "../../include/semantic.h"
#include "../../include/codegen.h"

namespace SemAnalysis
{
	using namespace Codegen;
	using namespace Ast;

	struct GlobalState
	{
		std::deque<std::deque<VarDef>> vars;
	};

	static GlobalState gs;

	static VarDef& findVarDef(CodegenInstance* cgi, Expr* user, std::string name)
	{
		if(gs.vars.size() > 0)
		{
			for(size_t i = gs.vars.size(); i-- > 0; )
			{
				printf("LOOK: %zu\n", i);
				for(VarDef& vd : gs.vars[i])
				{
					if(vd.name == name)
					{
						printf("FOUND %s\n", name.c_str());
						return vd;
					}
				}
			}
		}

		// not found
		error(cgi, user, "Could not check var ref '%s'", name.c_str());
	}

	static void complainAboutVarState(CodegenInstance* cgi, Expr* user, VarDef vd)
	{
		switch(vd.state)
		{
			case VarState::Invalid:
				error(cgi, user, "Invalid var reference to '%s'", vd.name.c_str());

			case VarState::NoValue:
				warn(cgi, user, "Variable '%s' does not have a value when it is used here", vd.name.c_str());
				break;

			case VarState::ValidAlloc:
			case VarState::ValidStack:
			case VarState::ModifiedAlloc:
				break;

			case VarState::Deallocated:
				warn(cgi, user, "Variable '%s' has since been deallocated", vd.name.c_str());
				warn(cgi, vd.expr, "Deallocation was here");
				break;
		}
	}

	static void checkExpr(CodegenInstance* cgi, Expr* ex)
	{
		if(VarRef* vr = dynamic_cast<VarRef*>(ex))
		{
			VarDef& vd = findVarDef(cgi, vr, vr->name);
			complainAboutVarState(cgi, vr, vd);
		}
	}

	static void analyseBlock(CodegenInstance* cgi, std::deque<Expr*> exprs)
	{
		for(Expr* ex : exprs)
		{
			if(NamespaceDecl* ns = dynamic_cast<NamespaceDecl*>(ex))
			{
				cgi->pushNamespaceScope(ns->name);

				// todo: how to handle defers
				gs.vars.push_back({ });
				analyseBlock(cgi, ns->innards->statements);
				gs.vars.pop_back();

				cgi->popNamespaceScope();
			}
			else if(FuncDecl* fn = dynamic_cast<FuncDecl*>(ex))
			{
				error(cgi, fn, "how?");
			}
			else if(Func* fn = dynamic_cast<Func*>(ex))
			{
				gs.vars.push_back({ });
				for(VarDecl* var : fn->decl->params)
				{
					VarDef vdef;
					vdef.name = var->name;
					vdef.state = VarState::ValidStack;

					printf("DEF %s: %zu\n", vdef.name.c_str(), gs.vars.size() - 1);
					gs.vars.back().push_back(vdef);
				}

				analyseBlock(cgi, fn->block->statements);
				gs.vars.pop_back();
			}




			else if(VarDecl* vd = dynamic_cast<VarDecl*>(ex))
			{
				VarDef vdef;
				vdef.name = vd->name;
				vdef.state = VarState::NoValue;

				if(vd->initVal != 0)
				{
					// check if it's "alloc"
					// todo: more robust identification
					if(dynamic_cast<Alloc*>(vd->initVal))
					{
						vdef.state = VarState::ValidAlloc;
						printf("%s is ALLOC (%zu)\n", vdef.name.c_str(), gs.vars.size() - 1);
					}
					else
					{
						vdef.state = VarState::ValidStack;
						printf("%s is STACK (%zu)\n", vdef.name.c_str(), gs.vars.size() - 1);
					}
				}

				gs.vars.back().push_back(vdef);
			}
			else if(VarRef* vr = dynamic_cast<VarRef*>(ex))
			{
				checkExpr(cgi, vr);
			}



			else if(BinOp* bo = dynamic_cast<BinOp*>(ex))
			{
				if(VarRef* vr = dynamic_cast<VarRef*>(bo->right))
				{
					checkExpr(cgi, vr);
				}
				else if(VarRef* vr = dynamic_cast<VarRef*>(bo->left))
				{
					VarDef& vd = findVarDef(cgi, vr, vr->name);

					// check the right side
					// todo: more robust
					if(bo->op == ArithmeticOp::Assign)
					{
						if(dynamic_cast<Alloc*>(bo->right))
						{
							vd.state = VarState::ValidAlloc;
						}
						else
						{
							if(vd.state == VarState::ValidAlloc)
							{
								warn(cgi, vr, "Modifying alloced variable prevents proper deallocation checking");
								vd.state = VarState::ModifiedAlloc;
								vd.expr = bo;
							}
							else
							{
								vd.state = VarState::ValidStack;
							}
						}
					}
					else if(bo->op == ArithmeticOp::PlusEquals || bo->op == ArithmeticOp::MinusEquals ||
							bo->op == ArithmeticOp::MultiplyEquals || bo->op == ArithmeticOp::DivideEquals ||
							bo->op == ArithmeticOp::ModEquals || bo->op == ArithmeticOp::ShiftLeftEquals ||
							bo->op == ArithmeticOp::ShiftRightEquals || bo->op == ArithmeticOp::BitwiseAndEquals ||
							bo->op == ArithmeticOp::BitwiseOrEquals || bo->op == ArithmeticOp::BitwiseXorEquals)
					{
						if(vd.state == VarState::ValidAlloc)
						{
							warn(cgi, vr, "Modifying alloced variable prevents proper deallocation checking");
							vd.state = VarState::ModifiedAlloc;
							vd.expr = bo;
						}
					}
					else
					{
						complainAboutVarState(cgi, vr, vd);
					}
				}
			}
			else if(ArrayIndex* ai = dynamic_cast<ArrayIndex*>(ex))
			{
				checkExpr(cgi, ai->arr);
				checkExpr(cgi, ai->index);
			}
			else if(If* ifstmt = dynamic_cast<If*>(ex))
			{
				for(auto cs : ifstmt->_cases)
				{
					checkExpr(cgi, cs.first);

					gs.vars.push_back({ });
					analyseBlock(cgi, cs.second->statements);
					gs.vars.pop_back();
				}

				if(ifstmt->final)
				{
					gs.vars.push_back({ });
					analyseBlock(cgi, ifstmt->final->statements);
					gs.vars.pop_back();
				}
			}
			else if(WhileLoop* wloop = dynamic_cast<WhileLoop*>(ex))
			{
				checkExpr(cgi, wloop->cond);

				gs.vars.push_back({ });
				analyseBlock(cgi, wloop->body->statements);
				gs.vars.pop_back();
			}



			else if(FuncCall* fc = dynamic_cast<FuncCall*>(ex))
			{
				analyseBlock(cgi, fc->params);
			}
			else if(Dealloc* da = dynamic_cast<Dealloc*>(ex))
			{
				if(VarRef* vr = dynamic_cast<VarRef*>(da->expr))
				{
					VarDef& vd = findVarDef(cgi, vr, vr->name);
					complainAboutVarState(cgi, vr, vd);

					if(vd.state == VarState::ModifiedAlloc)
					{
						warn(cgi, vr, "Variable '%s' has been modified since its allocation", vd.name.c_str());
						warn(cgi, vd.expr, "First modified here");
					}

					vd.expr = da;
					vd.state = VarState::Deallocated;
				}
			}
			else
			{
				// skip
			}
		}
	}

	void analyseVarUsage(CodegenInstance* cgi)
	{
		// do funcs
		// from root down.

		gs = GlobalState();

		Root* root = cgi->rootNode;

		gs.vars.push_back({ });
		analyseBlock(cgi, root->topLevelExpressions);


		gs = GlobalState();
	}
}



























