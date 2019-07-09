#!/usr/bin/env python3

# taken from github.com/pervognesn/bitwise/ion/generate_test.py

import sys

def gen_test(reps):
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
			var list: LinkedList(?)!<T: int>

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

	fn genericstuff(?)()
	{
		do {
			fn gincr<A>(x: A) -> A => x + 1
			fn apply<B, C>(x: B, f: fn(B) -> C) -> C => f(x)

			fn mapstupid<D, E, F>(arr: [D:], f: fn(D) -> E, fa: fn(D, fn(D) -> E) -> F) -> [F]
			{
				var i = 0
				var ret: [F]
				while i < arr.length
				{
					ret.append(fa(arr[i], f))
					i += 1
				}

				return ret
			}

			printf("set 4:")
			let new = mapstupid([ 5, 6, 7, 8, 9 ], gincr, apply)
			for it in new { printf(" %d", it) }

			printf("\\n")
		}


		do {

			fn map2<T, K, R>(arr: [(T, K):], f: fn(T, K) -> R) -> [R]
			{
				var i = 0
				var ret: [R]
				while i < arr.length
				{
					ret.append(f(arr[i].0, arr[i].1))
					i += 1
				}

				return ret
			}


			fn add2<A, B>(x: A, y: B) -> A => x + y


			printf("set 5:")
			let new = map2([ (2, 2), (4, 4), (6, 6), (8, 8), (10, 10) ], add2)
			for it in new { printf(" %d", it) }

			printf("\\nset (?) done\\n\\n")
		}
	}
	"""

	outfile = open("build/massive.flx", "wt")

	outfile.write("export massive\n")
	outfile.write("import libc as _\n")
	outfile.write("ffi fn srand(s: i32)\n")
	outfile.write("ffi fn rand() -> i32\n")

	# dupes = range(128 * 48)
	dupes = range(reps)

	for i in dupes:
		outfile.write(template.replace("(?)", str(i)) + "\n")


	outfile.write("@entry fn main() -> i32 {\n")
	for i in dupes:
		outfile.write("\tstuff(?)()\n".replace("(?)", str(i)))
		outfile.write("\tgenericstuff(?)()\n".replace("(?)", str(i)))

	outfile.write("\treturn 0\n}\n")

	outfile.close()




if __name__ == '__main__':
    globals()[sys.argv[1]](int(sys.argv[2]))













