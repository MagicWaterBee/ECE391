
.data					# section declaration

        # Useful offset constants for accessing members of a 
        # struct mp1_blink_struct structure
        LOCATION   = 0    
        ON_CHAR    = 2
        OFF_CHAR   = 3 
        ON_LENGTH  = 4
        OFF_LENGTH = 6
        COUNTDOWN  = 8
        STATUS     = 10
        NEXT       = 12

        STRUCT_SIZE = 16

# Pointer to head of list (initialized to NULL)
mp1_list_head:
        .long   0

.text					# section declaration

# Export the function symbol names

.global mp1_rtc_tasklet
.global mp1_ioctl

# void mp1_poke(void);
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX
mp1_poke:
        
	movl    vmem_base_addr(,1),%edx
	movb    %cl,(%edx,%eax,1)
	ret









# void mp1 rtc tasklet (unsigned long);
# Every time an RTC interrupt is generated, mp1 rtc tasklet will be called. Your tasklet will walk down the
# mp1 list head list, examining each mp1 blink struct structure.
# Change status and call mp1_poke to produce a text-mode video
mp1_rtc_tasklet:
	# save values
	pushl   %ebp
	movl    %esp, %ebp
	pushl   %ebx
	pushl   %edi
	pushl   %esi

	movl	mp1_list_head, %ebx

	# main of tasklet
countdown_check:
	cmpl	$0, %ebx
	je   	end_tasklet

	cmpw	$0, COUNTDOWN(%ebx)
    je		status_check
	addw    $-1, COUNTDOWN(%ebx)
	cmpw    $0, COUNTDOWN(%ebx)  
	je      status_check

	# go find next struct
next_struct:
	movl	NEXT(%ebx), %ebx
	jmp		countdown_check

	# invert current status
status_check:
	movzwl	STATUS(%ebx), %esi
	cmpl	$0, %esi
	je		status_0to1

	# invert the status then go to next struct
status_1to0:
	movw	$0, STATUS(%ebx)
	movzwl	LOCATION(%ebx), %eax
 	shll	$1, %eax
 	movb	OFF_CHAR(%ebx), %cl
 	pushl	%ebx                    
 	call	mp1_poke
 	popl	%ebx                    
 	movw	OFF_LENGTH(%ebx), %si
 	movw	%si, COUNTDOWN(%ebx)    
 	jmp		next_struct

status_0to1:
	movw	$1, STATUS(%ebx)
	movzwl	LOCATION(%ebx), %eax
 	shll	$1, %eax
 	movb	ON_CHAR(%ebx), %cl
 	pushl	%ebx                    
 	call	mp1_poke
 	popl	%ebx                    
 	movw	ON_LENGTH(%ebx), %si
 	movw	%si, COUNTDOWN(%ebx)
 	jmp		next_struct

	# load registers back and return
end_tasklet:
	# load values back
	popl	%esi
    popl	%edi
    popl	%ebx

    leave
	ret









# int mp1_ioctl (unsigned long arg, unsigned long cmd);
# This function serves as a “dispatcher” function. 
# It uses the cmd argument to determine which of the next four functions
# to jump to.
# input: argument cmd
# output: return 0 if works correctly, -1 if invalid
mp1_ioctl:
	# copy cmd to eax then decide
	movl	8(%esp), %eax
	cmpl	$3, %eax
	ja		other_cmd
	cmpl	$0, %eax
	jb		other_cmd
	jmp		*jumptable(, %eax, 4)

	# jump table for cmd in [0,3]
jumptable:
	.long	mp1_ioctl_add, mp1_ioctl_remove, mp1_ioctl_find, mp1_ioctl_sync

	# return -1 for cmd<0 or cmd>3
other_cmd:
	movl	$-1, %eax
	leave
	ret









