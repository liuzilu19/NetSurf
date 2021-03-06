#if defined(__aof__)
XOS_Module	*	&2001e

		^	0
fcc		#	4
exclusive_time	#	8
inclusive_time	#	8
sizeof_prof_blk	*	@

		AREA	|ARM$$Code|, CODE

		EXPORT	__cyg_profile_func_enter
		EXPORT	__cyg_profile_func_exit

		;@ Standard C Functions
		IMPORT	atexit
		IMPORT	exit
		IMPORT	malloc
		IMPORT	memset

		;@ In profc.c
		IMPORT	profile_save

		;@ Generated by Linker
		IMPORT	|Image$$RO$$Base|
		IMPORT	|Image$$RO$$Limit|
#else
		.set	XOS_Module, 0x2001e

		.struct 0
fcc:		.skip 4
exclusive_time:	.skip 8
inclusive_time:	.skip 8
sizeof_prof_blk:

		.section	".text"
#endif

;@ Function entry handler
;@
;@ On Entry:
;@		r0 -> function
;@		r1 -> call site
;@
;@ On Exit:
;@		r0-r3 corrupt
#if defined(__aof__)
__cyg_profile_func_enter

		ROUT
#else
		.global	__cyg_profile_func_enter
		.type	__cyg_profile_func_enter, %function
__cyg_profile_func_enter:
#endif
		STMFD	sp!,{r4-r8,lr}
		MOV	r4,r0			;@ preserve function pointer

		LDR	r1,initialised		;@ initialise, if necessary
		TEQ	r1,#0
		BLEQ	profile_init

		MRC	P14,0,a1,C1,C0,0	;@ read CCNT

		ADR	r5,stack_base
		LDMIA	r5,{r5-r8}

		TEQ	r5,r6
#if defined(__aof__)
		BEQ	%FT01			;@ stack empty
#else
		BEQ	1f			;@ stack empty
