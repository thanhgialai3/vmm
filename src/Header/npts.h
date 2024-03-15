#pragma once
#include "commons.h"
#include "ARCH/CPUID/Extended Features/fn_identifiers.h"
#include "ARCH/PAGES/PAGES.h"
#include "ARCH/CPUID/Extended Features/fn_processor_capacity.h"
#include "ARCH/VMCB/vmcb.h"

//todo factor in mttrs
//todo gpat

//might wanna put this shit in the shared vpcu thingo
PML4E* plm4es; //aka buffer
PDPE* pdepes;
PDE* pdes;

bool setup_huge() 
{
	constexpr uint64_t pdepe_address_range = 0x40000000; //1GB
	constexpr uint64_t plm4e_address_range = pdepe_address_range * 512; //512GB

	CPUID::fn_processor_capacity proc_cap{};
	proc_cap.load();
	const uint64_t guest_phys_addr_size = proc_cap.address_size_identifiers.guest_physical_address_size;
	
	const uint64_t amount_pdepes = (guest_phys_addr_size + pdepe_address_range - 1) / pdepe_address_range; //round up
	const uint64_t amount_plm4es = (amount_pdepes + 511) / 512; //round up

	uint64_t* buffer = static_cast<uint64_t*>(MmAllocateContiguousMemory((amount_plm4es + amount_pdepes) * 8, { .QuadPart = -1 }));
	if (!buffer)
	{
		print("buffer failed to be allocated\n");
		return false;
	}
	memset(buffer, 0, (amount_plm4es + amount_pdepes) * 8);

	plm4es = reinterpret_cast<PML4E*>(buffer);
	pdepes = reinterpret_cast<PDPE*>(buffer + amount_plm4es);


	for (uint64_t i = 0; i < amount_plm4es; i++) {

		plm4es[i].present = 1;
		plm4es[i].write = 1;
		plm4es[i].usermode = 1;
		plm4es[i].page_pa = MmGetPhysicalAddress(&pdepes[i * 512]).QuadPart >> PAGE_SHIFT;


		for (uint64_t j = 0; j < min(512, amount_pdepes - i * 512); j++)
		{
			pdepes[j].present = 1;
			pdepes[j].huge_page = 1;
			pdepes[j].write = 1;
			pdepes[j].usermode = 1;

			pdepes[j].uhuge_page.page_pa = (j * pdepe_address_range) + (i * plm4e_address_range); //this is wrong (needs to be shifted)
		}
	}

	return true;
} 

bool setup_allusive() 
{
	constexpr uint64_t pdes_address_range = 0x200000; //2MB
	constexpr uint64_t pdepe_address_range = pdes_address_range * 512;
	constexpr uint64_t plm4e_address_range = pdepe_address_range * 512;

	CPUID::fn_processor_capacity proc_cap{};
	proc_cap.load();
	const uint64_t guest_phys_addr_size = proc_cap.address_size_identifiers.guest_physical_address_size;

	const uint64_t amount_pdes = (guest_phys_addr_size + pdes_address_range - 1) / pdes_address_range; //round up
	const uint64_t amount_pdepes = (amount_pdes + 511) / 512; //round up
	const uint64_t amount_plm4es = (amount_pdepes + 511) / 512; //round up

	uint64_t* buffer = static_cast<uint64_t*>(MmAllocateContiguousMemory((amount_plm4es + amount_pdepes + amount_pdes) * 8, { .QuadPart = -1 }));
	if (!buffer)
	{
		print("buffer failed to be allocated\n");
		return false;
	}
	memset(buffer, 0, (amount_plm4es + amount_pdepes + amount_pdes) * 8);

	plm4es = reinterpret_cast<PML4E*>(buffer);
	pdepes = reinterpret_cast<PDPE*>(buffer + amount_plm4es);
	pdes = reinterpret_cast<PDE*>(buffer + amount_plm4es + amount_pdepes);

	for (uint64_t i = 0; i < amount_plm4es; i++) {

		plm4es[i].present = 1;
		plm4es[i].write = 1;
		plm4es[i].usermode = 1;
		plm4es[i].page_pa = MmGetPhysicalAddress(&pdepes[i * 512]).QuadPart >> PAGE_SHIFT;


		for (uint64_t j = 0; j < min(512, amount_pdepes - i * 512); j++)
		{
			pdepes[j].present = 1;
			pdepes[j].write = 1;
			pdepes[j].usermode = 1;
			pdepes[j].page_pa = MmGetPhysicalAddress(&pdes[j * 512]).QuadPart >> PAGE_SHIFT;

			for (uint64_t k = 0; k < min(512, amount_pdes - (j * 512) - (i * 512 * 512)); k++) 
			{
				pdes[k].present = 1;
				pdes[k].write = 1;
				pdes[k].usermode = 1;
				pdes[k].large_page = 1;

				pdes[k].ularge_page.page_pa = (k * pdes_address_range) + (j * pdepe_address_range) + (i * plm4e_address_range); //this is wrong (needs to be shifted)
			}
		}
	}
}

void initnpts() 
{
	//EDX bit 26 as returned by CPUID function 8000_0001h indicates 1 - Gbyte page support.
	//The EAX register as returned by CPUID function 8000_0019h reports the number of 1 - Gbyte L1 TLB entries supported
	// and EBX reports the number of 1 - Gbyte L2 TLB entries.

	//maybe want to check the amount of supported and decide if its worth using hugepages (even if they are allowed)

	CPUID::fn_identifiers ident{};
	ident.load();

	bool huge_page_supported = ident.feature_identifiers_ext.page_1gb;

	bool result{};
	if (huge_page_supported)
	{
		result = setup_huge();
	}
	else 
	{
		result = setup_allusive();
	}

	if (!result) 
	{
		print("failed to setup npts\n");
		deletenpts();
		return;
	}

	//set vmcb here
	for (uint16_t i = 0; i < global.vcpu_count; i++)
	{
		global.vcpus[i].guest_vmcb.control.np_enable = 1;
		global.vcpus[i].guest_vmcb.control.np = MmGetPhysicalAddress(plm4es).QuadPart;
	}
}

void deletenpts() 
{
	if(plm4es)
		MmFreeContiguousMemory(plm4es);
}