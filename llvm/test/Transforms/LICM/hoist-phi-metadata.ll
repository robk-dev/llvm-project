; Test that hoisting conditional branches copies the debug and profiling info
; metadata from the branch being hoisted.
; RUN: opt -S -passes=licm -licm-control-flow-hoisting=1 %s -o - | FileCheck %s

; CHECK-LABEL: @triangle_phi
define void @triangle_phi(i32 %x, ptr %p) {
; CHECK-LABEL: entry:
; CHECK: %cmp1 = icmp sgt i32 %x, 0
; CHECK: br i1 %cmp1, label %[[IF_LICM:.*]], label %[[THEN_LICM:.*]], !dbg [[ORIG_DBG:![0-9]+]], !prof [[ORIG_PROF:![0-9]+]]
entry:
  br label %loop, !dbg !5

; CHECK: [[IF_LICM]]:
; CHECK: %add = add i32 %x, 1
; CHECK: br label %[[THEN_LICM]]

; CHECK: [[THEN_LICM]]:
; CHECK: phi i32 [ %add, %[[IF_LICM]] ], [ %x, %entry ]
; CHECK: store i32 %phi, ptr %p
; CHECK: %cmp2 = icmp ne i32 %phi, 0
; CHECK: br label %loop

; CHECK-LABEL: loop:
loop:
  %cmp1 = icmp sgt i32 %x, 0
; CHECK: br i1 %cmp1, label %if, label %then, !dbg [[ORIG_DBG]], !prof [[ORIG_PROF]]
  br i1 %cmp1, label %if, label %then, !dbg !6, !prof !8
if:
  %add = add i32 %x, 1
  br label %then

; CHECK-LABEL: then:
then:
  %phi = phi i32 [ %add, %if ], [ %x, %loop ]
  store i32 %phi, ptr %p
  %cmp2 = icmp ne i32 %phi, 0
; CHECK-NEXT: br i1 %cmp2, label %loop, label %end, !dbg [[OTHER_DBG:![0-9]+]], !prof [[OTHER_PROF:![0-9]+]]
  br i1 %cmp2, label %loop, label %end, !dbg !7, !prof !9

; CHECK-LABEL: end:
end:
  ret void
}

; CHECK-DAG: [[ORIG_DBG]] = !DILocation(line: 2, column: 22
; CHECK-DAG: [[ORIG_PROF]] = !{!"branch_weights", i32 5, i32 7}
; CHECK-DAG: [[OTHER_DBG]] = !DILocation(line: 3, column: 22
; CHECK-DAG: [[OTHER_PROF]] = !{!"branch_weights", i32 13, i32 11}


!llvm.module.flags = !{!2, !3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1)
!1 = !DIFile(filename: "t", directory: "/")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = distinct !DISubprogram(name: "triangle_phi", linkageName: "triangle_phi", scope: !1, file: !1, line: 1, unit: !0)
!5 = !DILocation(line: 1, column: 22, scope: !4)
!6 = !DILocation(line: 2, column: 22, scope: !4)
!7 = !DILocation(line: 3, column: 22, scope: !4)
!8 = !{!"branch_weights", i32 5, i32 7}
!9 = !{!"branch_weights", i32 13, i32 11}
