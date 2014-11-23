; ModuleID = 'build/test.bc'
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

%String = type { i64, i8*, void (%String*)*, i64 (%String*)*, void (%String*, i8*)*, i1 (%String*, %String*)* }

@0 = private unnamed_addr constant [14 x i8] c"Hello, World!\00"
@1 = private unnamed_addr constant [10 x i8] c"Hello, %s\00"
@2 = private unnamed_addr constant [14 x i8] c"Cocksuckers!\0A\00"
@3 = private unnamed_addr constant [12 x i8] c"int8: %hhd\0A\00"
@4 = private unnamed_addr constant [12 x i8] c"int16: %hd\0A\00"
@5 = private unnamed_addr constant [11 x i8] c"int32: %d\0A\00"
@6 = private unnamed_addr constant [13 x i8] c"int64: %lld\0A\00"

declare i32 @putchar(i32)

declare i8* @malloc(i64)

declare i32 @puts(i8*)

declare i32 @printf(i8*, ...)

define void @main(i32 %argc, i8** %argv) {
entry:
  %y = alloca %String
  %x = alloca %String
  call void @"__struct#String_init#_%String*"(%String* %x)
  call void @"__struct#String_operator#=#_%String*_i8*"(%String* %x, i8* getelementptr inbounds ([14 x i8]* @0, i32 0, i32 0))
  call void @"__struct#String_init#_%String*"(%String* %y)
  %0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([10 x i8]* @1, i32 0, i32 0), i8* getelementptr inbounds ([14 x i8]* @2, i32 0, i32 0))
  call void @"printInt#_i64"(i64 500)
  ret void
}

define void @"printInt#_i8"(i8 %x) {
entry:
  %0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([12 x i8]* @3, i32 0, i32 0), i8 %x)
  ret void
}

define void @"printInt#_i16"(i16 %x) {
entry:
  %0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([12 x i8]* @4, i32 0, i32 0), i16 %x)
  ret void
}

define void @"printInt#_i32"(i32 %x) {
entry:
  %0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([11 x i8]* @5, i32 0, i32 0), i32 %x)
  ret void
}

define void @"printInt#_i64"(i64 %x) {
entry:
  %0 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([13 x i8]* @6, i32 0, i32 0), i64 %x)
  ret void
}

define void @"__automatic_init#String"(%String*) {
initialiser:
  %memberPtr_len = getelementptr inbounds %String* %0, i32 0, i32 0
  store i64 0, i64* %memberPtr_len
  %memberPtr_data = getelementptr inbounds %String* %0, i32 0, i32 1
  store i8* null, i8** %memberPtr_data
  %memberPtr_init = getelementptr inbounds %String* %0, i32 0, i32 2
  store void (%String*)* @"__struct#String_init#_%String*", void (%String*)** %memberPtr_init
  %memberPtr_length = getelementptr inbounds %String* %0, i32 0, i32 3
  store i64 (%String*)* @"__struct#String_length#_%String*", i64 (%String*)** %memberPtr_length
  %"memberPtr_operator#=" = getelementptr inbounds %String* %0, i32 0, i32 4
  store void (%String*, i8*)* @"__struct#String_operator#=#_%String*_i8*", void (%String*, i8*)** %"memberPtr_operator#="
  %"memberPtr_operator#==" = getelementptr inbounds %String* %0, i32 0, i32 5
  store i1 (%String*, %String*)* @"__struct#String_operator#==#_%String*_%String*", i1 (%String*, %String*)** %"memberPtr_operator#=="
  ret void
}

define void @"__struct#String_init#_%String*"(%String* %self) {
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

define i64 @"__struct#String_length#_%String*"(%String* %self) {
entry:
  %memberPtr_len = getelementptr inbounds %String* %self, i32 0, i32 0
  %0 = load i64* %memberPtr_len
  ret i64 %0
}

define void @"__struct#String_operator#=#_%String*_i8*"(%String* %self, i8* %other) {
entry:
  %memberPtr_data = getelementptr inbounds %String* %self, i32 0, i32 1
  store i8* %other, i8** %memberPtr_data
  ret void
}

define i1 @"__struct#String_operator#==#_%String*_%String*"(%String* %self, %String* %other) {
entry:
  ret i1 false
}
