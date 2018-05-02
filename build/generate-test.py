# taken from github.com/pervognesn/bitwise/ion/generate_test.py

template = """
let names(?) = [ "max", "ollie", "coco", "piper", "tim", "bill", "alex", "ron", "isaac", "jim" ]

class Animal(?)
{
	init(w: f64)
	{
		// printf("make animal w = %f\\n", w)
		weight = w
		name = names(?)[rand() % names(?).length]
	}

	virtual fn makeNoise()
	{
		printf("some generic sound\\n")
	}

	var name: str
	var weight: f64
	var cute: bool = true
}

class Dog(?) : Animal(?)
{
	init() : super(w: 300)
	{
		// printf("make dog (w = %f)\\n", weight)
	}

	override fn makeNoise()
	{
		printf("bark bark (%f)\\n", weight)
	}

	virtual fn wag()
	{
		printf("wagging tail of length %d\\n", tail)
	}

	var tail: int = 37
}

class Dalmation(?) : Dog(?)
{
	init() : super()
	{
		printf("make dalmation\\n")
	}

	override fn wag()
	{
		printf("dalmation wags %d\\n", tail)
	}

	var numSpots: int
}


class LinkedList(?)<T>
{
	struct Node
	{
		var data: T

		var prev: &Node
		var next: &Node
	}

	var head: &Node
	var tail: &Node

	var count: int

	init()
	{
	}

	mut fn insert(thing: T)
	{
		var nn = alloc mut Node (data: thing, prev: null, next: null)

		if head == null { head = nn; tail = nn }
		else
		{
			var ot = tail as mut
			tail = nn

			nn.prev = ot
			ot.next = nn
		}

		count += 1
	}

	static fn hello() -> T { printf("hi\\n"); return 10 }
}




fn stuff(?)()
{
	do {
		// don't question.
		srand(284811)

		// fn test() -> f64 { printf("hi\\n"); return 31.51 }

		let dogs = alloc &Dog(?) [5] {
			if i % 2 == 1   { it = alloc Dalmation(?) }
			else            { it = alloc Dog(?) }
		}

		for dog, i in dogs {
			printf("%d: %s - ", i, dog.name)
			dog.wag()
		}
	}


	do {
		var list: LinkedList(?)<T: int>

		list.insert(10)
		list.insert(20)
		list.insert(30)
		list.insert(40)

		var head = list.head
		while(head)
		{
			printf("%d\\n", head.data)
			head = head.next
		}
	}
}
"""
print("export massive")
print("import \"libc\" as _")
print("import \"math\" as _")
print("ffi fn srand(s: i32)")
print("ffi fn rand() -> i32")

dupes = range(16 * 32)

for i in dupes:
	print(template.replace("(?)", str(i)))


print("@entry fn main() -> i32 {")
for i in dupes:
	print("\tstuff(?)()".replace("(?)", str(i)))

print("\treturn 0\n}")











