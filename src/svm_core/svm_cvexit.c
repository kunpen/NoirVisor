/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the Exit Handler of CVM in SVM-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_cvexit.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include "svm_vmcb.h"
#include "svm_npt.h"
#include "svm_exit.h"
#include "svm_def.h"

// Unexpected VM-Exit occured!
void static fastcall nvc_svm_default_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
}

// Expected Intercept Code: -1
void static fastcall nvc_svm_invalid_state_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	void* vmcb=cvcpu->vmcb.virt;
	u64 cr0,cr3,cr4,efer,dr6,dr7;
	svm_segment_access_rights cs_attrib;
	u64 csb,dsb,esb,ssb;
	amd64_event_injection evi;
	// Invalid State in VMCB is detected by processor.
	// Deliver to subverted host.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_invalid_state;
	// We may record additional information to indicate why the state is invalid
	// in order to help hypervisor developer check their vCPU state settings.
	cvcpu->header.exit_context.invalid_state.id=noir_cvm_invalid_state_amd_v;
	cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_unknown_failure;
	// Preparation: Read all registers subject to consistency checks.
	cr0=noir_svm_vmread64(vmcb,guest_cr0);
	cr3=noir_svm_vmread64(vmcb,guest_cr3);
	cr4=noir_svm_vmread64(vmcb,guest_cr4);
	efer=noir_svm_vmread64(vmcb,guest_efer);
	dr6=noir_svm_vmread64(vmcb,guest_dr6);
	dr7=noir_svm_vmread64(vmcb,guest_dr7);
	cs_attrib.value=noir_svm_vmread16(vmcb,guest_cs_attrib);
	csb=noir_svm_vmread64(vmcb,guest_cs_base);
	dsb=noir_svm_vmread64(vmcb,guest_ds_base);
	esb=noir_svm_vmread64(vmcb,guest_es_base);
	ssb=noir_svm_vmread64(vmcb,guest_ss_base);
	evi.value=noir_svm_vmread64(vmcb,event_injection);
	// Examination I: Is CR0.CD reset while CR0.NW is set?
	if(!noir_bt(&cr0,amd64_cr0_cd) && noir_bt(&cr0,amd64_cr0_nw))
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_cr0_cd0_nw1;
	// Examination II: Are upper 32 bits of CR0 zero?
	if(cr0>0xFFFFFFFF)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_cr0_high;
	// Examination III: Are there MBZ bits set in CR3?
	if(cr3>>52)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_cr3_mbz;
	// Examination IV: Are there MBZ bits set in CR4?
	if(cr4 & 0xFFFFFFFFFF08F000)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_cr4_mbz;
	// Examination V: Are upper 32 bits of DR6 zero?
	if(dr6>0xFFFFFFFF)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_dr6_high;
	// Examination VI: Are upper 32 bits of DR7 zero?
	if(dr7>0xFFFFFFFF)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_dr7_high;
	// Examination VII: Are there MBZ bits set in EFER?
	if(efer & 0xFFFFFFFFFFF90200)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_efer_mbz;
	// Examination VIII:  Are EFER.LME and CR0.PG both set while CR4.PAE is reset?
	if(noir_bt(&efer,amd64_efer_lme) && noir_bt(&cr0,amd64_cr0_pg) && !noir_bt(&cr4,amd64_cr4_pae))
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_efer_lme1_cr0_pg1_cr4_pae0;
	// Examination IX: Are EFER.LME and CR0.PG both set while CR0.PE is reset?
	if(noir_bt(&efer,amd64_efer_lme) && noir_bt(&cr0,amd64_cr0_pg) && !noir_bt(&cr0,amd64_cr0_pe))
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_efer_lme1_cr0_pg1_pe0;
	// Examination X: Are EFER.LME, CR0.PG, CR4.PAE, CS.L, and CS.D all set?
	if(noir_bt(&efer,amd64_efer_lme) && noir_bt(&cr0,amd64_cr0_pg) && noir_bt(&cr4,amd64_cr4_pae) && cs_attrib.long_mode && cs_attrib.default_size)
		cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_efer_lme1_cr0_pg1_cr4_pae1_cs_l1_d1;
	// Examination XI: Is the event being injected illegal?
	if(evi.valid)
	{
		// Is the vector of the exception is actually not an exception?
		if(evi.type==amd64_fault_trap_exception && (evi.type>=0x20 || evi.type==amd64_nmi_interrupt))
			cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_illegal_event_injection;
		// Is type of event reserved?
		if(evi.type==1 || evi.type>4)cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_illegal_event_injection;
	}
	// Examination XII: Is the segment base canonical?
	if(csb>0x7fffffffffff && csb<0xffff800000000000)cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_incanonical_segment_base;
	if(dsb>0x7fffffffffff && dsb<0xffff800000000000)cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_incanonical_segment_base;
	if(esb>0x7fffffffffff && esb<0xffff800000000000)cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_incanonical_segment_base;
	if(ssb>0x7fffffffffff && ssb<0xffff800000000000)cvcpu->header.exit_context.invalid_state.reason=noir_svm_failure_incanonical_segment_base;
}

