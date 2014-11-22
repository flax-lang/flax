; ModuleID = 'build/test.bc'
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

%String = type { i64, i8*, void (%String*)*, i64 (%String*)*, void (%String*, i8*)*, i1 (%String*, %String*)* }

@0 = private unnamed_addr constant [14 x i8] c"Hello, World!\00"

declare void @printBool(i1)

declare void @printInt64(i64)

declare i32 @putchar(i32)

declare i8* @malloc(i64)

declare i32 @puts(i8*)

define void @main() {
entry:
  %y = alloca %String
  %x = alloca %String
  call void @"__struct#String_init"(%String* %x)
  call void @"__struct#String_operator#="(%String* %x, i8* getelementptr inbounds ([14 x i8]* @0, i32 0, i32 0))
  call void @"__struct#String_init"(%String* %y)
  %0 = call i1 @"__struct#String_operator#=="(%String* %x, %String* %y)
  %1 = zext i1 %0 to i64
  call void @printInt64(i64 %1)
  ret void
}

define void @"__automatic_init#String"(%String*) {
initialiser:
  %memberPtr_len = getelementptr inbounds %String* %0, i32 0, i32 0
  store i64 0, i64* %memberPtr_len
  %memberPtr_data = getelementptr inbounds %String* %0, i32 0, i32 1
  store i8* null, i8** %memberPtr_data
  %memberPtr_init = getelementptr inbounds %String* %0, i32 0, i32 2
  store void (%String*)* @"__struct#String_init", void (%String*)** %memberPtr_init
  %memberPtr_length = getelementptr inbounds %String* %0, i32 0, i32 3
  store i64 (%String*)* @"__struct#String_length", i64 (%String*)** %memberPtr_length
  %"memberPtr_operator#=" = getelementptr inbounds %String* %0, i32 0, i32 4
  store void (%String*, i8*)* @"__struct#String_operator#=", void (%String*, i8*)** %"memberPtr_operator#="
  %"memberPtr_operator#==" = getelementptr inbounds %String* %0, i32 0, i32 5
  store i1 (%String*, %String*)* @"__struct#String_operator#==", i1 (%String*, %String*)** %"memberPtr_operator#=="
  ret void
}

define void @"__struct#String_init"(%String* %self) {
entry:
  %self1 = alloca %String*
  store %String* %self, %String** %self1
  call void @"__automatic_init#String"(%String* %self)
  %memberPtr_len = getelementptr inbounds %String* %self, i32 0, i32 0
  store i64 64, i64* %memberPtr_len
  %memberPtr_data = getelementptr inbounds %String* %self, i32 0, i32 1
  %0 = call i8* @malloc(i64 64)
  store i8* %0, i8** %memberPtr_data
  ret void
}

define i64 @"__struct#String_length"(%String* %self) {
entry:
  %memberPtr_len = getelementptr inbounds %String* %self, i32 0, i32 0
  %0 = load i64* %memberPtr_len
  ret i64 %0
}

define void @"__struct#String_operator#="(%String* %self, i8* %other) {
entry:
  %memberPtr_data = getelementptr inbounds %String* %self, i32 0, i32 1
  store i8* %other, i8** %memberPtr_data
  ret void
}

define i1 @"__struct#String_operator#=="(%String* %self, %String* %other) {
entry:
  ret i1 false
}
