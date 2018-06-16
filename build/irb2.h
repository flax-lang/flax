// irb2.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <vector>

//? DESIGN DOCUMENT FOR IRBUILDER 2.0
//* not an actual code file.



// main change from the current design is to make fir::Value* have a concept of lvalues and rvalues.
struct Value
{
	enum class Kind
	{
		lvalue,
		rvalue,

	} kind;

	int id = 0;
};


/*
	there would be nothing actually *inside* the values, except for their ID as usual.
	instead, we modify the IRBuilder to return either RValues or LValues from various instructions.

	we'd repurpose the 'store' instruction to be solely for assignment to an lvalue. 'load' would not
	exist any more. to interface with memory (ie. dereference pointer types) we'd make new 'read' and 'write' instructions.

	subscripting an rvalue gives you back an rvalue; member access on an rvalue gives you an rvalue back too.

	most of the normal ops (arithmetic, etc.) would just give rvalues. address-of would take an lvalue and return an rvalue.
	vice-versa for dereference.

	finally, to create lvalues we'd make a new 'makelvalue' instruction. at the translator level, this corresponds to a stack
	allocation. we'd need to keep track of the actual storage pointer, and when we do a lvalue-to-rvalue cast (which should
	be automatic from our IRBuilder's POV -- we must handle this in the translator), we insert a load.
*/

// for example:
Value* load(Value* lv);

static Value* decay(Value* v)
{
	if(v->kind == Value::Kind::lvalue)
		return load(v);

	else
		return v;
}

static void* tollvm(Value* v) { return (void*) v; }
static void add(Value* v, void* llvm) { }
static void* alloc(void* type) { return type; }

void translate()
{
	std::string instr;
	std::vector<Value*> operands;

	if(instr == "add")
	{
		// assert(operands.size() == 2)

		auto l = decay(operands[0]);
		auto r = decay(operands[1]);

		// ... do stuff ...
		(void) tollvm(l);
		(void) tollvm(r);
	}
	else if(instr == "makelvalue")
	{
		auto stack = alloc(operands[0]);
		add(operands[0], stack);
	}
	else if(instr == "addressof")
	{
		// to take the address of an lvalue, we simply make it an rvalue.

		// assert(operands[0].kind == Value::Kind::lvalue)
		auto output = tollvm(operands[0]);
	}
}

