// Expected Intercept Code: 0x0~0x1F
void static fastcall nvc_svm_cr4_read_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Access to CR4 register is intercepted.
	nvc_svm_cr_access_exit_info info;
	info.value=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info1);
	// Process the CR4.MCE shadowing.
	ulong_ptr *gpr_array=(ulong_ptr*)gpr_state;
	gpr_array[info.gpr]=noir_svm_vmread(cvcpu->vmcb.virt,guest_cr4);
	if(!cvcpu->shadowed_bits.mce)noir_btr((u32*)&gpr_array[info.gpr],amd64_cr4_mce);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

void static fastcall nvc_svm_cr4_write_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Access to CR4 register is intercepted.
	nvc_svm_cr_access_exit_info info;
	info.value=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info1);
	// Process the CR4.MCE shadowing.
	ulong_ptr *gpr_array=(ulong_ptr*)gpr_state;
	cvcpu->shadowed_bits.mce=noir_bt((u32*)&gpr_array[info.gpr],amd64_cr4_mce);
	noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_cr4,gpr_array[info.gpr]|amd64_cr4_mce_bit);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

void static fastcall nvc_svm_cr_access_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Access to Control Registers is intercepted.
	// We don't have to determine whether Interception is subject to be delivered to subverted host,
	// in that accesses to control registers are only intercepted if the subverted host specifies so.
	// Switch to subverted host in order to handle this instruction.
	nvc_svm_cr_access_exit_info info;
	u32 code=noir_svm_vmread32(cvcpu->vmcb.virt,exit_code);
	info.value=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info1);
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	// Save the Exit Context.
	cvcpu->header.exit_context.intercept_code=cv_cr_access;
	cvcpu->header.exit_context.cr_access.cr_number=code&0xf;
	cvcpu->header.exit_context.cr_access.gpr_number=(u32)info.gpr;
	cvcpu->header.exit_context.cr_access.mov_instruction=(u32)info.mov;
	cvcpu->header.exit_context.cr_access.write=noir_bt(&code,4);
}

// Expected Intercept Code: 0x20~0x3F
void static fastcall nvc_svm_dr_access_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Access to Debug Registers is intercepted.
	// We don't have to determine whether Interception is subject to be delivered to subverted host,
	// in that accesses to debug registers are only intercepted if the subverted host specifies so.
	// Switch to subverted host in order to handle this instruction.
	nvc_svm_dr_access_exit_info info;
	u32 code=noir_svm_vmread32(cvcpu->vmcb.virt,exit_code);
	info.value=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info1);
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	// Save the Exit Context.
	cvcpu->header.exit_context.intercept_code=cv_dr_access;
	cvcpu->header.exit_context.dr_access.dr_number=code&0xf;
	cvcpu->header.exit_context.dr_access.gpr_number=(u32)info.gpr;
	cvcpu->header.exit_context.dr_access.write=noir_bt(&code,4);
}

