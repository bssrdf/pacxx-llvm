// RUN: llvm-mc -filetype=obj %s -o - \
// RUN: | llvm-readobj -s -t | FileCheck %s

        .section .text,    "0x806", %progbits, unique, 0
        .section .comment, "0x21"


// CHECK:      Section {
// CHECK:        Name: .text (1)
// CHECK-NEXT:   Type: SHT_PROGBITS (0x1)
// CHECK-NEXT:   Flags [ (0x6)
// CHECK-NEXT:     SHF_ALLOC (0x2)
// CHECK-NEXT:     SHF_EXECINSTR (0x4)
// CHECK-NEXT:   ]
// CHECK:        Size: 0
// CHECK:      }

// CHECK:      Section {
// CHECK:        Name: .text (1)
// CHECK-NEXT:   Type: SHT_PROGBITS (0x1)
// CHECK-NEXT:   Flags [ (0x806)
// CHECK-NEXT:     SHF_ALLOC (0x2)
// CHECK-NEXT:     SHF_COMPRESSED (0x800)
// CHECK-NEXT:     SHF_EXECINSTR (0x4)
// CHECK-NEXT:   ]
// CHECK:        Size: 0
// CHECK:      }

// CHECK:      Section {
// CHECK:        Name: .comment (7)
// CHECK-NEXT:   Type: SHT_PROGBITS (0x1)
// CHECK-NEXT:   Flags [ (0x21)
// CHECK-NEXT:     SHF_STRINGS (0x20)
// CHECK-NEXT:     SHF_WRITE (0x1)
// CHECK-NEXT:   ]
// CHECK:        Size: 0
// CHECK:      }
