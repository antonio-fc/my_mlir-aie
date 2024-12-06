; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"
target triple = "aie2"

@in_cons_buff_1 = external global [1024 x i32]
@in_cons_buff_0 = external global [1024 x i32]
@infactor_cons_buff_1 = external global [1 x i32]
@infactor_cons_buff_0 = external global [1 x i32]
@in0to1_buff_1 = external global [1024 x i32]
@in0to1_buff_0 = external global [1024 x i32]
@in1to2_buff_1 = external global [1024 x i32]
@in1to2_buff_0 = external global [1024 x i32]
@in2to3_buff_1 = external global [1024 x i32]
@in2to3_buff_0 = external global [1024 x i32]
@out_buff_1 = external global [1024 x i32]
@out_buff_0 = external global [1024 x i32]
@out_cons = external global [1024 x i32]
@out = external global [1024 x i32]
@in2to3 = external global [1024 x i32]
@in1to2 = external global [1024 x i32]
@in0to1 = external global [1024 x i32]
@infactor_cons = external global [1 x i32]
@infactor = external global [1 x i32]
@in_cons = external global [1024 x i32]
@in = external global [1024 x i32]

declare void @debug_i32(i32)

declare void @llvm.aie2.put.ms(i32, i32)

declare { i32, i32 } @llvm.aie2.get.ss()

declare void @llvm.aie2.mcd.write.vec(<16 x i32>, i32)

declare <16 x i32> @llvm.aie2.scd.read.vec(i32)

declare void @llvm.aie2.acquire(i32, i32)

declare void @llvm.aie2.release(i32, i32)

declare void @vector_scalar_mul_aie_scalar(ptr, ptr, ptr, i32)

declare void @passthrough(ptr, ptr, i32)

define void @core_0_5() {
  br label %1

1:                                                ; preds = %35, %0
  %2 = phi i64 [ %36, %35 ], [ 0, %0 ]
  %3 = icmp slt i64 %2, 9223372036854775807
  br i1 %3, label %4, label %37

4:                                                ; preds = %33, %1
  %5 = phi i64 [ %34, %33 ], [ 0, %1 ]
  %6 = icmp slt i64 %5, 4
  br i1 %6, label %7, label %35

7:                                                ; preds = %4
  call void @llvm.aie2.acquire(i32 48, i32 -1)
  call void @llvm.aie2.acquire(i32 1, i32 -1)
  br label %8

8:                                                ; preds = %11, %7
  %9 = phi i64 [ %19, %11 ], [ 0, %7 ]
  %10 = icmp slt i64 %9, 1024
  br i1 %10, label %11, label %20

11:                                               ; preds = %8
  %12 = and i64 ptrtoint (ptr @in2to3_buff_0 to i64), 31
  %13 = icmp eq i64 %12, 0
  call void @llvm.assume(i1 %13)
  %14 = getelementptr i32, ptr @in2to3_buff_0, i64 %9
  %15 = load i32, ptr %14, align 4
  %16 = and i64 ptrtoint (ptr @out_buff_0 to i64), 31
  %17 = icmp eq i64 %16, 0
  call void @llvm.assume(i1 %17)
  %18 = getelementptr i32, ptr @out_buff_0, i64 %9
  store i32 %15, ptr %18, align 4
  %19 = add i64 %9, 1
  br label %8

20:                                               ; preds = %8
  call void @llvm.aie2.release(i32 0, i32 1)
  call void @llvm.aie2.release(i32 49, i32 1)
  call void @llvm.aie2.acquire(i32 48, i32 -1)
  call void @llvm.aie2.acquire(i32 1, i32 -1)
  br label %21

21:                                               ; preds = %24, %20
  %22 = phi i64 [ %32, %24 ], [ 0, %20 ]
  %23 = icmp slt i64 %22, 1024
  br i1 %23, label %24, label %33

24:                                               ; preds = %21
  %25 = and i64 ptrtoint (ptr @in2to3_buff_1 to i64), 31
  %26 = icmp eq i64 %25, 0
  call void @llvm.assume(i1 %26)
  %27 = getelementptr i32, ptr @in2to3_buff_1, i64 %22
  %28 = load i32, ptr %27, align 4
  %29 = and i64 ptrtoint (ptr @out_buff_1 to i64), 31
  %30 = icmp eq i64 %29, 0
  call void @llvm.assume(i1 %30)
  %31 = getelementptr i32, ptr @out_buff_1, i64 %22
  store i32 %28, ptr %31, align 4
  %32 = add i64 %22, 1
  br label %21

33:                                               ; preds = %21
  call void @llvm.aie2.release(i32 0, i32 1)
  call void @llvm.aie2.release(i32 49, i32 1)
  %34 = add i64 %5, 2
  br label %4

35:                                               ; preds = %4
  %36 = add i64 %2, 1
  br label %1

37:                                               ; preds = %1
  ret void
}