// Expected Intercept Code: 0x40~0x5F, except 0x52 and 0x5E.
void static fastcall nvc_svm_exception_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Exception is intercepted.
	// We don't have to determine whether Exception is subject to be delivered to subverted
	// host, in that Exceptions are intercepted only if the subverted host specifies so.
	// Switch to subverted host in order to handle this instruction.
	amd64_event_injection int_info;
	u64 code=noir_svm_vmread32(cvcpu->vmcb.virt,exit_code);
	u64 err_code=noir_svm_vmread32(cvcpu->vmcb.virt,exit_info1);
	u64 vector=code&0x1f;
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	int_info.value=noir_svm_vmread64(cvcpu->vmcb.virt,exit_interrupt_info);
	// Deliver Exception Context...
	cvcpu->header.exit_context.intercept_code=cv_exception;
	cvcpu->header.exit_context.exception.vector=(u32)vector;
	cvcpu->header.exit_context.exception.ev_valid=(u32)int_info.error_valid;
	cvcpu->header.exit_context.exception.error_code=(u32)err_code;
	// Page-Fault has more info to save.
	if(vector==amd64_page_fault)
	{
		cvcpu->header.exit_context.exception.pf_addr=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info2);
		cvcpu->header.exit_context.exception.fetched_bytes=noir_svm_vmread8(cvcpu->vmcb.virt,number_of_bytes_fetched);
		noir_movsb(cvcpu->header.exit_context.exception.instruction_bytes,(u8*)((ulong_ptr)cvcpu->vmcb.virt+guest_instruction_bytes),15);
	}
}

// Expected Intercept Code: 0x52
void static fastcall nvc_svm_mc_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Machine-Check is intercepted.
	// CVM User is not supposed to intercept an #MC exception.
	// Transfer the #MC exception to the host.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	// Treat #MC as a scheduler's exit.
	cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
	// Control-flow could come back if this #MC is correctable.
}

// Expected Intercept Code: 0x5E
void static fastcall nvc_svm_sx_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Security Exception is intercepted.
	// We have to check if #SX is induced by an arrived INIT signal.
	void* vmcb=cvcpu->vmcb.virt;
	u32 error_code=noir_svm_vmread32(vmcb,exit_info1);
	switch(error_code)
	{
		case amd64_sx_init_redirection:
		{
			// This is #SX is induced by an arrived INIT signal conversion.
			nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
			cvcpu->header.exit_context.intercept_code=cv_init_signal;
			break;
		}
		default:
		{
			// Unknown #SX reason. Deliver to subverted host.
			nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
			cvcpu->header.exit_context.intercept_code=cv_exception;
			cvcpu->header.exit_context.exception.vector=amd64_security_exception;
			cvcpu->header.exit_context.exception.ev_valid=1;
			cvcpu->header.exit_context.exception.error_code=error_code;
			break;
		}
	}
}

// Expected Intercept Code: 0x60
void static fastcall nvc_svm_extint_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// External Interrupt has arrived.
	// According to AMD-V, external interrupt is held pending until GIF is set.
	// Switching to subverted host should have the interrupt handed in under hypervision.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
}

// Expected Intercept Code: 0x61
void static fastcall nvc_svm_nmi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Non-Maskable Interrupt has arrived. According to AMD-V, NMI is held pending until GIF is set.
	// Switching to the subverted host should have the interrupt handed in under hypervision.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
}

// Expected Intercept Code: 0x62
void static fastcall nvc_svm_smi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// System-Management Interrupt has arrived. According to AMD-V, SMI is held pending until GIF is set.
	// Switching to the subverted host should have the interrupt handed in under hypervision.
	// No Internal SMIs are intercepted because all I/O instructions are intercepted.
	// If the firmware locks the SMM, SMIs would not be intercepted. However, SMI handler could recognize
	// if it is running in guest context and should have adjusted its environment to fit accordingly.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_scheduler_exit;
}

