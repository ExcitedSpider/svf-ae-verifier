; A single GEP equivalent to m[i][j] with i == 1 and j == 2.
source_filename = "matrix_index.ll"
target datalayout = "e-p:64:64-i64:64-n32:64"

define i32 @main() !dbg !4 {
entry:
  %m = alloca [3 x [4 x i32]], align 4
  %i = add i64 0, 1
  %j = add i64 0, 2
  %cell = getelementptr inbounds [3 x [4 x i32]], ptr %m, i64 0, i64 %i, i64 %j
  store i32 1, ptr %cell, align 4, !dbg !8
  ret i32 0
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}
!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "fixture", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "matrix_index.ll", directory: ".")
!2 = !{i32 2, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 5, type: !5, scopeLine: 5, spFlags: DISPFlagDefinition, unit: !0)
!5 = !DISubroutineType(types: !6)
!6 = !{!7}
!7 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!8 = !DILocation(line: 11, column: 3, scope: !4)

; // EXPECT: line 11 store proved ae-in-bounds
; // OFFSET: line 11 24 24
