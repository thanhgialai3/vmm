#pragma once
#include "vmcb_control.h"
#include "vmcb_state_save.h"
#include "../MSRs/msrs.h"
#include "../PAGES/pages.h"

struct vmcb_t {
	// table b-1 (control area)
	vmcb_control control;
	static_assert(sizeof(vmcb_control) == 0x400, "vmcb control is not 0x400");

	// table b-2 (state save area)
	vmcb_state_save save_state;
	static_assert(sizeof(vmcb_state_save) == 0xC00, "vmcb state save is not 0xC00");
};
static_assert(sizeof(vmcb_t) == 0x1000, "vmcb size is not 0x1000");

struct register_t {
	union {
		uint64_t value;
		struct {
			uint32_t low;
			uint32_t high;
		};
	};

	inline bool operator==(uint64_t val) {
		return value == val;
	}
	
	inline bool operator!=(uint64_t val) {
		return value != val;
	}
};

struct stack_frame_t
{
	register_t rax; // rax is in vmcb
	register_t rcx;
	register_t rbx;
	register_t rdx;
	register_t rsi;
	register_t rdi;
	register_t r8;
	register_t r9;
	register_t r10;
	register_t r11;
	register_t r12;
	register_t r13;
	register_t r14;
	register_t r15;
	
	M128A xmm0;
	M128A xmm1;
	M128A xmm2;
	M128A xmm3;
	M128A xmm4;
	M128A xmm5;
	M128A xmm6;
	M128A xmm7;
	M128A xmm8;
	M128A xmm9;
	M128A xmm10;
	M128A xmm11;
	M128A xmm12;
	M128A xmm13;
	M128A xmm14;
	M128A xmm15;
};

struct alignas(0x1000) vcpu_t {
	union {
		uint8_t host_stack[0x6000]; //0x6000 default size of KM stack
		struct {
			uint8_t stack_contents[0x6000 - (sizeof(uint64_t) * 6) - sizeof(stack_frame_t)];
			stack_frame_t guest_stack_frame; 
			uint64_t guest_vmcb_pa; 
			vcpu_t* self;
			uint64_t guest_rip; // used when devirtualizing along with rsp, these are copies just to make it easier to access them thru the vmrun loop
			uint64_t guest_rsp; 
			uint64_t is_virtualized; // 16byte aligned
			uint64_t should_exit;
		};
	};
	vmcb_t host_vmcb;
	vmcb_t guest_vmcb;
	uint8_t host_state_area[0x1000]; //Do not modfiy (depends on chipset), just set phys (page alligned) to VM_HSAVE_PA
};
static_assert(sizeof(vcpu_t) == 0x9000, "vcpu size is not 0x9000");

struct shared {
	vcpu_t* current_vcpu;
	vcpu_t* vcpus;
	uint32_t vcpu_count;
	MSR::msrpm_t* shared_msrpm;
	uint64_t* npt;
};

inline shared global{};