// Expected Intercept Code: 0x72
void static fastcall nvc_svm_cpuid_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Determine whether CPUID-Interception is subject to be delivered to subverted host.
	if(cvcpu->header.vcpu_options.intercept_cpuid)
	{
		// Switch to subverted host in order to handle the cpuid instruction.
		nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
		cvcpu->header.exit_context.intercept_code=cv_cpuid_instruction;
		cvcpu->header.exit_context.cpuid.leaf.a=(u32)cvcpu->header.gpr.rax;
		cvcpu->header.exit_context.cpuid.leaf.c=(u32)cvcpu->header.gpr.rcx;
	}
	else
	{
		// NoirVisor will be handling CVM's CPUID Interception.
		u32 leaf=(u32)gpr_state->rax,subleaf=(u32)gpr_state->rcx;
		u32 leaf_class=noir_cpuid_class(leaf);
		noir_cpuid_general_info info;
		if(leaf_class==hvm_leaf_index)
		{
			// The first two fields are compliant with Microsoft Hypervisor Top-Level Functionality Specification
			// even though NoirVisor CVM is running with different set of Hypervisor functionalities.
			switch(leaf)
			{
				case ncvm_cpuid_leaf_range_and_vendor_string:
				{
					info.eax=ncvm_cpuid_leaf_limit;					// NoirVisor CVM CPUID Leaf Limit.
					noir_movsb(&info.ebx,"NoirVisor ZT",12);		// The Vendor String is "NoirVisor ZT"
					break;
				}
				case ncvm_cpuid_vendor_neutral_interface_id:
				{
					noir_movsb(&info.eax,"Nv#1",4);		// Interface Signature is "Nv#1". Indicate Non-Compliance to MSHV-TLFS.
					info.eax=info.ebx=info.ecx=0;		// Clear the Reserved CPUID fields.
					break;
				}
				default:
				{
					noir_movsd((u32*)&info,0,4);
					break;
				}
			}
		}
		else
		{
			noir_cpuid(leaf,subleaf,&info.eax,&info.ebx,&info.ecx,&info.edx);
			switch(leaf)
			{
				case amd64_cpuid_std_proc_feature:
				{
					// Indicate hypervisor presence.
					noir_bts(&info.ecx,amd64_cpuid_hv_presence);
					// In addition to indicating hypervisor presence,
					// the Local APIC ID must be emulated as well.
					break;
				}
				case amd64_cpuid_ext_proc_feature:
				{
					noir_btr(&info.ecx,amd64_cpuid_svm);
					break;
				}
				case amd64_cpuid_ext_svm_features:
				{
					noir_stosd((u32*)&info,0,4);
					break;
				}
				case amd64_cpuid_ext_mem_crypting:
				{
					noir_stosd((u32*)&info,0,4);
					break;
				}
			}
		}
		*(u32*)&gpr_state->rax=info.eax;
		*(u32*)&gpr_state->rbx=info.ebx;
		*(u32*)&gpr_state->rcx=info.ecx;
		*(u32*)&gpr_state->rdx=info.edx;
		noir_svm_advance_rip(cvcpu->vmcb.virt);
	}
	// DO NOT advance the rip unless cpuid is handled by NoirVisor.
}

// Expected Intercept Code: 0x76
void static fastcall nvc_svm_invd_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// The invd instruction would corrupt cache globally host and it thereby must be intercepted.
	// Execute wbinvd to protect global cache.
	noir_wbinvd();
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x78
void static fastcall nvc_svm_hlt_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// The hlt instruction halts the execution of processor.
	// In this regard, schedule the host to the processor.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_hlt_instruction;
}

