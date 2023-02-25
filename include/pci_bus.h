/*
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_PCI_H
#define DOSBOX_PCI_H

#include "mem.h"

#define PCI_MAX_PCIDEVICES   10
#define PCI_MAX_PCIFUNCTIONS 8

class PCI_Device {
private:
	PCI_Device(const PCI_Device&)            = delete;
	PCI_Device& operator=(const PCI_Device&) = delete;

	uint16_t device_id   = 0;
	uint16_t vendor_id   = 0;
	Bits pci_id          = 0;
	Bits pci_subfunction = 0;

	// subdevices declarations, they will respond to pci functions 1 to 7
	// (main device is attached to function 0)
	Bitu num_subdevices = 0;
	PCI_Device* subdevices[PCI_MAX_PCIFUNCTIONS - 1];

public:
	PCI_Device(const uint16_t vendor, const uint16_t device);

	Bits PCIId() const
	{
		return pci_id;
	}

	Bits PCISubfunction() const
	{
		return pci_subfunction;
	}

	uint16_t VendorID() const
	{
		return vendor_id;
	}

	uint16_t DeviceID() const
	{
		return device_id;
	}

	void SetPCIId(const Bitu number, const Bits subfct);

	bool AddSubdevice(PCI_Device* dev);
	void RemoveSubdevice(const Bits sub_fct);

	PCI_Device* GetSubdevice(const Bits sub_fct);

	uint16_t NumSubdevices() const
	{
		if (num_subdevices > PCI_MAX_PCIFUNCTIONS - 1) {
			return static_cast<uint16_t>(PCI_MAX_PCIFUNCTIONS - 1);
		}
		return static_cast<uint16_t>(num_subdevices);
	}

	Bits GetNextSubdeviceNumber() const
	{
		if (num_subdevices >= PCI_MAX_PCIFUNCTIONS - 1) {
			return -1;
		}
		return static_cast<Bits>(num_subdevices + 1);
	}

	virtual bool InitializeRegisters(uint8_t registers[256]) = 0;
	virtual Bits ParseReadRegister(const uint8_t reg_num)    = 0;
	virtual bool OverrideReadRegister(const uint8_t reg_num, uint8_t* rval,
	                                  uint8_t* rval_mask)    = 0;
	virtual Bits ParseWriteRegister(const uint8_t reg_num,
	                                const uint8_t value)     = 0;
	virtual ~PCI_Device()                                    = 0;
};

bool PCI_IsInitialized();

void PCI_AddSVGAS3_Device();
void PCI_RemoveSVGAS3_Device();

RealPt PCI_GetPModeInterface();

#endif
