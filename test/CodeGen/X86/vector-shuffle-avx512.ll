; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-pc-linux-gnu -mcpu=skx | FileCheck %s --check-prefix=SKX64
; RUN: llc < %s -mtriple=x86_64-pc-linux-gnu -mcpu=knl | FileCheck %s --check-prefix=KNL64
; RUN: llc < %s -mtriple=i386-pc-linux-gnu -mcpu=skx | FileCheck %s --check-prefix=SKX32
; RUN: llc < %s -mtriple=i386-pc-linux-gnu -mcpu=knl | FileCheck %s --check-prefix=KNL32

;expand 128 -> 256 include <4 x float> <2 x double>
define <8 x float> @expand(<4 x float> %a) {
; SKX64-LABEL: expand:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX64-NEXT:    movb $5, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vexpandps %ymm0, %ymm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand:
; KNL64:       # BB#0:
; KNL64-NEXT:    vpermilps {{.*#+}} xmm0 = xmm0[0,1,1,3]
; KNL64-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vblendps {{.*#+}} ymm0 = ymm0[0],ymm1[1],ymm0[2],ymm1[3,4,5,6,7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX32-NEXT:    movb $5, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vexpandps %ymm0, %ymm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand:
; KNL32:       # BB#0:
; KNL32-NEXT:    vpermilps {{.*#+}} xmm0 = xmm0[0,1,1,3]
; KNL32-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vblendps {{.*#+}} ymm0 = ymm0[0],ymm1[1],ymm0[2],ymm1[3,4,5,6,7]
; KNL32-NEXT:    retl
   %res = shufflevector <4 x float> %a, <4 x float> zeroinitializer, <8 x i32> <i32 0, i32 5, i32 1, i32 5, i32 5, i32 5, i32 5, i32 5>
   ret <8 x float> %res
}

define <8 x float> @expand1(<4 x float> %a ) {
; SKX64-LABEL: expand1:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX64-NEXT:    movb $-86, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vexpandps %ymm0, %ymm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand1:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL64-NEXT:    vmovaps {{.*#+}} ymm1 = <u,0,u,1,u,2,u,3>
; KNL64-NEXT:    vpermps %ymm0, %ymm1, %ymm0
; KNL64-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0],ymm0[1],ymm1[2],ymm0[3],ymm1[4],ymm0[5],ymm1[6],ymm0[7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand1:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX32-NEXT:    movb $-86, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vexpandps %ymm0, %ymm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand1:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL32-NEXT:    vmovaps {{.*#+}} ymm1 = <u,0,u,1,u,2,u,3>
; KNL32-NEXT:    vpermps %ymm0, %ymm1, %ymm0
; KNL32-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0],ymm0[1],ymm1[2],ymm0[3],ymm1[4],ymm0[5],ymm1[6],ymm0[7]
; KNL32-NEXT:    retl
   %res = shufflevector <4 x float> zeroinitializer, <4 x float> %a, <8 x i32> <i32 0, i32 4, i32 1, i32 5, i32 2, i32 6, i32 3, i32 7>
   ret <8 x float> %res
}

;Expand 128 -> 256 test <2 x double> -> <4 x double>
define <4 x double> @expand2(<2 x double> %a) {
; SKX64-LABEL: expand2:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX64-NEXT:    movb $9, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vexpandpd %ymm0, %ymm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand2:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL64-NEXT:    vpermpd {{.*#+}} ymm0 = ymm0[0,1,2,1]
; KNL64-NEXT:    vxorpd %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vblendpd {{.*#+}} ymm0 = ymm0[0],ymm1[1,2],ymm0[3]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand2:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX32-NEXT:    movb $9, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vexpandpd %ymm0, %ymm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand2:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL32-NEXT:    vpermpd {{.*#+}} ymm0 = ymm0[0,1,2,1]
; KNL32-NEXT:    vxorpd %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vblendpd {{.*#+}} ymm0 = ymm0[0],ymm1[1,2],ymm0[3]
; KNL32-NEXT:    retl
   %res = shufflevector <2 x double> %a, <2 x double> zeroinitializer, <4 x i32> <i32 0, i32 2, i32 2, i32 1>
   ret <4 x double> %res
}

;expand 128 -> 256 include case <4 x i32> <8 x i32>
define <8 x i32> @expand3(<4 x i32> %a ) {
; SKX64-LABEL: expand3:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX64-NEXT:    movb $-127, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vpexpandd %ymm0, %ymm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand3:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL64-NEXT:    vpbroadcastq %xmm0, %ymm0
; KNL64-NEXT:    vpxor %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vpblendd {{.*#+}} ymm0 = ymm0[0],ymm1[1,2,3,4,5,6],ymm0[7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand3:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX32-NEXT:    movb $-127, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vpexpandd %ymm0, %ymm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand3:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL32-NEXT:    vpbroadcastq %xmm0, %ymm0
; KNL32-NEXT:    vpxor %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vpblendd {{.*#+}} ymm0 = ymm0[0],ymm1[1,2,3,4,5,6],ymm0[7]
; KNL32-NEXT:    retl
   %res = shufflevector <4 x i32> zeroinitializer, <4 x i32> %a, <8 x i32> <i32 4, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0,i32 5>
   ret <8 x i32> %res
}

;expand 128 -> 256 include case <2 x i64> <4 x i64>
define <4 x i64> @expand4(<2 x i64> %a ) {
; SKX64-LABEL: expand4:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX64-NEXT:    movb $9, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vpexpandq %ymm0, %ymm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand4:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL64-NEXT:    vpermq {{.*#+}} ymm0 = ymm0[0,1,2,1]
; KNL64-NEXT:    vpxor %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vpblendd {{.*#+}} ymm0 = ymm0[0,1],ymm1[2,3,4,5],ymm0[6,7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand4:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX32-NEXT:    movb $9, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vpexpandq %ymm0, %ymm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand4:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; KNL32-NEXT:    vpermq {{.*#+}} ymm0 = ymm0[0,1,2,1]
; KNL32-NEXT:    vpxor %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vpblendd {{.*#+}} ymm0 = ymm0[0,1],ymm1[2,3,4,5],ymm0[6,7]
; KNL32-NEXT:    retl
   %res = shufflevector <2 x i64> zeroinitializer, <2 x i64> %a, <4 x i32> <i32 2, i32 0, i32 0, i32 3>
   ret <4 x i64> %res
}

;Negative test for 128-> 256
define <8 x float> @expand5(<4 x float> %a ) {
; SKX64-LABEL: expand5:
; SKX64:       # BB#0:
; SKX64-NEXT:    vbroadcastss %xmm0, %ymm0
; SKX64-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; SKX64-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0],ymm0[1],ymm1[2],ymm0[3],ymm1[4],ymm0[5],ymm1[6],ymm0[7]
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand5:
; KNL64:       # BB#0:
; KNL64-NEXT:    vbroadcastss %xmm0, %ymm0
; KNL64-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0],ymm0[1],ymm1[2],ymm0[3],ymm1[4],ymm0[5],ymm1[6],ymm0[7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand5:
; SKX32:       # BB#0:
; SKX32-NEXT:    vbroadcastss %xmm0, %ymm0
; SKX32-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; SKX32-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0],ymm0[1],ymm1[2],ymm0[3],ymm1[4],ymm0[5],ymm1[6],ymm0[7]
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand5:
; KNL32:       # BB#0:
; KNL32-NEXT:    vbroadcastss %xmm0, %ymm0
; KNL32-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0],ymm0[1],ymm1[2],ymm0[3],ymm1[4],ymm0[5],ymm1[6],ymm0[7]
; KNL32-NEXT:    retl
   %res = shufflevector <4 x float> zeroinitializer, <4 x float> %a, <8 x i32> <i32 0, i32 4, i32 1, i32 4, i32 2, i32 4, i32 3, i32 4>
   ret <8 x float> %res
}

;expand 256 -> 512 include <8 x float> <16 x float>
define <8 x float> @expand6(<4 x float> %a ) {
; SKX64-LABEL: expand6:
; SKX64:       # BB#0:
; SKX64-NEXT:    vxorps %xmm1, %xmm1, %xmm1
; SKX64-NEXT:    vinsertf128 $1, %xmm0, %ymm1, %ymm0
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand6:
; KNL64:       # BB#0:
; KNL64-NEXT:    vxorps %xmm1, %xmm1, %xmm1
; KNL64-NEXT:    vinsertf128 $1, %xmm0, %ymm1, %ymm0
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand6:
; SKX32:       # BB#0:
; SKX32-NEXT:    vxorps %xmm1, %xmm1, %xmm1
; SKX32-NEXT:    vinsertf128 $1, %xmm0, %ymm1, %ymm0
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand6:
; KNL32:       # BB#0:
; KNL32-NEXT:    vxorps %xmm1, %xmm1, %xmm1
; KNL32-NEXT:    vinsertf128 $1, %xmm0, %ymm1, %ymm0
; KNL32-NEXT:    retl
   %res = shufflevector <4 x float> zeroinitializer, <4 x float> %a, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
   ret <8 x float> %res
}

define <16 x float> @expand7(<8 x float> %a) {
; SKX64-LABEL: expand7:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX64-NEXT:    movw $1285, %ax # imm = 0x505
; SKX64-NEXT:    kmovw %eax, %k1
; SKX64-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand7:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL64-NEXT:    movw $1285, %ax # imm = 0x505
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand7:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX32-NEXT:    movw $1285, %ax # imm = 0x505
; SKX32-NEXT:    kmovw %eax, %k1
; SKX32-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand7:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL32-NEXT:    movw $1285, %ax # imm = 0x505
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; KNL32-NEXT:    retl
   %res = shufflevector <8 x float> %a, <8 x float> zeroinitializer, <16 x i32> <i32 0, i32 8, i32 1, i32 8, i32 8, i32 8, i32 8, i32 8, i32 2, i32 8, i32 3, i32 8, i32 8, i32 8, i32 8, i32 8>
   ret <16 x float> %res
}

define <16 x float> @expand8(<8 x float> %a ) {
; SKX64-LABEL: expand8:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX64-NEXT:    kmovw %eax, %k1
; SKX64-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand8:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand8:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX32-NEXT:    kmovw %eax, %k1
; SKX32-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand8:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vexpandps %zmm0, %zmm0 {%k1} {z}
; KNL32-NEXT:    retl
   %res = shufflevector <8 x float> zeroinitializer, <8 x float> %a, <16 x i32> <i32 0, i32 8, i32 1, i32 9, i32 2, i32 10, i32 3, i32 11, i32 4, i32 12, i32 5, i32 13, i32 6, i32 14, i32 7, i32 15>
   ret <16 x float> %res
}

;expand 256 -> 512 include <4 x double> <8 x double>
define <8 x double> @expand9(<4 x double> %a) {
; SKX64-LABEL: expand9:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX64-NEXT:    movb $-127, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vexpandpd %zmm0, %zmm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand9:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL64-NEXT:    movb $-127, %al
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vexpandpd %zmm0, %zmm0 {%k1} {z}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand9:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX32-NEXT:    movb $-127, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vexpandpd %zmm0, %zmm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand9:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL32-NEXT:    movb $-127, %al
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vexpandpd %zmm0, %zmm0 {%k1} {z}
; KNL32-NEXT:    retl
   %res = shufflevector <4 x double> %a, <4 x double> zeroinitializer, <8 x i32> <i32 0, i32 4, i32 4, i32 4, i32 4, i32 4, i32 4, i32 1>
   ret <8 x double> %res
}

define <16 x i32> @expand10(<8 x i32> %a ) {
; SKX64-LABEL: expand10:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX64-NEXT:    kmovw %eax, %k1
; SKX64-NEXT:    vpexpandd %zmm0, %zmm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand10:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vpexpandd %zmm0, %zmm0 {%k1} {z}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand10:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX32-NEXT:    kmovw %eax, %k1
; SKX32-NEXT:    vpexpandd %zmm0, %zmm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand10:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vpexpandd %zmm0, %zmm0 {%k1} {z}
; KNL32-NEXT:    retl
   %res = shufflevector <8 x i32> zeroinitializer, <8 x i32> %a, <16 x i32> <i32 0, i32 8, i32 1, i32 9, i32 2, i32 10, i32 3, i32 11, i32 4, i32 12, i32 5, i32 13, i32 6, i32 14, i32 7, i32 15>
   ret <16 x i32> %res
}

define <8 x i64> @expand11(<4 x i64> %a) {
; SKX64-LABEL: expand11:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX64-NEXT:    movb $-127, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vpexpandq %zmm0, %zmm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand11:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL64-NEXT:    movb $-127, %al
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vpexpandq %zmm0, %zmm0 {%k1} {z}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand11:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX32-NEXT:    movb $-127, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vpexpandq %zmm0, %zmm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand11:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL32-NEXT:    movb $-127, %al
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vpexpandq %zmm0, %zmm0 {%k1} {z}
; KNL32-NEXT:    retl
   %res = shufflevector <4 x i64> %a, <4 x i64> zeroinitializer, <8 x i32> <i32 0, i32 4, i32 4, i32 4, i32 4, i32 4, i32 4, i32 1>
   ret <8 x i64> %res
}

;Negative test for 256-> 512
define <16 x float> @expand12(<8 x float> %a) {
; SKX64-LABEL: expand12:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX64-NEXT:    vmovaps {{.*#+}} zmm2 = [0,16,2,16,4,16,6,16,0,16,1,16,2,16,3,16]
; SKX64-NEXT:    vxorps %zmm1, %zmm1, %zmm1
; SKX64-NEXT:    vpermt2ps %zmm0, %zmm2, %zmm1
; SKX64-NEXT:    vmovaps %zmm1, %zmm0
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand12:
; KNL64:       # BB#0:
; KNL64-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL64-NEXT:    vmovaps {{.*#+}} zmm2 = [0,16,2,16,4,16,6,16,0,16,1,16,2,16,3,16]
; KNL64-NEXT:    vpxord %zmm1, %zmm1, %zmm1
; KNL64-NEXT:    vpermt2ps %zmm0, %zmm2, %zmm1
; KNL64-NEXT:    vmovaps %zmm1, %zmm0
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand12:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; SKX32-NEXT:    vmovaps {{.*#+}} zmm2 = [0,16,2,16,4,16,6,16,0,16,1,16,2,16,3,16]
; SKX32-NEXT:    vxorps %zmm1, %zmm1, %zmm1
; SKX32-NEXT:    vpermt2ps %zmm0, %zmm2, %zmm1
; SKX32-NEXT:    vmovaps %zmm1, %zmm0
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand12:
; KNL32:       # BB#0:
; KNL32-NEXT:    # kill: %YMM0<def> %YMM0<kill> %ZMM0<def>
; KNL32-NEXT:    vmovaps {{.*#+}} zmm2 = [0,16,2,16,4,16,6,16,0,16,1,16,2,16,3,16]
; KNL32-NEXT:    vpxord %zmm1, %zmm1, %zmm1
; KNL32-NEXT:    vpermt2ps %zmm0, %zmm2, %zmm1
; KNL32-NEXT:    vmovaps %zmm1, %zmm0
; KNL32-NEXT:    retl
   %res = shufflevector <8 x float> zeroinitializer, <8 x float> %a, <16 x i32> <i32 0, i32 8, i32 1, i32 8, i32 2, i32 8, i32 3, i32 8,i32 0, i32 8, i32 1, i32 8, i32 2, i32 8, i32 3, i32 8>
   ret <16 x float> %res
}

define <16 x float> @expand13(<8 x float> %a ) {
; SKX64-LABEL: expand13:
; SKX64:       # BB#0:
; SKX64-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; SKX64-NEXT:    vinsertf32x8 $1, %ymm0, %zmm1, %zmm0
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand13:
; KNL64:       # BB#0:
; KNL64-NEXT:    vxorpd %ymm1, %ymm1, %ymm1
; KNL64-NEXT:    vinsertf64x4 $1, %ymm0, %zmm1, %zmm0
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand13:
; SKX32:       # BB#0:
; SKX32-NEXT:    vxorps %ymm1, %ymm1, %ymm1
; SKX32-NEXT:    vinsertf32x8 $1, %ymm0, %zmm1, %zmm0
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand13:
; KNL32:       # BB#0:
; KNL32-NEXT:    vxorpd %ymm1, %ymm1, %ymm1
; KNL32-NEXT:    vinsertf64x4 $1, %ymm0, %zmm1, %zmm0
; KNL32-NEXT:    retl
   %res = shufflevector <8 x float> zeroinitializer, <8 x float> %a, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7,i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
   ret <16 x float> %res
}

; The function checks for a case where the vector is mixed values vector ,and the mask points on zero elements from this vector.

define <8 x float> @expand14(<4 x float> %a) {
; SKX64-LABEL: expand14:
; SKX64:       # BB#0:
; SKX64-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX64-NEXT:    movb $20, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vexpandps %ymm0, %ymm0 {%k1} {z}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand14:
; KNL64:       # BB#0:
; KNL64-NEXT:    vpermilps {{.*#+}} xmm0 = xmm0[0,1,1,3]
; KNL64-NEXT:    vpermpd {{.*#+}} ymm0 = ymm0[0,0,1,3]
; KNL64-NEXT:    vmovaps {{.*#+}} ymm1 = <0,2,4,0,u,u,u,u>
; KNL64-NEXT:    vpermilps {{.*#+}} xmm1 = xmm1[3,3,0,0]
; KNL64-NEXT:    vpermpd {{.*#+}} ymm1 = ymm1[0,1,1,1]
; KNL64-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0,1],ymm0[2],ymm1[3],ymm0[4],ymm1[5,6,7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand14:
; SKX32:       # BB#0:
; SKX32-NEXT:    # kill: %XMM0<def> %XMM0<kill> %YMM0<def>
; SKX32-NEXT:    movb $20, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vexpandps %ymm0, %ymm0 {%k1} {z}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand14:
; KNL32:       # BB#0:
; KNL32-NEXT:    vpermilps {{.*#+}} xmm0 = xmm0[0,1,1,3]
; KNL32-NEXT:    vpermpd {{.*#+}} ymm0 = ymm0[0,0,1,3]
; KNL32-NEXT:    vmovaps {{.*#+}} ymm1 = <0,2,4,0,u,u,u,u>
; KNL32-NEXT:    vpermilps {{.*#+}} xmm1 = xmm1[3,3,0,0]
; KNL32-NEXT:    vpermpd {{.*#+}} ymm1 = ymm1[0,1,1,1]
; KNL32-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0,1],ymm0[2],ymm1[3],ymm0[4],ymm1[5,6,7]
; KNL32-NEXT:    retl
   %addV = fadd <4 x float> <float 0.0,float 1.0,float 2.0,float 0.0> , <float 0.0,float 1.0,float 2.0,float 0.0>
   %res = shufflevector <4 x float> %addV, <4 x float> %a, <8 x i32> <i32 3, i32 3, i32 4, i32 0, i32 5, i32 0, i32 0, i32 0>
   ret <8 x float> %res
}

;Negative test.
define <8 x float> @expand15(<4 x float> %a) {
; SKX64-LABEL: expand15:
; SKX64:       # BB#0:
; SKX64-NEXT:    vpermilps {{.*#+}} xmm1 = xmm0[0,1,1,3]
; SKX64-NEXT:    vmovaps {{.*#+}} ymm0 = <0,2,4,0,u,u,u,u>
; SKX64-NEXT:    vpermilps {{.*#+}} xmm2 = xmm0[0,1,0,0]
; SKX64-NEXT:    vmovaps {{.*#+}} ymm0 = [0,1,8,3,10,3,2,3]
; SKX64-NEXT:    vpermi2ps %ymm1, %ymm2, %ymm0
; SKX64-NEXT:    retq
;
; KNL64-LABEL: expand15:
; KNL64:       # BB#0:
; KNL64-NEXT:    vpermilps {{.*#+}} xmm0 = xmm0[0,1,1,3]
; KNL64-NEXT:    vpermpd {{.*#+}} ymm0 = ymm0[0,0,1,3]
; KNL64-NEXT:    vmovaps {{.*#+}} ymm1 = <0,2,4,0,u,u,u,u>
; KNL64-NEXT:    vpermilps {{.*#+}} xmm1 = xmm1[0,1,0,0]
; KNL64-NEXT:    vpermpd {{.*#+}} ymm1 = ymm1[0,1,1,1]
; KNL64-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0,1],ymm0[2],ymm1[3],ymm0[4],ymm1[5,6,7]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: expand15:
; SKX32:       # BB#0:
; SKX32-NEXT:    vpermilps {{.*#+}} xmm1 = xmm0[0,1,1,3]
; SKX32-NEXT:    vmovaps {{.*#+}} ymm0 = <0,2,4,0,u,u,u,u>
; SKX32-NEXT:    vpermilps {{.*#+}} xmm2 = xmm0[0,1,0,0]
; SKX32-NEXT:    vmovaps {{.*#+}} ymm0 = [0,1,8,3,10,3,2,3]
; SKX32-NEXT:    vpermi2ps %ymm1, %ymm2, %ymm0
; SKX32-NEXT:    retl
;
; KNL32-LABEL: expand15:
; KNL32:       # BB#0:
; KNL32-NEXT:    vpermilps {{.*#+}} xmm0 = xmm0[0,1,1,3]
; KNL32-NEXT:    vpermpd {{.*#+}} ymm0 = ymm0[0,0,1,3]
; KNL32-NEXT:    vmovaps {{.*#+}} ymm1 = <0,2,4,0,u,u,u,u>
; KNL32-NEXT:    vpermilps {{.*#+}} xmm1 = xmm1[0,1,0,0]
; KNL32-NEXT:    vpermpd {{.*#+}} ymm1 = ymm1[0,1,1,1]
; KNL32-NEXT:    vblendps {{.*#+}} ymm0 = ymm1[0,1],ymm0[2],ymm1[3],ymm0[4],ymm1[5,6,7]
; KNL32-NEXT:    retl
   %addV = fadd <4 x float> <float 0.0,float 1.0,float 2.0,float 0.0> , <float 0.0,float 1.0,float 2.0,float 0.0>
   %res = shufflevector <4 x float> %addV, <4 x float> %a, <8 x i32> <i32 0, i32 1, i32 4, i32 0, i32 5, i32 0, i32 0, i32 0>
   ret <8 x float> %res
}


; Shuffle to blend test

define <64 x i8> @test_mm512_mask_blend_epi8(<64 x i8> %A, <64 x i8> %W){
; SKX64-LABEL: test_mm512_mask_blend_epi8:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movabsq $-6148914691236517206, %rax # imm = 0xAAAAAAAAAAAAAAAA
; SKX64-NEXT:    kmovq %rax, %k1
; SKX64-NEXT:    vpblendmb %zmm0, %zmm1, %zmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm512_mask_blend_epi8:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    vpbroadcastw {{.*}}(%rip), %ymm4
; KNL64-NEXT:    vpblendvb %ymm4, %ymm2, %ymm0, %ymm0
; KNL64-NEXT:    vpblendvb %ymm4, %ymm3, %ymm1, %ymm1
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm512_mask_blend_epi8:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movl $-1431655766, %eax # imm = 0xAAAAAAAA
; SKX32-NEXT:    kmovd %eax, %k0
; SKX32-NEXT:    kunpckdq %k0, %k0, %k1
; SKX32-NEXT:    vpblendmb %zmm0, %zmm1, %zmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm512_mask_blend_epi8:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    pushl %ebp
; KNL32-NEXT:  .Lcfi0:
; KNL32-NEXT:    .cfi_def_cfa_offset 8
; KNL32-NEXT:  .Lcfi1:
; KNL32-NEXT:    .cfi_offset %ebp, -8
; KNL32-NEXT:    movl %esp, %ebp
; KNL32-NEXT:  .Lcfi2:
; KNL32-NEXT:    .cfi_def_cfa_register %ebp
; KNL32-NEXT:    andl $-32, %esp
; KNL32-NEXT:    subl $32, %esp
; KNL32-NEXT:    vpbroadcastw {{\.LCPI.*}}, %ymm3
; KNL32-NEXT:    vpblendvb %ymm3, %ymm2, %ymm0, %ymm0
; KNL32-NEXT:    vpblendvb %ymm3, 8(%ebp), %ymm1, %ymm1
; KNL32-NEXT:    movl %ebp, %esp
; KNL32-NEXT:    popl %ebp
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <64 x i8> %A, <64 x i8> %W, <64 x i32>  <i32 64, i32 1, i32 66, i32 3, i32 68, i32 5, i32 70, i32 7, i32 72, i32 9, i32 74, i32 11, i32 76, i32 13, i32 78, i32 15, i32 80, i32 17, i32 82, i32 19, i32 84, i32 21, i32 86, i32 23, i32 88, i32 25, i32 90, i32 27, i32 92, i32 29, i32 94, i32 31, i32 96, i32 33, i32 98, i32 35, i32 100, i32 37, i32 102, i32 39, i32 104, i32 41, i32 106, i32 43, i32 108, i32 45, i32 110, i32 47, i32 112, i32 49, i32 114, i32 51, i32 116, i32 53, i32 118, i32 55, i32 120, i32 57, i32 122, i32 59, i32 124, i32 61, i32 126, i32 63>
  ret <64 x i8> %0
}

define <32 x i16> @test_mm512_mask_blend_epi16(<32 x i16> %A, <32 x i16> %W){
; SKX64-LABEL: test_mm512_mask_blend_epi16:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movl $-1431655766, %eax # imm = 0xAAAAAAAA
; SKX64-NEXT:    kmovd %eax, %k1
; SKX64-NEXT:    vpblendmw %zmm0, %zmm1, %zmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm512_mask_blend_epi16:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    vpblendw {{.*#+}} ymm0 = ymm2[0],ymm0[1],ymm2[2],ymm0[3],ymm2[4],ymm0[5],ymm2[6],ymm0[7],ymm2[8],ymm0[9],ymm2[10],ymm0[11],ymm2[12],ymm0[13],ymm2[14],ymm0[15]
; KNL64-NEXT:    vpblendw {{.*#+}} ymm1 = ymm3[0],ymm1[1],ymm3[2],ymm1[3],ymm3[4],ymm1[5],ymm3[6],ymm1[7],ymm3[8],ymm1[9],ymm3[10],ymm1[11],ymm3[12],ymm1[13],ymm3[14],ymm1[15]
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm512_mask_blend_epi16:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movl $-1431655766, %eax # imm = 0xAAAAAAAA
; SKX32-NEXT:    kmovd %eax, %k1
; SKX32-NEXT:    vpblendmw %zmm0, %zmm1, %zmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm512_mask_blend_epi16:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    pushl %ebp
; KNL32-NEXT:  .Lcfi3:
; KNL32-NEXT:    .cfi_def_cfa_offset 8
; KNL32-NEXT:  .Lcfi4:
; KNL32-NEXT:    .cfi_offset %ebp, -8
; KNL32-NEXT:    movl %esp, %ebp
; KNL32-NEXT:  .Lcfi5:
; KNL32-NEXT:    .cfi_def_cfa_register %ebp
; KNL32-NEXT:    andl $-32, %esp
; KNL32-NEXT:    subl $32, %esp
; KNL32-NEXT:    vpblendw {{.*#+}} ymm0 = ymm2[0],ymm0[1],ymm2[2],ymm0[3],ymm2[4],ymm0[5],ymm2[6],ymm0[7],ymm2[8],ymm0[9],ymm2[10],ymm0[11],ymm2[12],ymm0[13],ymm2[14],ymm0[15]
; KNL32-NEXT:    vpblendw {{.*#+}} ymm1 = mem[0],ymm1[1],mem[2],ymm1[3],mem[4],ymm1[5],mem[6],ymm1[7],mem[8],ymm1[9],mem[10],ymm1[11],mem[12],ymm1[13],mem[14],ymm1[15]
; KNL32-NEXT:    movl %ebp, %esp
; KNL32-NEXT:    popl %ebp
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <32 x i16> %A, <32 x i16> %W, <32 x i32>  <i32 32, i32 1, i32 34, i32 3, i32 36, i32 5, i32 38, i32 7, i32 40, i32 9, i32 42, i32 11, i32 44, i32 13, i32 46, i32 15, i32 48, i32 17, i32 50, i32 19, i32 52, i32 21, i32 54, i32 23, i32 56, i32 25, i32 58, i32 27, i32 60, i32 29, i32 62, i32 31>
  ret <32 x i16> %0
}

define <16 x i32> @test_mm512_mask_blend_epi32(<16 x i32> %A, <16 x i32> %W){
; SKX64-LABEL: test_mm512_mask_blend_epi32:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX64-NEXT:    kmovw %eax, %k1
; SKX64-NEXT:    vpblendmd %zmm0, %zmm1, %zmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm512_mask_blend_epi32:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vpblendmd %zmm0, %zmm1, %zmm0 {%k1}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm512_mask_blend_epi32:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX32-NEXT:    kmovw %eax, %k1
; SKX32-NEXT:    vpblendmd %zmm0, %zmm1, %zmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm512_mask_blend_epi32:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vpblendmd %zmm0, %zmm1, %zmm0 {%k1}
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <16 x i32> %A, <16 x i32> %W, <16 x i32>  <i32 16, i32 1, i32 18, i32 3, i32 20, i32 5, i32 22, i32 7, i32 24, i32 9, i32 26, i32 11, i32 28, i32 13, i32 30, i32 15>
  ret <16 x i32> %0
}

define <8 x i64> @test_mm512_mask_blend_epi64(<8 x i64> %A, <8 x i64> %W){
; SKX64-LABEL: test_mm512_mask_blend_epi64:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movb $-86, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vpblendmq %zmm0, %zmm1, %zmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm512_mask_blend_epi64:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    movb $-86, %al
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vpblendmq %zmm0, %zmm1, %zmm0 {%k1}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm512_mask_blend_epi64:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movb $-86, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vpblendmq %zmm0, %zmm1, %zmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm512_mask_blend_epi64:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    movb $-86, %al
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vpblendmq %zmm0, %zmm1, %zmm0 {%k1}
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <8 x i64> %A, <8 x i64> %W, <8 x i32>  <i32 8, i32 1, i32 10, i32 3, i32 12, i32 5, i32 14, i32 7>
  ret <8 x i64> %0
}

define <16 x float> @test_mm512_mask_blend_ps(<16 x float> %A, <16 x float> %W){
; SKX64-LABEL: test_mm512_mask_blend_ps:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX64-NEXT:    kmovw %eax, %k1
; SKX64-NEXT:    vblendmps %zmm0, %zmm1, %zmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm512_mask_blend_ps:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vblendmps %zmm0, %zmm1, %zmm0 {%k1}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm512_mask_blend_ps:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX32-NEXT:    kmovw %eax, %k1
; SKX32-NEXT:    vblendmps %zmm0, %zmm1, %zmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm512_mask_blend_ps:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vblendmps %zmm0, %zmm1, %zmm0 {%k1}
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <16 x float> %A, <16 x float> %W, <16 x i32>  <i32 16, i32 1, i32 18, i32 3, i32 20, i32 5, i32 22, i32 7, i32 24, i32 9, i32 26, i32 11, i32 28, i32 13, i32 30, i32 15>
  ret <16 x float> %0
}

define <8 x double> @test_mm512_mask_blend_pd(<8 x double> %A, <8 x double> %W){
; SKX64-LABEL: test_mm512_mask_blend_pd:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movb $-88, %al
; SKX64-NEXT:    kmovb %eax, %k1
; SKX64-NEXT:    vblendmpd %zmm0, %zmm1, %zmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm512_mask_blend_pd:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    movb $-88, %al
; KNL64-NEXT:    kmovw %eax, %k1
; KNL64-NEXT:    vblendmpd %zmm0, %zmm1, %zmm0 {%k1}
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm512_mask_blend_pd:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movb $-88, %al
; SKX32-NEXT:    kmovb %eax, %k1
; SKX32-NEXT:    vblendmpd %zmm0, %zmm1, %zmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm512_mask_blend_pd:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    movb $-88, %al
; KNL32-NEXT:    kmovw %eax, %k1
; KNL32-NEXT:    vblendmpd %zmm0, %zmm1, %zmm0 {%k1}
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <8 x double> %A, <8 x double> %W, <8 x i32>  <i32 8, i32 9, i32 10, i32 3, i32 12, i32 5, i32 14, i32 7>
  ret <8 x double> %0
}


define <32 x i8> @test_mm256_mask_blend_epi8(<32 x i8> %A, <32 x i8> %W){
; SKX64-LABEL: test_mm256_mask_blend_epi8:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movl $-1431655766, %eax # imm = 0xAAAAAAAA
; SKX64-NEXT:    kmovd %eax, %k1
; SKX64-NEXT:    vpblendmb %ymm0, %ymm1, %ymm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm256_mask_blend_epi8:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    vmovdqa {{.*#+}} ymm2 = [255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0]
; KNL64-NEXT:    vpblendvb %ymm2, %ymm1, %ymm0, %ymm0
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm256_mask_blend_epi8:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movl $-1431655766, %eax # imm = 0xAAAAAAAA
; SKX32-NEXT:    kmovd %eax, %k1
; SKX32-NEXT:    vpblendmb %ymm0, %ymm1, %ymm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm256_mask_blend_epi8:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    vmovdqa {{.*#+}} ymm2 = [255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0]
; KNL32-NEXT:    vpblendvb %ymm2, %ymm1, %ymm0, %ymm0
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <32 x i8> %A, <32 x i8> %W, <32 x i32>  <i32 32, i32 1, i32 34, i32 3, i32 36, i32 5, i32 38, i32 7, i32 40, i32 9, i32 42, i32 11, i32 44, i32 13, i32 46, i32 15, i32 48, i32 17, i32 50, i32 19, i32 52, i32 21, i32 54, i32 23, i32 56, i32 25, i32 58, i32 27, i32 60, i32 29, i32 62, i32 31>
  ret <32 x i8> %0
}

define <16 x i8> @test_mm_mask_blend_epi8(<16 x i8> %A, <16 x i8> %W){
; SKX64-LABEL: test_mm_mask_blend_epi8:
; SKX64:       # BB#0: # %entry
; SKX64-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX64-NEXT:    kmovw %eax, %k1
; SKX64-NEXT:    vpblendmb %xmm0, %xmm1, %xmm0 {%k1}
; SKX64-NEXT:    retq
;
; KNL64-LABEL: test_mm_mask_blend_epi8:
; KNL64:       # BB#0: # %entry
; KNL64-NEXT:    vmovdqa {{.*#+}} xmm2 = [255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0]
; KNL64-NEXT:    vpblendvb %xmm2, %xmm1, %xmm0, %xmm0
; KNL64-NEXT:    retq
;
; SKX32-LABEL: test_mm_mask_blend_epi8:
; SKX32:       # BB#0: # %entry
; SKX32-NEXT:    movw $-21846, %ax # imm = 0xAAAA
; SKX32-NEXT:    kmovw %eax, %k1
; SKX32-NEXT:    vpblendmb %xmm0, %xmm1, %xmm0 {%k1}
; SKX32-NEXT:    retl
;
; KNL32-LABEL: test_mm_mask_blend_epi8:
; KNL32:       # BB#0: # %entry
; KNL32-NEXT:    vmovdqa {{.*#+}} xmm2 = [255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0]
; KNL32-NEXT:    vpblendvb %xmm2, %xmm1, %xmm0, %xmm0
; KNL32-NEXT:    retl
entry:
  %0 = shufflevector <16 x i8> %A, <16 x i8> %W, <16 x i32>  <i32 16, i32 1, i32 18, i32 3, i32 20, i32 5, i32 22, i32 7, i32 24, i32 9, i32 26, i32 11, i32 28, i32 13, i32 30, i32 15>
  ret <16 x i8> %0
}