#endif

		LDR	r1,[r6,#-8]		;@ r1 -> caller's profile blk

		;@ update caller's exclusive time

		LDR	r2,[r1,#exclusive_time]
		LDR	r3,[r1,#exclusive_time + 4]
		CMP	r0,r8			;@ wrapped around?
		MOVLO	r5,#-1
		SUBLO	r5,r5,r8		;@ diff = UINT_MAX - last
		ADDLO	r5,r5,r0		;@ diff += now
		SUBHS	r5,r0,r8		;@ diff = last - now
		ADDS	r2,r2,r5
		ADC	r3,r3,#0
		STR	r2,[r1,#exclusive_time]
		STR	r3,[r1,#exclusive_time + 4]

#if defined(__aof__)
01
#else
1:
#endif
		LDR	r1,ro_base
		LDR	r2,ro_limit
		CMP	r4,r1
		CMPGT	r2,r4
#if defined(__aof__)
		BLE	%FT02 	   	       	;@ address out of bounds
#else
		BLE	2f			;@ address out of bounds
#endif

		SUB	r4,r4,r1		;@ get offset into table
		ADD	r4,r4,r7
		LDR	r1,[r4]
		TEQ	r1,#0			;@ create struct, if necessary
		BLEQ	profile_create

		LDR	r2,[r1,#fcc]		;@ increase call count
		ADD	r2,r2,#1
		STR	r2,[r1,#fcc]

		;@ push onto call stack

		STR	r1,[r6],#4		;@ profile block pointer
		STR	r0,[r6],#4		;@ time called
		STR	r6,stackp

#if defined(__aof__)
02
#else
2:
#endif
  		STR	r0,last_called
		LDMFD	sp!,{r4-r8,pc}


;@ Function exit handler
;@
;@ On Entry:
;@		r0 -> function
;@		r1 -> call site
;@
;@ On Exit:
;@		r0-r3 corrupt
#if defined(__aof__)
__cyg_profile_func_exit

		ROUT
#else
		.global	__cyg_profile_func_exit
		.type	__cyg_profile_func_exit, %function
__cyg_profile_func_exit:
#endif
		;@ if (!initialised || stack empty) return;
		LDR	r1,initialised
		TEQ	r1,#0
		LDRNE	r1,stack_base
		LDRNE	r2,stackp
		TEQNE	r1,r2
		MOVEQ	pc,lr

		STMFD	sp!,{r4-r8,lr}
		MOV	r4,r0

		MRC	P14,0,a1,C1,C0,0	;@ read CCNT
		LDR	r5,ro_base
		LDR	r6,ro_limit
		CMP	r4,r5
		CMPGT	r6,r4
#if defined(__aof__)
		BLE	%FT01			;@ address out of bounds
#else
		BLE	1f			;@ address out of bounds
#endif

		ADR	r5,stack_base
		LDMIA	r5,{r5-r8}
		LDR	r1,[r6,#-4]!		;@ r1 = time function entered
		LDR	r2,[r6,#-4]!		;@ r2 -> profile block
		STR	r6,stackp

		LDMIB	r2,{r3-r6}		;@ load times

		;@ update exclusive
		CMP	r0,r8			;@ wrapped around?
		MOVLO	r7,#-1
		SUBLO	r7,r7,r8		;@ diff = UINT_MAX - last
		ADDLO	r7,r7,r0		;@ diff += now
		SUBHS	r7,r0,r8		;@ diff = last - now
		ADDS	r3,r3,r7
		ADC	r4,r4,#0

		;@ update inclusive
		CMP	r0,r1			;@ wrapped around?
		MOVLO	r7,#-1
		SUBLO	r7,r7,r1		;@ diff = UINT_MAX - last
		ADDLO	r7,r7,r0		;@ diff += now
		SUBHS	r7,r0,r1		;@ diff = last - now
		ADDS	r5,r5,r7
		ADC	r6,r6,#0

		STMIB	r2,{r3-r6}		;@ save back

#if defined(__aof__)
01
#else
1:
#endif

  		STR	r0,last_called		;@ update last called and exit
		LDMFD	sp!,{r4-r8,pc}


;@ Initialise the profiling code
;@ Ensures ProfileMod is loaded and allocates memory for call stack etc.
;@
;@ On Entry:
;@		-
;@
;@ On Exit:
;@		r0-r3 corrupt
#if defined(__aof__)
profile_init

		ROUT
#else
		.type	profile_init, %function
profile_init:
#endif
		STMFD	sp!,{r4,r5,lr}
		MOV	r0,#18
		ADR	r1,profile_mod
		SWI	XOS_Module
#if defined(__aof__)
		BVC	%FT01
#else
		BVC	1f
#endif

		;@try to load the module

		MOV	r0,#1
		ADR	r1,profile_cmd
		SWI	XOS_Module
		MOVVS	a1,#255
		BVS	exit

#if defined(__aof__)
01
#else
1:
#endif
		STR	pc,initialised		;@ mark as initialised

		LDR	a1,ro_base		;@ create table
		LDR	a2,ro_limit
		SUB	a1,a2,a1
		BL	malloc

		TEQ	a1,#0
		MOVEQ	a1,#255			;@ malloc failed
		BEQ	exit

		STR	a1,table_base

		LDR	a2,ro_limit
		LDR	a3,ro_base		;@ zero-init table
		SUB	a3,a2,a3
		MOV	a2,#0
		BL	memset

		MOV	a1,#4096		;@ create call stack
		BL	malloc

		TEQ	a1,#0
		MOVEQ	a1,#255
		BEQ	exit

		STR	a1,stack_base
		STR	a1,stackp

		;@ register exit handler
		ADR	a1,profile_save_shim
		BL	atexit

		LDMFD	sp!,{r4,r5,pc}

#if defined(__aof__)
profile_cmd	=	"<NetSurf$Dir>."
profile_mod	=	"ProfileMod",0
		ALIGN
#else
profile_cmd:	.ascii	"<NetSurf$Dir>."
profile_mod:	.asciz	"ProfileMod"
		.align
#endif


;@ On Entry:
;@    	   	r4 -> table entry
;@ On Exit:
;@    	  	r1 -> block
;@		table entry filled in
#if defined(__aof__)
profile_create

		ROUT
#else
		.type	profile_create, %function
profile_create:
#endif
		STMFD	sp!,{r0,lr}
		MOV	a1,#sizeof_prof_blk
		BL	malloc

		TEQ	a1,#0
		MOVEQ	a1,#255	       	  	;@ malloc failed
		BEQ	exit

		MOV	r1,#0
		STR	r1,[a1,#fcc]
		STR	r1,[a1,#exclusive_time]
		STR	r1,[a1,#exclusive_time + 4]
		STR	r1,[a1,#inclusive_time]
		STR	r1,[a1,#inclusive_time + 4]

		MOV	r1,a1

		STR	a1,[r4]
		LDMFD	sp!,{r0,pc}


;@ Shim for profile_save, filling in arguments as required
;@
;@ On Entry:
;@		-
;@
;@ On Exit:
;@		r0-r3 corrupt - exits through profile_save
#if defined(__aof__)
profile_save_shim

		ROUT
#else
		.type	profile_save_shim, %function
profile_save_shim:
#endif
		LDR	a1,table_base
		LDR	a3,ro_base
		LDR	a2,ro_limit
		SUB	a2,a2,a3
		MOV	a2,a2,LSR#2
		B	profile_save


;@ Data

#if defined(__aof__)
initialised	DCD	0

stack_base	DCD	0

stackp		DCD	0

table_base	DCD	0

last_called	DCD	0

ro_base		DCD	|Image$$RO$$Base|

ro_limit	DCD	|Image$$RO$$Limit|
#else
initialised:	.word	0

stack_base:	.word	0

stackp:		.word	0

table_base:	.word	0

last_called:	.word	0

ro_base:	.word	Image$$RO$$Base

ro_limit:	.word	Image$$RO$$Limit
#endif

#if defined(__aof__)
		END
#endif
