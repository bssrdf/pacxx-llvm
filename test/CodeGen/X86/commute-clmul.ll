; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown -mattr=+sse2,+pclmul | FileCheck %s --check-prefix=SSE
; RUN: llc < %s -mtriple=x86_64-unknown -mattr=+avx2,+pclmul | FileCheck %s --check-prefix=AVX

declare <2 x i64> @llvm.x86.pclmulqdq(<2 x i64>, <2 x i64>, i8) nounwind readnone

define <2 x i64> @commute_lq_lq(<2 x i64>* %a0, <2 x i64> %a1) #0 {
; SSE-LABEL: commute_lq_lq:
; SSE:       # BB#0:
; SSE-NEXT:    pclmulqdq $0, (%rdi), %xmm0
; SSE-NEXT:    retq
;
; AVX-LABEL: commute_lq_lq:
; AVX:       # BB#0:
; AVX-NEXT:    vpclmulqdq $0, (%rdi), %xmm0, %xmm0
; AVX-NEXT:    retq
  %1 = load <2 x i64>, <2 x i64>* %a0
  %2 = call <2 x i64> @llvm.x86.pclmulqdq(<2 x i64> %1, <2 x i64> %a1, i8 0)
  ret <2 x i64> %2
}

define <2 x i64> @commute_lq_hq(<2 x i64>* %a0, <2 x i64> %a1) #0 {
; SSE-LABEL: commute_lq_hq:
; SSE:       # BB#0:
; SSE-NEXT:    pclmulqdq $1, (%rdi), %xmm0
; SSE-NEXT:    retq
;
; AVX-LABEL: commute_lq_hq:
; AVX:       # BB#0:
; AVX-NEXT:    vpclmulqdq $1, (%rdi), %xmm0, %xmm0
; AVX-NEXT:    retq
  %1 = load <2 x i64>, <2 x i64>* %a0
  %2 = call <2 x i64> @llvm.x86.pclmulqdq(<2 x i64> %1, <2 x i64> %a1, i8 16)
  ret <2 x i64> %2
}

define <2 x i64> @commute_hq_lq(<2 x i64>* %a0, <2 x i64> %a1) #0 {
; SSE-LABEL: commute_hq_lq:
; SSE:       # BB#0:
; SSE-NEXT:    pclmulqdq $16, (%rdi), %xmm0
; SSE-NEXT:    retq
;
; AVX-LABEL: commute_hq_lq:
; AVX:       # BB#0:
; AVX-NEXT:    vpclmulqdq $16, (%rdi), %xmm0, %xmm0
; AVX-NEXT:    retq
  %1 = load <2 x i64>, <2 x i64>* %a0
  %2 = call <2 x i64> @llvm.x86.pclmulqdq(<2 x i64> %1, <2 x i64> %a1, i8 1)
  ret <2 x i64> %2
}

define <2 x i64> @commute_hq_hq(<2 x i64>* %a0, <2 x i64> %a1) #0 {
; SSE-LABEL: commute_hq_hq:
; SSE:       # BB#0:
; SSE-NEXT:    pclmulqdq $17, (%rdi), %xmm0
; SSE-NEXT:    retq
;
; AVX-LABEL: commute_hq_hq:
; AVX:       # BB#0:
; AVX-NEXT:    vpclmulqdq $17, (%rdi), %xmm0, %xmm0
; AVX-NEXT:    retq
  %1 = load <2 x i64>, <2 x i64>* %a0
  %2 = call <2 x i64> @llvm.x86.pclmulqdq(<2 x i64> %1, <2 x i64> %a1, i8 17)
  ret <2 x i64> %2
}