define void @core_0_4() {
  br label %1

1:                                                ; preds = %35, %0
  %2 = phi i64 [ %36, %35 ], [ 0, %0 ]
  %3 = icmp slt i64 %2, 9223372036854775807
  br i1 %3, label %4, label %37

4:                                                ; preds = %33, %1
  %5 = phi i64 [ %34, %33 ], [ 0, %1 ]
  %6 = icmp slt i64 %5, 4
  br i1 %6, label %7, label %35

7:                                                ; preds = %4
  call void @llvm.aie2.acquire(i32 48, i32 -1)
  call void @llvm.aie2.acquire(i32 1, i32 -1)
  br label %8

8:                                                ; preds = %11, %7
  %9 = phi i64 [ %19, %11 ], [ 0, %7 ]
  %10 = icmp slt i64 %9, 1024
  br i1 %10, label %11, label %20

11:                                               ; preds = %8
  %12 = and i64 ptrtoint (ptr @in1to2_buff_0 to i64), 31
  %13 = icmp eq i64 %12, 0
  call void @llvm.assume(i1 %13)
  %14 = getelementptr i32, ptr @in1to2_buff_0, i64 %9
  %15 = load i32, ptr %14, align 4
  %16 = and i64 ptrtoint (ptr @in2to3_buff_0 to i64), 31
  %17 = icmp eq i64 %16, 0
  call void @llvm.assume(i1 %17)
  %18 = getelementptr i32, ptr @in2to3_buff_0, i64 %9
  store i32 %15, ptr %18, align 4
  %19 = add i64 %9, 1
  br label %8

20:                                               ; preds = %8
  call void @llvm.aie2.release(i32 0, i32 1)
  call void @llvm.aie2.release(i32 49, i32 1)
  call void @llvm.aie2.acquire(i32 48, i32 -1)
  call void @llvm.aie2.acquire(i32 1, i32 -1)
  br label %21

21:                                               ; preds = %24, %20
  %22 = phi i64 [ %32, %24 ], [ 0, %20 ]
  %23 = icmp slt i64 %22, 1024
  br i1 %23, label %24, label %33

24:                                               ; preds = %21
  %25 = and i64 ptrtoint (ptr @in1to2_buff_1 to i64), 31
  %26 = icmp eq i64 %25, 0
  call void @llvm.assume(i1 %26)
  %27 = getelementptr i32, ptr @in1to2_buff_1, i64 %22
  %28 = load i32, ptr %27, align 4
  %29 = and i64 ptrtoint (ptr @in2to3_buff_1 to i64), 31
  %30 = icmp eq i64 %29, 0
  call void @llvm.assume(i1 %30)
  %31 = getelementptr i32, ptr @in2to3_buff_1, i64 %22
  store i32 %28, ptr %31, align 4
  %32 = add i64 %22, 1
  br label %21

33:                                               ; preds = %21
  call void @llvm.aie2.release(i32 0, i32 1)
  call void @llvm.aie2.release(i32 49, i32 1)
  %34 = add i64 %5, 2
  br label %4

35:                                               ; preds = %4
  %36 = add i64 %2, 1
  br label %1

37:                                               ; preds = %1
  ret void
}

