/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is header file for Customizable VM of NoirVisor.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/cvm_hvm.h
*/

#include <nvdef.h>
#include <nvbdk.h>

#define noir_cvm_invalid_state_intel_vmx	0
#define noir_cvm_invalid_state_amd_v		1
#define noir_cvm_invalid_state_reserved		2
#define noir_cvm_invalid_state_general		3

// Defining vCPU priority on scheduler.
#define noir_cvm_vcpu_priority_user				0
#define noir_cvm_vcpu_priority_kernel			8

// CPUID Leaves for NoirVisor Customizable VM.
#define ncvm_cpuid_leaf_range_and_vendor_string			0x40000000
#define ncvm_cpuid_vendor_neutral_interface_id			0x40000001

#define ncvm_cpuid_leaf_limit		0x40000001

typedef enum _noir_cvm_intercept_code
{
	cv_invalid_state=0,
	cv_shutdown_condition=1,
	cv_memory_access=2,
	cv_init_signal=3,
	cv_hlt_instruction=4,
	cv_io_instruction=5,
	cv_cpuid_instruction=6,
	cv_rdmsr_instruction=7,
	cv_wrmsr_instruction=8,
	cv_cr_access=9,
	cv_dr_access=10,
	cv_hypercall=11,
	cv_exception=12,
	cv_rescission=13,
	cv_interrupt_window=14,
	// The rest are scheduler-relevant.
	cv_scheduler_exit=0x80000000,
	cv_scheduler_pause=0x80000001
}noir_cvm_intercept_code,*noir_cvm_intercept_code_p;

typedef enum _noir_cvm_register_type
{
	noir_cvm_general_purpose_register,
	noir_cvm_flags_register,
	noir_cvm_instruction_pointer,
	noir_cvm_control_register,
	noir_cvm_cr2_register,
	noir_cvm_debug_register,
	noir_cvm_dr67_register,
	noir_cvm_segment_register,
	noir_cvm_fgseg_register,
	noir_cvm_descriptor_table,
	noir_cvm_ldtr_task_register,
	noir_cvm_syscall_msr_register,
	noir_cvm_sysenter_msr_register,
	noir_cvm_cr8_register,
	noir_cvm_fxstate,
	noir_cvm_xsave_area,
	noir_cvm_xcr0_register,
	noir_cvm_efer_register,
	noir_cvm_pat_register,
	noir_cvm_maximum_register_type
}noir_cvm_register_type,*noir_cvm_register_type_p;

typedef enum _noir_cvm_vcpu_option_type
{
	noir_cvm_guest_vcpu_options,
	noir_cvm_exception_bitmap,
	noir_cvm_vcpu_priority
}noir_cvm_vcpu_option_type,*noir_cvm_vcpu_option_type_p;

typedef union _noir_cvm_invalid_state_context
{
	struct
	{
		u32 reason:30;
		u32 id:2;
	};
	u32 value;
}noir_cvm_invalid_state_context,*noir_cvm_invalid_state_context_p;

typedef struct _noir_cvm_cr_access_context
{
	struct
	{
		u32 cr_number:4;
		u32 gpr_number:4;
		u32 mov_instruction:1;
		u32 write:1;
		u32 reserved0:22;
	};
}noir_cvm_cr_access_context,*noir_cvm_cr_access_context_p;

typedef struct _noir_cvm_dr_access_context
{
	struct
	{
		u32 dr_number:4;
		u32 gpr_number:4;
		u32 write:1;
		u32 reserved:23;
	};
}noir_cvm_dr_access_context,*noir_cvm_dr_access_context_p;

typedef struct _noir_cvm_exception_context
{
	struct
	{
		u32 vector:5;
		u32 ev_valid:1;
		u32 reserved:26;
	};
	u32 error_code;
	// Only available for #PF exception.
	u64 pf_addr;
	u8 fetched_bytes;
	u8 instruction_bytes[15];
}noir_cvm_exception_context,*noir_cvm_exception_context_p;

typedef struct _noir_cvm_io_context
{
	struct
	{
		u16 io_type:1;
		u16 string:1;
		u16 repeat:1;
		u16 operand_size:3;
		u16 address_width:4;
		u16 reserved:6;
	}access;
	u16 port;
	u64 rax;
	u64 rcx;
	u64 rsi;
	u64 rdi;
	segment_register ds;
	segment_register es;
}noir_cvm_io_context,*noir_cvm_io_context_p;