// Expected Intercept Code: 0x7A
void static fastcall nvc_svm_invlpga_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x7B
void static fastcall nvc_svm_io_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	nvc_svm_io_exit_info info;
	// Deliver the I/O interception to subverted host.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_io_instruction;
	info.value=noir_svm_vmread32(cvcpu->vmcb.virt,exit_info1);
	cvcpu->header.exit_context.io.access.io_type=(u16)info.type;
	cvcpu->header.exit_context.io.access.string=(u16)info.string;
	cvcpu->header.exit_context.io.access.repeat=(u16)info.repeat;
	cvcpu->header.exit_context.io.access.operand_size=(u16)info.op_size;
	cvcpu->header.exit_context.io.access.address_width=(u16)info.addr_size<<1;
	cvcpu->header.exit_context.io.port=(u16)info.port;
	cvcpu->header.exit_context.io.rax=cvcpu->header.gpr.rax;
	cvcpu->header.exit_context.io.rcx=cvcpu->header.gpr.rcx;
	cvcpu->header.exit_context.io.rsi=cvcpu->header.gpr.rsi;
	cvcpu->header.exit_context.io.rdi=cvcpu->header.gpr.rdi;
	cvcpu->header.exit_context.io.ds.selector=noir_svm_vmread16(cvcpu->vmcb.virt,guest_ds_selector);
	cvcpu->header.exit_context.io.ds.attrib=svm_attrib_inverse(noir_svm_vmread16(cvcpu->vmcb.virt,guest_ds_attrib));
	cvcpu->header.exit_context.io.ds.limit=noir_svm_vmread32(cvcpu->vmcb.virt,guest_ds_limit);
	cvcpu->header.exit_context.io.ds.base=noir_svm_vmread64(cvcpu->vmcb.virt,guest_ds_base);
	cvcpu->header.exit_context.io.es.selector=noir_svm_vmread16(cvcpu->vmcb.virt,guest_es_selector);
	cvcpu->header.exit_context.io.es.attrib=svm_attrib_inverse(noir_svm_vmread16(cvcpu->vmcb.virt,guest_es_attrib));
	cvcpu->header.exit_context.io.es.limit=noir_svm_vmread32(cvcpu->vmcb.virt,guest_es_limit);
	cvcpu->header.exit_context.io.es.base=noir_svm_vmread64(cvcpu->vmcb.virt,guest_es_base);
}

// Expected Intercept Code: 0x7C
bool static fastcall nvc_svm_rdmsr_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_custom_vcpu_p cvcpu)
{
	u32 index=(u32)gpr_state->rcx;
	large_integer val;
	bool advance=true;
	switch(index)
	{
		case amd64_efer:
		{
			// Perform SVME shadowing...
			val.value=noir_svm_vmread64(cvcpu->vmcb.virt,guest_efer);
			if(!cvcpu->shadowed_bits.svme)noir_btr(&val.low,amd64_efer_svme);
			break;
		}
		default:
		{
			advance=false;
			break;
		}
	}
	if(advance)
	{
		*(u32*)&gpr_state->rax=val.low;
		*(u32*)&gpr_state->rdx=val.high;
	}
	return advance;
}

bool static fastcall nvc_svm_wrmsr_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_custom_vcpu_p cvcpu)
{
	u32 index=(u32)gpr_state->rcx;
	large_integer val;
	bool advance=true;
	val.low=(u32)gpr_state->rax;
	val.high=(u32)gpr_state->rdx;
	switch(index)
	{
		case amd64_efer:
		{
			// Perform SVME shadowing...
			cvcpu->shadowed_bits.svme=noir_bt(&val.low,amd64_efer_svme);
			noir_svm_vmwrite64(cvcpu->vmcb.virt,guest_efer,val.value|amd64_efer_svme_bit);
			break;
		}
		default:
		{
			advance=false;
			break;
		}
	}
	return advance;
}

