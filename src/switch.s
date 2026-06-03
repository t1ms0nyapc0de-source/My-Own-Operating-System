[bits 32]
section .text

global switch_task
extern tss_set_kernel_stack

switch_task:
    ; Save caller-preserved registers onto the stack of the current task
    push ebp
    push ebx
    push esi
    push edi

    ; Get arguments
    ; Stack layout:
    ;   [esp + 24] -> next task pointer
    ;   [esp + 20] -> current task pointer
    ;   [esp + 16] -> return address
    ;   [esp + 12] -> saved ebp
    ;   [esp + 8]  -> saved ebx
    ;   [esp + 4]  -> saved esi
    ;   [esp + 0]  -> saved edi
    mov eax, [esp + 20]     ; eax = current
    mov edx, [esp + 24]     ; edx = next

    ; Save the current stack pointer
    ; task_t->esp is at offset 4
    mov [eax + 4], esp

    ; Load the stack pointer of the next task
    mov esp, [edx + 4]

    ; Switch CR3 (Page Directory) if they differ
    ; task_t->page_directory is at offset 12
    mov ecx, [edx + 12]
    mov ebx, cr3
    cmp ecx, ebx
    je .no_paging_switch
    mov cr3, ecx
.no_paging_switch:

    ; Update the TSS kernel stack pointer for user-mode switch context
    ; task_t->kstack is at offset 16
    mov ecx, [edx + 16]
    push ecx
    call tss_set_kernel_stack
    add esp, 4

    ; Restore caller-preserved registers from the stack of the next task
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