typedef struct _noir_cvm_msr_context
{
	u32 eax;
	u32 edx;
	u32 ecx;
}noir_cvm_msr_context,*noir_cvm_msr_context_p;

typedef struct _noir_cvm_memory_access_context
{
	struct
	{
		u8 read:1;
		u8 write:1;
		u8 execute:1;
		u8 user:1;
		u8 fetched_bytes:4;
	}access;
	u8 instruction_bytes[15];
	u64 gpa;
}noir_cvm_memory_access_context,*noir_cvm_memory_access_context_p;

typedef struct _noir_cvm_cpuid_context
{
	struct
	{
		u32 a;
		u32 c;
	}leaf;
}noir_cvm_cpuid_context,*noir_cvm_cpuid_context_p;

typedef struct _noir_cvm_exit_context
{
	noir_cvm_intercept_code intercept_code;
	union
	{
		noir_cvm_invalid_state_context invalid_state;
		noir_cvm_cr_access_context cr_access;
		noir_cvm_dr_access_context dr_access;
		noir_cvm_exception_context exception;
		noir_cvm_io_context io;
		noir_cvm_msr_context msr;
		noir_cvm_memory_access_context memory_access;
		noir_cvm_cpuid_context cpuid;
	};
	segment_register cs;
	u64 rip;
	u64 rflags;
	struct
	{
		u32 cpl:2;
		u32 pe:1;
		u32 lm:1;
		u32 int_shadow:1;
		u32 instruction_length:4;
		u32 reserved:23;
	}vcpu_state;
}noir_cvm_exit_context,*noir_cvm_exit_context_p;

typedef struct _noir_seg_state
{
	segment_register cs;
	segment_register ds;
	segment_register es;
	segment_register fs;
	segment_register gs;
	segment_register ss;
	segment_register tr;
	segment_register gdtr;
	segment_register idtr;
	segment_register ldtr;
}noir_seg_state,*noir_seg_state_p;

typedef struct _noir_cr_state
{
	u64 cr0;
	u64 cr2;
	u64 cr3;
	u64 cr4;
	u64 cr8;
}noir_cr_state,*noir_cr_state_p;

typedef struct _noir_dr_state
{
	u64 dr0;
	u64 dr1;
	u64 dr2;
	u64 dr3;
	u64 dr6;
	u64 dr7;
}noir_dr_state,*noir_dr_state_p;

typedef struct _noir_msr_state
{
	u64 sysenter_cs;
	u64 sysenter_esp;
	u64 sysenter_eip;
	u64 pat;
	u64 efer;
	u64 star;
	u64 lstar;
	u64 cstar;
	u64 sfmask;
}noir_msr_state,*noir_msr_state_p;

typedef struct _noir_xcr_state
{
	u64 xcr0;
}noir_xcr_state,*noir_xcr_state_p;

#define noir_cvm_vcpu_cancellation	31

typedef union _noir_cvm_vcpu_options
{
	struct
	{
		u32 intercept_cpuid:1;
		u32 intercept_msr:1;
		u32 intercept_interrupt_window:1;
		u32 intercept_exceptions:1;
		u32 intercept_cr3:1;
		u32 intercept_drx:1;
		u32 intercept_pause:1;
		u32 npiep:1;
		u32 reserved:24;
	};
	u32 value;
}noir_cvm_vcpu_options,*noir_cvm_vcpu_options_p;

typedef union _noir_cvm_vcpu_state_cache
{
	struct
	{
		u32 gprvalid:1;		// Includes rsp,rip,rflags.
		u32 cr_valid:1;		// Includes cr0,cr3,cr4.
		u32 cr2valid:1;		// Includes cr2.
		u32 dr_valid:1;		// Includes dr6,dr7.
		u32 sr_valid:1;		// Includes cs,ds,es,ss.
		u32 fg_valid:1;		// Includes fs,gs,kgsbase.
		u32 dt_valid:1;		// Includes idtr,gdtr.
		u32 lt_valid:1;		// Includes tr,ldtr.
		u32 sc_valid:1;		// Includes star,lstar,cstar,sfmask.
		u32 se_valid:1;		// Includes esp,eip,cs for sysenter.
		u32 tp_valid:1;		// Includes cr8.tpr.
		u32 ef_valid:1;		// Includes efer.
		u32 pa_valid:1;		// Includes pat.
		u32 reserved:18;
		// This field indicates whether the state in VMCS/VMCB is
		// updated to the state save area in the vCPU structure.
		u32 synchronized:1;
	};
	u32 value;
}noir_cvm_vcpu_state_cache,*noir_cvm_vcpu_state_cache_p;