# int mp1 ioctl add(unsigned long arg);
# The add ioctl takes as its argument a user-level pointer to a mp1 blink struct structure.
# input:	arg which is a pointer
# call mp1_malloc to copy a struct and insert to the linked list of structs.
mp1_ioctl_add:
	pushl	%ebp
    movl 	%esp, %ebp
    pushl	%ebx
    pushl	%esi
    pushl	%edi

	# check invalid pointer
	movl	8(%ebp), %esi
	cmpl	$0, %esi
	je  	invalid_add

	# call mp1_malloc
	pushl	$STRUCT_SIZE
	call	mp1_malloc
	addl	$4, %esp
	cmpl	$0, %eax
	je  	invalid_add
	pushl	%eax
	movl	%eax, %ebx

	# call	mp1_copy_from_user
	pushl	$STRUCT_SIZE
	pushl	%esi
	pushl	%ebx
	call	mp1_copy_from_user
	addl	$12, %esp

	# check returned value
    cmpl	$0, %eax
	jne		invalid_add_free

	# check if the copy overflows
	movzwl  LOCATION(%ebx), %eax
	cmpl	$0, %eax
	jl		invalid_add_free
	cmpl	$1999, %eax				#80*25-1
	jg		invalid_add_free

	# insert the struct
	movw	ON_LENGTH(%ebx), %cx
	movw	%cx, COUNTDOWN(%ebx)	
	movw	$1, STATUS(%ebx)
	movl	mp1_list_head, %ecx
	movl	%ecx,NEXT(%ebx)		
	movl	%ebx, mp1_list_head

	# call mp1_poke
	movzwl	LOCATION(%ebx), %eax
	shll	$1, %eax
	movb	ON_CHAR(%ebx), %cl
	call	mp1_poke

	# pop malloc pointer and return 0
	popl	%edi
	popl	%esi
	popl	%ebx
	xorl	%eax,%eax
	leave
	ret

# pop pointers and return -1
invalid_add_free:
	call	mp1_free
invalid_add:
	popl	%edi
	popl	%esi
	popl	%ebx
	xorl	%eax, %eax
	addl	$-1, %eax
	leave
	ret









# int mp1 ioctl remove(unsigned long arg);
# The remove ioctl takes an integer location as its parameter. Traverse the mp1 list head list, looking for an element
# whose location field matches the argument given to this function. If there is such an element, remove it from the
# linked list and free its memory with a call to mp1 free, and return 0. If there is no element whose location matches,
# return -1.
mp1_ioctl_remove:
	# save registers
	pushl   %ebp
	movl    %esp, %ebp
	pushl   %ebx
	pushl   %esi
	pushl   %edi

	# load arg into edi and check if it is valid
	movl    8(%ebp), %edi
	cmpl    $0, %edi
	jl      invalid_remove
	cmpl    $1999, %edi
	jg      invalid_remove
	movl	mp1_list_head, %edx

	# use a loop to search the struct to remove
search_loop:
	cmpl    $0, %edx
	je      invalid_remove
	cmpw    LOCATION(%edx), %di
	je      find_remove
	movl    NEXT(%edx), %edx
	jmp     search_loop

	# need to check if it is list head here
	# other than list head would be removed 
find_remove:
	movl    %edx, %esi
	movl    mp1_list_head, %edx
	cmpl    %esi, %edx
	jne     remove_loop
	movl    NEXT(%edx), %edx    
	movl    %edx, mp1_list_head
	jmp     remove_end
	
	# loop till arg is found
remove_loop:
	cmpl    $0, NEXT(%edx)
	je      invalid_remove
	cmpl    NEXT(%edx), %esi
	je      act_remove
	movl    NEXT(%edx), %edx
	jmp     remove_loop

	# remove the linked list
act_remove:
	movl    NEXT(%esi), %ecx
	movl    %ecx, NEXT(%edx)

	# return 0 and restore registers
remove_end:
	pushl   %esi
	call    mp1_free
	addl    $4, %esp
	xorl    %eax, %eax
	popl    %edi
	popl    %esi
	popl    %ebx
	leave
	ret

	# return -1 and restore registers
invalid_remove:
	movl    $-1, %eax
	popl    %edi
	popl    %esi
	popl    %ebx
	leave
	ret
	
        








