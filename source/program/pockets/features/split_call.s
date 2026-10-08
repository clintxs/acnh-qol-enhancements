// Invoke the original generic Split with a per-call quantity on our own stack.
// x0 = game's pocket controller, w1 = quantity, x2 = original Split function.
// The return label identifies this invocation to the two quantity hooks.
// No global mutable state, heap allocation, or game-owned padding is required.
.section .text.qol_pockets_invoke_split,"ax",%progbits
.global qol_pockets_invoke_split
.hidden qol_pockets_invoke_split
.type qol_pockets_invoke_split,%function
.balign 4
qol_pockets_invoke_split:
    stp x29, x30, [sp, #-32]!
    str w1, [sp, #16]
    blr x2
.global qol_pockets_split_return
.hidden qol_pockets_split_return
qol_pockets_split_return:
    ldp x29, x30, [sp], #32
    ret
.size qol_pockets_invoke_split, .-qol_pockets_invoke_split
.section .note.GNU-stack,"",%progbits