define void @core_0_3() {
  br label %1

1:                                                ; preds = %17, %0
  %2 = phi i64 [ %18, %17 ], [ 0, %0 ]
  %3 = icmp slt i64 %2, 9223372036854775807
  br i1 %3, label %4, label %19

4:                                                ; preds = %7, %1
  %5 = phi i64 [ %16, %7 ], [ 0, %1 ]
  %6 = icmp slt i64 %5, 4
  br i1 %6, label %7, label %17

7:                                                ; preds = %4
  call void @llvm.aie2.acquire(i32 48, i32 -1)
  call void @llvm.aie2.acquire(i32 5, i32 -1)
  %8 = and i64 ptrtoint (ptr @in1to2_buff_0 to i64), 31
  %9 = icmp eq i64 %8, 0
  call void @llvm.assume(i1 %9)
  %10 = and i64 ptrtoint (ptr @in0to1_buff_0 to i64), 31
  %11 = icmp eq i64 %10, 0
  call void @llvm.assume(i1 %11)
  call void @passthrough(ptr @in0to1_buff_0, ptr @in1to2_buff_0, i32 1024)
  call void @llvm.aie2.release(i32 4, i32 1)
  call void @llvm.aie2.release(i32 49, i32 1)
  call void @llvm.aie2.acquire(i32 48, i32 -1)
  call void @llvm.aie2.acquire(i32 5, i32 -1)
  %12 = and i64 ptrtoint (ptr @in1to2_buff_1 to i64), 31
  %13 = icmp eq i64 %12, 0
  call void @llvm.assume(i1 %13)
  %14 = and i64 ptrtoint (ptr @in0to1_buff_1 to i64), 31
  %15 = icmp eq i64 %14, 0
  call void @llvm.assume(i1 %15)
  call void @passthrough(ptr @in0to1_buff_1, ptr @in1to2_buff_1, i32 1024)
  call void @llvm.aie2.release(i32 4, i32 1)
  call void @llvm.aie2.release(i32 49, i32 1)
  %16 = add i64 %5, 2
  br label %4

17:                                               ; preds = %4
  %18 = add i64 %2, 1
  br label %1

19:                                               ; preds = %1
  ret void
}

