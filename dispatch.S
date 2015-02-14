.global __lwt_dispatch

__lwt_dispatch:

push %ebp				#push base pointer
mov %esp, %ebp			#prepare new frame
mov 0xc(%ebp), %eax 	#grab second arg
mov 0x8(%ebp), %edx 	#grab first arg
push %ebx 				#push ebx
push %edi 				#push ebi
push %esi 				#push esi
mov %esp, 0x8(%eax) 	#store current stack
mov 0x8(%edx), %esp 	#change to new stack
pop %esi 				#pop esi
pop %edi 				#pop edi
pop %ebx 				#pop ebx
pop %ebp				#pop ebp
jmp return_routine
return_routine:
ret
