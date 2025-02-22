#include <vmexit/handlers.h>
#include <vmm.h>

bool vmexit_handler(vcpu_t& cpu) {
	__svm_vmload(cpu.host_vmcb_pa);
	// we copy rax and rsp into guest context so we can easily index gpr array and only modify cpu.ctx when modificatios are required cleaner than cpu.guest.state
	cpu.ctx.rax.value = cpu.guest.state.rax;
	cpu.ctx.rsp.value = cpu.guest.state.rsp;

	if (cpu.guest.control.exit_code != vmexit_code::VMMCALL) {
		print("VMEXIT: %X\n", cpu.guest.control.exit_code);
	}

	switch (cpu.guest.control.exit_code) {

	case vmexit_code::VMMCALL:
		hypercall_handler(cpu);
		break;

	case vmexit_code::MSR:
		msr_handler(cpu);
		break;

	case vmexit_code::CPUID:
		cpuid_handler(cpu);
		break;

	case vmexit_code::INVALID:
		print("INVALID GUEST STATE, EXITING...\n");
		cpu.shadow.should_exit = true;
		break;

	case vmexit_code::NPF:
		print("[NPF] %zX\n", cpu.guest.control.exit_info_1.info);
		print("[NPF] %zX\n", cpu.guest.control.exit_info_2.nested_page_fault.faulting_gpa);
		npf_handler(cpu);
		break;

	case vmexit_code::HV: // event injection exception
		print("Failed to inject event\n");
		cpu.guest.control.event_injection.value = 0; // reset to avoid infinite loop incase cpu doesnt clear it
		break;

	// Fallthrough for SVM instructions
	case vmexit_code::VMLOAD:
	case vmexit_code::VMSAVE:
	case vmexit_code::VMRUN:
	case vmexit_code::CLGI:
	case vmexit_code::STGI:
	case vmexit_code::SKINIT:
		svm_handler(cpu);
		break;

	default: // shouldnt happen unless we forgot something
		print("UNHANDLED EXIT CODE: %X || INFO1: %zX | INFO2: %zX\n", cpu.guest.control.exit_code, cpu.guest.control.exit_info_1.info, cpu.guest.control.exit_info_2.info);
		break;
	}

	// copy over the changes into the vmcb so the cpu can load them on next vmrun
	cpu.guest.state.rax = cpu.ctx.rax.value;
	cpu.guest.state.rsp = cpu.ctx.rsp.value;

	if (cpu.shadow.should_exit) { 
		unload_single_cpu(cpu); // devirtualize current vcpu
		return false;
	};

	return true; // continue looping
};