define void @core_0_2() {
  br label %1

1:                                                ; preds = %36, %0
  %2 = phi i64 [ %37, %36 ], [ 0, %0 ]
  %3 = icmp slt i64 %2, 9223372036854775806
  br i1 %3, label %4, label %38

4:                                                ; preds = %1
  call void @llvm.aie2.acquire(i32 51, i32 -1)
  br label %5

5:                                                ; preds = %8, %4
  %6 = phi i64 [ %19, %8 ], [ 0, %4 ]
  %7 = icmp slt i64 %6, 4
  br i1 %7, label %8, label %20

8:                                                ; preds = %5
  call void @llvm.aie2.acquire(i32 52, i32 -1)
  call void @llvm.aie2.acquire(i32 49, i32 -1)
  %9 = and i64 ptrtoint (ptr @in0to1_buff_0 to i64), 31
  %10 = icmp eq i64 %9, 0
  call void @llvm.assume(i1 %10)
  %11 = and i64 ptrtoint (ptr @infactor_cons_buff_0 to i64), 31
  %12 = icmp eq i64 %11, 0
  call void @llvm.assume(i1 %12)
  %13 = and i64 ptrtoint (ptr @in_cons_buff_0 to i64), 31
  %14 = icmp eq i64 %13, 0
  call void @llvm.assume(i1 %14)
  call void @vector_scalar_mul_aie_scalar(ptr @in_cons_buff_0, ptr @in0to1_buff_0, ptr @infactor_cons_buff_0, i32 1024)
  call void @llvm.aie2.release(i32 48, i32 1)
  call void @llvm.aie2.release(i32 53, i32 1)
  call void @llvm.aie2.acquire(i32 52, i32 -1)
  call void @llvm.aie2.acquire(i32 49, i32 -1)
  %15 = and i64 ptrtoint (ptr @in0to1_buff_1 to i64), 31
  %16 = icmp eq i64 %15, 0
  call void @llvm.assume(i1 %16)
  call void @llvm.assume(i1 %12)
  %17 = and i64 ptrtoint (ptr @in_cons_buff_1 to i64), 31
  %18 = icmp eq i64 %17, 0
  call void @llvm.assume(i1 %18)
  call void @vector_scalar_mul_aie_scalar(ptr @in_cons_buff_1, ptr @in0to1_buff_1, ptr @infactor_cons_buff_0, i32 1024)
  call void @llvm.aie2.release(i32 48, i32 1)
  call void @llvm.aie2.release(i32 53, i32 1)
  %19 = add i64 %6, 2
  br label %5

20:                                               ; preds = %5
  call void @llvm.aie2.release(i32 50, i32 1)
  call void @llvm.aie2.acquire(i32 51, i32 -1)
  br label %21

21:                                               ; preds = %24, %20
  %22 = phi i64 [ %35, %24 ], [ 0, %20 ]
  %23 = icmp slt i64 %22, 4
  br i1 %23, label %24, label %36

24:                                               ; preds = %21
  call void @llvm.aie2.acquire(i32 52, i32 -1)
  call void @llvm.aie2.acquire(i32 49, i32 -1)
  %25 = and i64 ptrtoint (ptr @in0to1_buff_0 to i64), 31
  %26 = icmp eq i64 %25, 0
  call void @llvm.assume(i1 %26)
  %27 = and i64 ptrtoint (ptr @infactor_cons_buff_1 to i64), 31
  %28 = icmp eq i64 %27, 0
  call void @llvm.assume(i1 %28)
  %29 = and i64 ptrtoint (ptr @in_cons_buff_0 to i64), 31
  %30 = icmp eq i64 %29, 0
  call void @llvm.assume(i1 %30)
  call void @vector_scalar_mul_aie_scalar(ptr @in_cons_buff_0, ptr @in0to1_buff_0, ptr @infactor_cons_buff_1, i32 1024)
  call void @llvm.aie2.release(i32 48, i32 1)
  call void @llvm.aie2.release(i32 53, i32 1)
  call void @llvm.aie2.acquire(i32 52, i32 -1)
  call void @llvm.aie2.acquire(i32 49, i32 -1)
  %31 = and i64 ptrtoint (ptr @in0to1_buff_1 to i64), 31
  %32 = icmp eq i64 %31, 0
  call void @llvm.assume(i1 %32)
  call void @llvm.assume(i1 %28)
  %33 = and i64 ptrtoint (ptr @in_cons_buff_1 to i64), 31
  %34 = icmp eq i64 %33, 0
  call void @llvm.assume(i1 %34)
  call void @vector_scalar_mul_aie_scalar(ptr @in_cons_buff_1, ptr @in0to1_buff_1, ptr @infactor_cons_buff_1, i32 1024)
  call void @llvm.aie2.release(i32 48, i32 1)
  call void @llvm.aie2.release(i32 53, i32 1)
  %35 = add i64 %22, 2
  br label %21

36:                                               ; preds = %21
  call void @llvm.aie2.release(i32 50, i32 1)
  %37 = add i64 %2, 2
  br label %1

38:                                               ; preds = %1
  call void @llvm.aie2.acquire(i32 51, i32 -1)
  br label %39

39:                                               ; preds = %42, %38
  %40 = phi i64 [ %53, %42 ], [ 0, %38 ]
  %41 = icmp slt i64 %40, 4
  br i1 %41, label %42, label %54

42:                                               ; preds = %39
  call void @llvm.aie2.acquire(i32 52, i32 -1)
  call void @llvm.aie2.acquire(i32 49, i32 -1)
  %43 = and i64 ptrtoint (ptr @in0to1_buff_0 to i64), 31
  %44 = icmp eq i64 %43, 0
  call void @llvm.assume(i1 %44)
  %45 = and i64 ptrtoint (ptr @infactor_cons_buff_0 to i64), 31
  %46 = icmp eq i64 %45, 0
  call void @llvm.assume(i1 %46)
  %47 = and i64 ptrtoint (ptr @in_cons_buff_0 to i64), 31
  %48 = icmp eq i64 %47, 0
  call void @llvm.assume(i1 %48)
  call void @vector_scalar_mul_aie_scalar(ptr @in_cons_buff_0, ptr @in0to1_buff_0, ptr @infactor_cons_buff_0, i32 1024)
  call void @llvm.aie2.release(i32 48, i32 1)
  call void @llvm.aie2.release(i32 53, i32 1)
  call void @llvm.aie2.acquire(i32 52, i32 -1)
  call void @llvm.aie2.acquire(i32 49, i32 -1)
  %49 = and i64 ptrtoint (ptr @in0to1_buff_1 to i64), 31
  %50 = icmp eq i64 %49, 0
  call void @llvm.assume(i1 %50)
  call void @llvm.assume(i1 %46)
  %51 = and i64 ptrtoint (ptr @in_cons_buff_1 to i64), 31
  %52 = icmp eq i64 %51, 0
  call void @llvm.assume(i1 %52)
  call void @vector_scalar_mul_aie_scalar(ptr @in_cons_buff_1, ptr @in0to1_buff_1, ptr @infactor_cons_buff_0, i32 1024)
  call void @llvm.aie2.release(i32 48, i32 1)
  call void @llvm.aie2.release(i32 53, i32 1)
  %53 = add i64 %40, 2
  br label %39

54:                                               ; preds = %39
  call void @llvm.aie2.release(i32 50, i32 1)
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write)
declare void @llvm.assume(i1 noundef) #0

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write) }

!llvm.module.flags = !{!0}

!0 = !{i32 2, !"Debug Info Version", i32 3}