typedef struct _noir_cvm_event_injection
{
	union
	{
		struct
		{
			u32 vector:8;
			u32 type:3;
			u32 ec_valid:1;
			u32 reserved:15;
			u32 priority:4;
			u32 valid:1;
		};
		u32 value;
	}attributes;
	u32 error_code;
}noir_cvm_event_injection,*noir_cvm_event_injection_p;

typedef struct _noir_cvm_virtual_cpu
{
	noir_gpr_state gpr;
	noir_seg_state seg;
	noir_cr_state crs;
	noir_dr_state drs;
	noir_msr_state msrs;
	noir_xcr_state xcrs;
	void* xsave_area;
	u64 rflags;
	u64 rip;
	noir_cvm_event_injection injected_event;
	noir_cvm_exit_context exit_context;
	noir_cvm_vcpu_options vcpu_options;
	noir_cvm_vcpu_state_cache state_cache;
	u32 exception_bitmap;
	u32 scheduling_priority;
}noir_cvm_virtual_cpu,*noir_cvm_virtual_cpu_p;

typedef union _noir_cvm_mapping_attributes
{
	struct
	{
		u32 present:1;
		u32 write:1;
		u32 execute:1;
		u32 user:1;
		u32 caching:3;
		u32 psize:2;
		u32 reserved:23;
	};
	u32 value;
}noir_cvm_mapping_attributes,*noir_cvm_mapping_attributes_p;

typedef struct _noir_cvm_address_mapping
{
	u64 gpa;
	u64 hva;
	u32 pages;
	noir_cvm_mapping_attributes attributes;
}noir_cvm_address_mapping,*noir_cvm_address_mapping_p;

typedef struct _noir_cvm_virtual_machine
{
	list_entry active_vm_list;
	u32 pid;
	noir_reslock vcpu_list_lock;
}noir_cvm_virtual_machine,*noir_cvm_virtual_machine_p;

#if defined(_central_hvm)
noir_status nvc_svmc_create_vm(noir_cvm_virtual_machine_p* virtual_machine);
void nvc_svmc_release_vm(noir_cvm_virtual_machine_p vm);
noir_status nvc_svmc_create_vcpu(noir_cvm_virtual_cpu_p* virtual_cpu,noir_cvm_virtual_machine_p virtual_machine,u32 vcpu_id);
void nvc_svmc_release_vcpu(noir_cvm_virtual_cpu_p vcpu);
noir_status nvc_svmc_run_vcpu(noir_cvm_virtual_cpu_p vcpu);
noir_status nvc_svmc_rescind_vcpu(noir_cvm_virtual_cpu_p vcpu);
noir_cvm_virtual_cpu_p nvc_svmc_reference_vcpu(noir_cvm_virtual_machine_p vm,u32 vcpu_id);
noir_status nvc_svmc_set_mapping(noir_cvm_virtual_machine_p vm,noir_cvm_address_mapping_p mapping_info);
u32 nvc_svmc_get_vm_asid(noir_cvm_virtual_machine_p vm);

// Idle VM is to be considered as the List Head.
noir_cvm_virtual_machine noir_idle_vm={0};
noir_reslock noir_vm_list_lock=null;

u32 noir_cvm_exit_context_size=sizeof(noir_cvm_exit_context);

u32 noir_cvm_register_buffer_limit[noir_cvm_maximum_register_type]=
{
	sizeof(noir_gpr_state),		// General-Purpose Register
	sizeof(u64),				// rflags register
	sizeof(u64),				// rip register
	sizeof(u64*)*3,				// CR0,CR3,CR4 register
	sizeof(u64),				// CR2 register
	sizeof(u64)*4,				// DR0,DR1,DR2,DR3 register
	sizeof(u64)*2,				// DR6,DR7 register
	sizeof(segment_register)*4,	// cs,ds,es,ss register
	sizeof(segment_register)*2,	// fs,gs register
	sizeof(segment_register)*2,	// tr, ldtr register
	sizeof(u64)*4,				// syscall MSRs.
	sizeof(u64)*3,				// sysenter MSRs.
	sizeof(u64),				// CR8 register
	sizeof(noir_fx_state),		// x87 FPU and XMM state.
	0,							// Extended State.
	8,							// XCR0 Register
	8,							// EFER Register
	8							// PAT Register
};
#else
extern noir_cvm_virtual_machine noir_idle_vm;
extern noir_reslock noir_vm_list_lock;
#endif