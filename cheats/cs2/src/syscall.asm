; Professional Direct Syscall Stub (Variant 1B - Data Segment)
; Target: NtReadVirtualMemory
; ABI: Microsoft x64
; No stack manipulation, no text patching.

.data
; Global variable to hold the SSN. 
; It will be set by C++ at runtime.
PUBLIC NtReadVirtualMemory_SSN
NtReadVirtualMemory_SSN DWORD 0

.code

; NTSTATUS NtReadVirtualMemory_Direct(
;     HANDLE  ProcessHandle,   ; rcx
;     PVOID   BaseAddress,     ; rdx
;     PVOID   Buffer,          ; r8
;     SIZE_T  Size,            ; r9
;     PSIZE_T BytesRead        ; [rsp+28h]
; )
NtReadVirtualMemory_Direct PROC
    mov     r10, rcx                    ; syscall ABI requirement (rcx -> r10)
    mov     eax, NtReadVirtualMemory_SSN ; Load SSN from .data
    syscall
    ret
NtReadVirtualMemory_Direct ENDP

end