void static fastcall nvc_svm_msr_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// Determine whether MSR-Interception is subject to be delivered to subverted host.
	bool op_write=noir_svm_vmread8(cvcpu->vmcb.virt,exit_info1);
	if(cvcpu->header.vcpu_options.intercept_msr)
	{
		// Switch to subverted host in order to handle the MSR instruction.
		nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
		cvcpu->header.exit_context.intercept_code=op_write?cv_wrmsr_instruction:cv_rdmsr_instruction;
		cvcpu->header.exit_context.msr.eax=(u32)cvcpu->header.gpr.rax;
		cvcpu->header.exit_context.msr.edx=(u32)cvcpu->header.gpr.rdx;
		cvcpu->header.exit_context.msr.ecx=(u32)cvcpu->header.gpr.rcx;
	}
	else
	{
		// NoirVisor will be handling CVM's MSR Interception.
		bool advance=op_write?nvc_svm_wrmsr_cvexit_handler(gpr_state,cvcpu):nvc_svm_rdmsr_cvexit_handler(gpr_state,cvcpu);
		if(advance)
			noir_svm_advance_rip(cvcpu);
		else
		{
			// If rip is not to be advance, this means #GP exception is subject to be injected.
			if(!cvcpu->header.vcpu_options.intercept_exceptions)
				noir_svm_inject_event(cvcpu->vmcb.virt,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
			else
			{
				// If user hypervisor specifies interception of exceptions, pass to the user hypervisor.
				nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
				cvcpu->header.exit_context.intercept_code=cv_exception;
				cvcpu->header.exit_context.exception.vector=amd64_general_protection;
				cvcpu->header.exit_context.exception.ev_valid=true;
				cvcpu->header.exit_context.exception.reserved=0;
				cvcpu->header.exit_context.exception.error_code=0;
			}
		}
	}
}

// Expected Intercept Code: 0x7F
void static fastcall nvc_svm_shutdown_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// The shutdown condition occured. Deliver to the subverted host.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_shutdown_condition;
}

// Expected Intercept Code: 0x80
void static fastcall nvc_svm_vmrun_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x81
void static fastcall nvc_svm_vmmcall_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// The Guest invoked a hypercall. Deliver to the subverted host.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_hypercall;
}

// Expected Intercept Code: 0x82
void static fastcall nvc_svm_vmload_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x83
void static fastcall nvc_svm_vmsave_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x84
void static fastcall nvc_svm_stgi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x85
void static fastcall nvc_svm_clgi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x86
void static fastcall nvc_svm_skinit_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	// NoirVisor currently does not support Nested Virtualization. Inject a #UD.
	noir_svm_inject_event(cvcpu->vmcb.virt,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	noir_svm_advance_rip(cvcpu->vmcb.virt);
}

// Expected Intercept Code: 0x400
void static fastcall nvc_svm_nested_pf_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	amd64_npt_fault_code fault;
	// #NPF occured, tell the subverted host there is a memory access fault.
	nvc_svm_switch_to_host_vcpu(gpr_state,vcpu);
	cvcpu->header.exit_context.intercept_code=cv_memory_access;
	cvcpu->header.exit_context.memory_access.gpa=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info2);
	fault.value=noir_svm_vmread64(cvcpu->vmcb.virt,exit_info1);
	cvcpu->header.exit_context.memory_access.access.read=(u8)fault.present;
	cvcpu->header.exit_context.memory_access.access.write=(u8)fault.write;
	cvcpu->header.exit_context.memory_access.access.execute=(u8)fault.execute;
	cvcpu->header.exit_context.memory_access.access.user=(u8)fault.user;
	cvcpu->header.exit_context.memory_access.access.fetched_bytes=noir_svm_vmread8(cvcpu->vmcb.virt,number_of_bytes_fetched);
	noir_movsb(cvcpu->header.exit_context.memory_access.instruction_bytes,(u8*)((ulong_ptr)cvcpu->vmcb.virt+guest_instruction_bytes),15);
}

// Expected Intercept Code: 0x401
void static fastcall nvc_svm_incomplete_ipi_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	;
}

// Expected Intercept Code: 0x402
void static fastcall nvc_svm_unaccelerated_avic_cvexit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu,noir_svm_custom_vcpu_p cvcpu)
{
	;
}