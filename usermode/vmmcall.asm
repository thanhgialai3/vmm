.code

__vmmcall proc
	mov r8, [rcx+8]
	mov r9, [rcx+10h]
	mov r10, [rcx+18h]
	mov r11, [rcx+20h]
	mov rcx, [rcx]
	vmmcall
	ret
__vmmcall endp

end