# int mp1 ioctl find(unsigned long arg);
# search the mp1 list head list for an element that matches the location. Then copy the entire 
# element, which is a mp1 blink struct, to the user-level structure pointed to by the parameter 
# that was passed in (use mp1 copy to user).
mp1_ioctl_find:
	pushl	%ebp	
	movl	%esp,%ebp	
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	# check if arg is valid
	movl	8(%ebp), %esi
	cmpl	$0, %esi
	je		unfound

	# load edx with arg
	movzwl	LOCATION(%esi), %edx
	cmpl	$0, %edx
	jl		unfound
	cmpl	$1999, %edx		# same as ioctl_add
	jg		unfound
	movl	mp1_list_head, %edx

	# load the location to eax and check if found
find_loop:
	cmpl	$0, %edx
	je		unfound
	movzwl	LOCATION(%edx), %eax
	cmpl	%eax, %ebx
  	je		found
  
	movl	NEXT(%edx), %edx
	jmp		find_loop

	# loop till 0 and return with -1
unfound:
	movl	$-1,%eax
	popl	%edi
  	popl	%esi
  	popl	%ebx
	leave
	ret

	# found, call the copy function and return 0
found:
  	movl	%edx, %eax
	pushl	$STRUCT_SIZE
	pushl	%edx
	pushl	%esi
	call	mp1_copy_to_user
	addl	$12, %esp
	xorl	%eax, %eax
	popl	%edi
  	popl	%esi
  	popl	%ebx
	leave
	ret









# int mp1 ioctl sync(unsigned long arg);
# The sync ioctl synchronizes two existing locations on the screen. The first integer represents the location of the first
# blinking character, and the second integer represents the location of the second blinking character that will become
# synchronized with the first.
mp1_ioctl_sync:
	# save registers
	pushl	%ebp	
	movl	%esp,%ebp	
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	# check arg if valid and load edi and esi repectively with 1st and 2nd byte
  	movl	8(%ebp), %esi
  	cmpl	$0, %esi			
  	je		sync_fail
	movl	%esi, %edi
	shrl	$16, %edi
	andl	$0x0000FFFF, %esi

	movl    mp1_list_head,%ebx
  	cmpl    $0,%ebx
  	je      sync_fail
	
	# use esi to check 2nd byte
sync_loop_1:
 	cmpw    LOCATION(%ebx), %si
 	je      valid_sync_1
 	cmpl    $0, NEXT(%ebx)
 	je      sync_fail
 	movl    NEXT(%ebx), %ebx
	jmp     sync_loop_1
	
	# check 1st if 2nd is valid
valid_sync_1:
	movl    %ebx,%esi
	movl    mp1_list_head, %ebx
  	cmpl    $0, %ebx
  	je      sync_fail
	
	# check 1st byte
sync_loop_2:
	cmpw    LOCATION(%ebx), %di   
	je      valid_sync_2        
	cmpl    $0,NEXT(%ebx)
	je      sync_fail
	movl    NEXT(%ebx),%ebx
	jmp     sync_loop_2

	# copy the whole struct and output cl based on status
valid_sync_2:
	movl    %ebx, %edi
	xorl	%eax, %eax
	movw	ON_LENGTH(%edi), %ax		
	movw	%ax, ON_LENGTH(%esi)
	movw	OFF_LENGTH(%edi), %ax	
	movw	%ax, OFF_LENGTH(%esi)
	movw	COUNTDOWN(%edi), %ax
	movw	%ax, COUNTDOWN(%esi)
	movw	STATUS(%edi), %ax	
	movw	%ax, STATUS(%esi)
	cmpw	$0, STATUS(%esi)	
	je		sync_off
	movb    ON_CHAR(%esi), %cl
	jmp		sync_success

sync_off:
	movb    OFF_CHAR(%esi), %cl
	jmp		sync_success

	# return -1
sync_fail:
  	movl	$-1, %eax
  	popl	%edi
  	popl	%esi
  	popl	%ebx
  	leave
  	ret

	# return 0
sync_success: 
	movzwl	LOCATION(%esi), %eax
	shll  	$1, %eax
	call  	mp1_poke
	xorl	%eax, %eax
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret









.end
