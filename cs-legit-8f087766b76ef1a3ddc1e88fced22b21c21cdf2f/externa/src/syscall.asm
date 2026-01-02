; Direct Syscall for NtReadVirtualMemory (x64)
; Bypasses usermode hooks in ntdll.dll
;
; Tested on Windows 10/11 all versions

.code

; Our function signature:
; NTSTATUS DirectSyscall(
;     DWORD syscallNumber,     ; rcx
;     HANDLE ProcessHandle,    ; rdx
;     PVOID BaseAddress,       ; r8
;     PVOID Buffer,            ; r9
;     SIZE_T Size,             ; [rsp+28h]
;     PSIZE_T BytesRead        ; [rsp+30h]
; )
;
; NtReadVirtualMemory syscall expects:
;     eax = syscall number
;     r10 = ProcessHandle (1st arg)
;     rdx = BaseAddress (2nd arg)
;     r8  = Buffer (3rd arg)
;     r9  = Size (4th arg)
;     [rsp+28h] = BytesRead (5th arg)

DirectSyscall PROC
    mov eax, ecx              ; syscall number -> eax
    mov r10, rdx              ; ProcessHandle -> r10
    mov rdx, r8               ; BaseAddress -> rdx
    mov r8, r9                ; Buffer -> r8
    mov r9, [rsp+28h]         ; Size -> r9
    
    ; Move BytesRead to correct stack position for syscall
    mov r11, [rsp+30h]        ; BytesRead (our 6th arg)
    mov [rsp+28h], r11        ; -> syscall's 5th arg position
    
    syscall
    ret
DirectSyscall ENDP

end
