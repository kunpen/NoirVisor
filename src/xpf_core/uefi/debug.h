/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file builds the debug agent for NoirVisor on UEFI.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/debug.h
*/

#include <Uefi.h>
#include <Protocol/DebugSupport.h>
#include <Protocol/DebugPort.h>

#if defined(MDE_CPU_X64) || defined(MDE_CPU_IA32)
CHAR16* ExceptionNames[0x20]=
{
	L"Divide Error Trap (#DE)",					// Vector 0
	L"Debug Trap/Fault (#DB)",					// Vector 1
	L"Non-Maskable Interrupt (NMI)",			// Vector 2
	L"Breakpoint Trap (#BP)",					// Vector 3
	L"Overflow Trap (#OF)",						// Vector 4
	L"Bound-Range Fault (#BR)",					// Vector 5
	L"Invalid Opcode Fault (#UD)",				// Vector 6
	L"Device Not Available Fault (#NM)",		// Vector 7
	L"Double Fault Abort (#DF)",				// Vector 8
	L"Coprocessor Segment Overrun Exception",	// Vector 9
	L"Invalid TSS Fault (#TS)",					// Vector 10
	L"Segment Not Present Fault (#NP)",			// Vector 11
	L"Stack Fault (#SS)",						// Vector 12
	L"General Protection Fault (#GP)",			// Vector 13
	L"Page Fault (#PF)",						// Vector 14
	L"Reserved (Vector 15)",					// Vector 15
	L"x87 Floating-Point Fault (#MF)",			// Vector 16
	L"Alignment-Check Fault (#AC)",				// Vector 17
	L"Machine-Check Abort (#MC)",				// Vector 18
	L"SIMD Floating-Point Fault (#XF)",			// Vector 19
	L"Virtualization Fault (#VE)",				// Vector 20
	L"Control-flow Protection Fault (#CP)",		// Vector 21
	L"Reserved (Vector 22)",					// Vector 22
	L"Reserved (Vector 23)",					// Vector 23
	L"Reserved (Vector 24)",					// Vector 24
	L"Reserved (Vector 25)",					// Vector 25
	L"Reserved (Vector 26)",					// Vector 26
	L"Reserved (Vector 27)",					// Vector 27
	L"Hypervisor Injection (#HV)",				// Vector 28
	L"VMM Communication Fault (#VC)",			// Vector 29
	L"Security Fault (#SX)",					// Vector 30
	L"Reserved (Vector 31)",					// Vector 31
};
#endif

EFI_DEBUG_SUPPORT_PROTOCOL* DebugSupport=NULL;
EFI_DEBUGPORT_PROTOCOL* DebugPort=NULL;
EFI_GUID gEfiDebugSupportProtocolGuid=EFI_DEBUG_SUPPORT_PROTOCOL_GUID;
EFI_GUID gEfiDebugPortProtocolGuid=EFI_DEBUGPORT_PROTOCOL_GUID;

extern EFI_BOOT_SERVICES *gBS;
extern EFI_SYSTEM_TABLE *gST;
extern EFI_SIMPLE_TEXT_INPUT_PROTOCOL *StdIn;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdOut;