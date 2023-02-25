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

#include "dosbox.h"
#include "pci_bus.h"

#include "callback.h"
#include "checks.h"
#include "debug.h"
#include "inout.h"
#include "regs.h"
#include "setup.h"
#include "support.h"

CHECK_NARROWING();

#define S3_LFB_BASE 0xE0000000u


static uint32_t pci_caddress          = 0; // current PCI addressing
static uint8_t  pci_devices_installed = 0; // number of registered PCI devices

// PCI configuration data
static uint8_t pci_cfg_data[PCI_MAX_PCIDEVICES][PCI_MAX_PCIFUNCTIONS][256];
// Registered PCI devices
static PCI_Device* pci_devices[PCI_MAX_PCIDEVICES];

// ***************************************************************************
// Helper functions
// ***************************************************************************

uint8_t get_pci_cfg_data(const PCI_Device* dev, const uint8_t reg_num)
{
	const auto id     = dev->PCIId();
	const auto sub_id = dev->PCISubfunction();

	return pci_cfg_data[id][sub_id][reg_num];
}

void set_pci_cfg_data(const PCI_Device* dev, const uint8_t reg_num, const uint8_t value)
{
	const auto id     = dev->PCIId();
	const auto sub_id = dev->PCISubfunction();

	pci_cfg_data[id][sub_id][reg_num] = value;
}

// PCI address
// 31    - set for a PCI access
// 30-24 - 0
// 23-16 - bus number			(0x00ff0000)
// 15-11 - device number (slot)	(0x0000f800)
// 10- 8 - subfunction number	(0x00000700)
//  7- 2 - config register #	(0x000000fc)

static uint32_t read_pci_addr([[maybe_unused]] io_port_t port, io_width_t)
{
	LOG(LOG_PCI, LOG_NORMAL)("Read PCI address -> %x", pci_caddress);
	return pci_caddress;
}

static void write_pci_addr(io_port_t, io_val_t val, io_width_t)
{
	LOG(LOG_PCI, LOG_NORMAL)("Write PCI address :=%x", val);
	pci_caddress = val;
}

// read single 8bit value from register file (special register treatment included)
static uint8_t read_pci_register(PCI_Device* dev, const uint8_t reg_num)
{
	switch (reg_num) {
	case 0x00:
		return static_cast<uint8_t>(dev->VendorID() & 0xff);
	case 0x01:
		return static_cast<uint8_t>((dev->VendorID() >> 8) & 0xff);
	case 0x02:
		return static_cast<uint8_t>(dev->DeviceID() & 0xff);
	case 0x03:
		return static_cast<uint8_t>((dev->DeviceID() >> 8) & 0xff);
	case 0x0e:
		return static_cast<uint8_t>((get_pci_cfg_data(dev, reg_num) & 0x7f) |
		                            ((dev->NumSubdevices() > 0) ? 0x80 : 0x00));
	default:
		break;
	}

	// call device routine for special actions and possibility to discard/remap register
	auto parsed = dev->ParseReadRegister(reg_num);
	if ((parsed >= 0) && (parsed < 256)) {
		return get_pci_cfg_data(dev, static_cast<uint8_t>(parsed));
	}

	uint8_t new_val = 0;
	uint8_t mask    = 0;
	if (dev->OverrideReadRegister(reg_num, &new_val, &mask)) {
		const auto old_val = static_cast<uint8_t>(get_pci_cfg_data(dev, reg_num) & ~mask);
		return static_cast<uint8_t>(old_val | (new_val & mask));
	}

	return 0xff;
}

static void write_pci_register(PCI_Device* dev, const uint8_t reg_num, const uint8_t value)
{
	// vendor/device/class IDs/header type/etc. are read-only
	if ((reg_num < 0x04) || (reg_num == 0x0e) ||
	    ((reg_num >= 0x06) && (reg_num < 0x0c))) {
		return;
	}

	if (!dev) {
		return;
	}

	switch (get_pci_cfg_data(dev, 0x0e) & 0x7f) {
	// Header-type specific handling
	case 0x00:
		if ((reg_num >= 0x28) && (reg_num < 0x30)) {
			// Subsystem information is read-only
			return;
		}
		break;
	case 0x01:
	case 0x02:
	default:
		break;
	}

	// call device routine for special actions and the
	// possibility to discard/replace the value that is to be written
	const auto parsed = dev->ParseWriteRegister(reg_num, value);
	if (parsed >= 0) {
		set_pci_cfg_data(dev, reg_num, static_cast<uint8_t>(parsed & 0xff));
	}
}

struct OpParams {
    uint8_t dev_num = 0;
    uint8_t fct_num = 0;
    uint8_t reg_num = 0;
    PCI_Device *dev = nullptr;
};

static bool get_op_params(OpParams &params, const io_port_t port)
{
	// check for enabled/bus 0
	if ((pci_caddress & 0x80ff0000) != 0x80000000) {
		return false;
	}

	params.dev_num = static_cast<uint8_t>((pci_caddress >> 11) & 0x1f);
	params.fct_num = static_cast<uint8_t>((pci_caddress >> 8)  & 0x7);
	params.reg_num = static_cast<uint8_t>((pci_caddress & 0xfc) + (port & 0x03));

	if (params.dev_num >= pci_devices_installed) {
		return false;
	}

	auto &pci_device = pci_devices[params.dev_num];
	if (!pci_device || (params.fct_num > pci_device->NumSubdevices())) {
		return false;
	}

	params.dev = pci_device->GetSubdevice(params.fct_num);
	return params.dev;
}

static void write_pci(io_port_t port, io_val_t value, io_width_t width)
{
	// write_pci is only ever registered as an 8-bit handler, despite
	// appearing to handle up to 32-bit requests. Let's check that.
	assert(width == io_width_t::byte);

	LOG(LOG_PCI, LOG_NORMAL)("Write PCI data :=%x (io_width=%d)", port, static_cast<int>(width));

	OpParams params;
	if (!get_op_params(params, port)) {
		return;
	}

	LOG(LOG_PCI,LOG_NORMAL)("  Write to device %x register %x (function %x) (:=%x)",
	                        params.dev_num, params.reg_num, params.fct_num, value);

	const auto reg_num_0 = static_cast<uint8_t>(params.reg_num + 0);
	const auto reg_num_1 = static_cast<uint8_t>(params.reg_num + 1);
	const auto reg_num_2 = static_cast<uint8_t>(params.reg_num + 2);
	const auto reg_num_3 = static_cast<uint8_t>(params.reg_num + 3);

	// write data to PCI device/configuration
	switch (width) {
	case io_width_t::byte:
		write_pci_register(params.dev, reg_num_0,
		                   static_cast<uint8_t>(value & 0xff));
		break;
	// currently WORD and DWORD are never used
	case io_width_t::word:
		write_pci_register(params.dev, reg_num_0,
		                   static_cast<uint8_t>(value & 0xff));
		write_pci_register(params.dev, reg_num_1,
		                   static_cast<uint8_t>((value >> 8) & 0xff));
		break;
	case io_width_t::dword:
		write_pci_register(params.dev, reg_num_0,
		                   static_cast<uint8_t>(value & 0xff));
		write_pci_register(params.dev, reg_num_1,
		                   static_cast<uint8_t>((value >> 8) & 0xff));
		write_pci_register(params.dev, reg_num_2,
		                   static_cast<uint8_t>((value >> 16) & 0xff));
		write_pci_register(params.dev, reg_num_3,
		                   static_cast<uint8_t>((value >> 24) & 0xff));
		break;
	}
}

static uint32_t read_pci(io_port_t port, io_width_t width)
{
	// read_pci is only ever registered as an 8-bit handler, despite
	// appearing to handle up to 32-bit requests. Let's check that.
	assert(width == io_width_t::byte);

	LOG(LOG_PCI, LOG_NORMAL)("Read PCI data -> %x", pci_caddress);

	OpParams params;
	if (!get_op_params(params, port)) {
		return UINT32_MAX;
	}

	LOG(LOG_PCI,LOG_NORMAL)("  Read from device %x register %x (function %x); addr %x",
	                        params.dev_num, params.reg_num, params.fct_num, pci_caddress);

	const auto reg_num_0 = static_cast<uint8_t>(params.reg_num + 0);
	const auto reg_num_1 = static_cast<uint8_t>(params.reg_num + 1);
	const auto reg_num_2 = static_cast<uint8_t>(params.reg_num + 2);
	const auto reg_num_3 = static_cast<uint8_t>(params.reg_num + 3);

	switch (width) {
	case io_width_t::byte:
		return read_pci_register(params.dev, reg_num_0);
	// currently WORD and DWORD are never used
	case io_width_t::word:
		return static_cast<uint32_t>((read_pci_register(params.dev, reg_num_0)) |
		                             (read_pci_register(params.dev, reg_num_1) << 8));
	case io_width_t::dword:
		return static_cast<uint32_t>((read_pci_register(params.dev, reg_num_0)) |
		                             (read_pci_register(params.dev, reg_num_1) << 8)  |
		                             (read_pci_register(params.dev, reg_num_2) << 16) |
		                             (read_pci_register(params.dev, reg_num_3) << 24));
	default:
		break;
	}

	return UINT32_MAX;
}

static Bitu PCI_PM_Handler()
{
	LOG_MSG("PCI PMode handler, function %x", reg_ax);
	return CBRET_NONE;
}

PCI_Device::PCI_Device(const uint16_t vendor, const uint16_t device)
{
	pci_id          = -1;
	pci_subfunction = -1;

	vendor_id = vendor;
	device_id = device;

	num_subdevices = 0;
	for (uint8_t dct = 0; dct < PCI_MAX_PCIFUNCTIONS - 1; ++dct) {
		subdevices[dct] = 0;
	}
}

PCI_Device::~PCI_Device()
{
}

void PCI_Device::SetPCIId(const Bitu number, const Bits sub_fct)
{
	if (number >= PCI_MAX_PCIDEVICES) {
		return;
	}

	pci_id = static_cast<Bits>(number);
	if ((sub_fct >= 0) && (sub_fct < PCI_MAX_PCIFUNCTIONS - 1)) {
		pci_subfunction = sub_fct;
	} else {
		pci_subfunction = -1;
	}
}

bool PCI_Device::AddSubdevice(PCI_Device* dev)
{
	if (num_subdevices >= PCI_MAX_PCIFUNCTIONS - 1) {
		return false;
	}

	if (subdevices[num_subdevices]) {
		E_Exit("PCI subdevice slot already in use!");
	}

	subdevices[num_subdevices] = dev;
	++num_subdevices;

	return true;
}

void PCI_Device::RemoveSubdevice(const Bits subfct)
{
	if ((subfct <= 0) || (subfct >= PCI_MAX_PCIFUNCTIONS)) {
		return;
	}

	if (subfct <= this->NumSubdevices()) {
		delete subdevices[subfct - 1];
		subdevices[subfct - 1] = nullptr;
		// should adjust things like num_subdevices as well...
	}
}

PCI_Device* PCI_Device::GetSubdevice(const Bits subfct)
{
	if (subfct>=PCI_MAX_PCIFUNCTIONS) {
		return nullptr;
	}

	if (subfct > 0) {
		if (subfct <= this->NumSubdevices()) {
			return subdevices[subfct - 1];
		}
	} else if (subfct == 0) {
		return this;
	}

	return nullptr;
}

// ***************************************************************************
// PCI bus
// ***************************************************************************

// Queued devices -  PCI device registering requested before the PCI framework
// was initialized

static constexpr size_t max_rqueued_devices = 16;
static uint8_t num_rqueued_devices = 0;
static PCI_Device* rqueued_devices[max_rqueued_devices];

#include "pci_devices.h"

class PCI final : public Module_base {
private:
	bool initialized = false;

protected:
	IO_WriteHandleObject PCI_WriteHandler[5];
	IO_ReadHandleObject  PCI_ReadHandler[5];

	CALLBACK_HandlerObject callback_pci = {};

public:

	PhysPt GetPModeCallbackPointer() {
		return Real2Phys(callback_pci.Get_RealPointer());
	}

	bool IsInitialized() const {
		return initialized;
	}

	// set up port handlers and configuration data
	void InitializePCI();

	// register PCI device to bus and setup data
	Bits RegisterPCIDevice(PCI_Device* device, Bits slot = -1);
	void RemoveDevice(uint16_t vendor_id, uint16_t device_id);

	void Deinitialize();

	PCI(Section* configuration);
	~PCI();

};

PCI::PCI(Section* configuration) : Module_base(configuration)
{
	initialized = false;
	pci_devices_installed = 0;

	for (uint8_t devct = 0; devct < PCI_MAX_PCIDEVICES; devct++) {
		pci_devices[devct] = nullptr;
	}

	if (num_rqueued_devices > 0) {
		// Register all devices that have been added
		// before the PCI bus was instantiated
		for (uint8_t dct = 0; dct < num_rqueued_devices; dct++) {
			this->RegisterPCIDevice(rqueued_devices[dct]);
		}
		num_rqueued_devices = 0;
	}
}

PCI::~PCI()
{
	initialized = false;

	pci_devices_installed = 0;
	num_rqueued_devices   = 0;
}

void PCI::InitializePCI()
{
	// install PCI-addressing ports
	PCI_WriteHandler[0].Install(port_num_pci_address,
	                            write_pci_addr,
	                            io_width_t::dword);
	PCI_ReadHandler[0].Install(port_num_pci_address,
	                           read_pci_addr,
	                           io_width_t::dword);

	// install PCI-register read/write handlers
	for (uint8_t ct = 0; ct < 4; ++ct) {
		const auto port_num = static_cast<io_port_t>(port_num_pci_data + ct);
		PCI_WriteHandler[1 + ct].Install(port_num, write_pci, io_width_t::byte);
		PCI_ReadHandler[1 + ct].Install(port_num, read_pci, io_width_t::byte);
	}

	for (uint8_t dev = 0; dev < PCI_MAX_PCIDEVICES; ++dev) {
		for (uint8_t fct = 0; fct < PCI_MAX_PCIFUNCTIONS - 1; ++fct) {
			for (uint16_t reg = 0; reg < 256; ++reg) {
				pci_cfg_data[dev][fct][reg] = 0;
			}
		}
	}

	callback_pci.Install(&PCI_PM_Handler, CB_IRETD, "PCI PM");
	initialized = true;
}

void PCI::Deinitialize()
{
	initialized = false;

	pci_devices_installed = 0;
	num_rqueued_devices   = 0;
	pci_caddress          = 0;

	for (uint8_t dev = 0; dev < PCI_MAX_PCIDEVICES; ++dev) {
		for (uint8_t fct = 0; fct < PCI_MAX_PCIFUNCTIONS - 1; ++fct) {
			for (uint16_t reg = 0; reg < 256; ++reg) {
				pci_cfg_data[dev][fct][reg] = 0;
			}
		}
	}

	// Uninstall PCI-addressing ports
	PCI_WriteHandler[0].Uninstall();
	PCI_ReadHandler[0].Uninstall();

	// Uninstall PCI-register read/write handlers
	for (uint8_t ct = 0; ct < 4; ++ct) {
		PCI_WriteHandler[1 + ct].Uninstall();
		PCI_ReadHandler[1 + ct].Uninstall();
	}

	callback_pci.Uninstall();
}

Bits PCI::RegisterPCIDevice(PCI_Device* device, Bits slot)
{
	if (!device) {
		return -1;
	}

	if (slot >= 0) {
		// specific slot specified, basic check for validity
		if (slot >= PCI_MAX_PCIDEVICES) {
			return -1;
		}
	} else {
		// auto-add to new slot, check if one is still free
		if (pci_devices_installed >= PCI_MAX_PCIDEVICES) {
			return -1;
		}
	}

	if (!initialized) {
		InitializePCI();
	}

	if (slot < 0) {
		slot = pci_devices_installed; // use next slot
	}

	// main device unless specific already-occupied slot is requested
	Bits subfunction = 0;
	if (pci_devices[slot]) {
		subfunction = pci_devices[slot]->GetNextSubdeviceNumber();
		if (subfunction < 0) {
			E_Exit("Too many PCI subdevices!");
		}
	}

	if (device->InitializeRegisters(pci_cfg_data[slot][subfunction])) {
		device->SetPCIId(static_cast<Bitu>(slot), subfunction);
		if (!pci_devices[slot]) {
			pci_devices[slot] = device;
			++pci_devices_installed;
		} else {
			pci_devices[slot]->AddSubdevice(device);
		}

		return slot;
	}

	return -1;
}

void PCI::RemoveDevice(const uint16_t vendor_id, const uint16_t device_id)
{
	for (uint8_t dct = 0; dct < pci_devices_installed; ++dct) {
		if (!pci_devices[dct]) {
			continue;
		}

		if (pci_devices[dct]->NumSubdevices() > 0) {
			for (uint8_t sct = 1; sct < PCI_MAX_PCIFUNCTIONS; ++sct) {
				PCI_Device* sdev = pci_devices[dct]->GetSubdevice(sct);
				if (!sdev) {
					continue;
				}
				if ((sdev->VendorID() == vendor_id) &&
				    (sdev->DeviceID() == device_id)) {
					pci_devices[dct]->RemoveSubdevice(sct);
				}
			}
		}

		if ((pci_devices[dct]->VendorID() == vendor_id) &&
		    (pci_devices[dct]->DeviceID() == device_id)) {
			delete pci_devices[dct];
			pci_devices[dct] = nullptr;
		}
	}

	// check if all devices have been removed
	bool any_device_left = false;
	for (uint8_t dct = 0; dct < PCI_MAX_PCIDEVICES; ++dct) {
		if (dct >= pci_devices_installed) {
			break;
		}
		if (pci_devices[dct]) {
			any_device_left = true;
			break;
		}
	}
	if (!any_device_left) {
		Deinitialize();
	}

	uint8_t last_active_device = PCI_MAX_PCIDEVICES;
	for (uint8_t dct = 0; dct < PCI_MAX_PCIDEVICES; ++dct) {
		if (pci_devices[dct]) {
			last_active_device = dct;
		}
	}
	if (last_active_device < pci_devices_installed) {
		pci_devices_installed = static_cast<uint8_t>(last_active_device + 1);
	}
}

static PCI* pci_interface = nullptr;

static void RegisterPCIDevice(PCI_Device* device)
{
	if (pci_interface) {
		pci_interface->RegisterPCIDevice(device);
	} else {
		if (num_rqueued_devices < max_rqueued_devices) {
			rqueued_devices[num_rqueued_devices++] = device;
		} else {
			// Too many devices waiting for registration
			assert(false);
		}
	}
}

static void UnregisterPCIDevice(PCI_Device* device)
{
	if (pci_interface) {
		// XXX pci_interface->RemoveDevice(device); */
	} else {
		// Trying to remove device before PCI got initialized
		assert(false);
	}
}

// ***************************************************************************
// SVGA S3 device
// ***************************************************************************

class PCI_VGADevice : public PCI_Device {
private:
	static const uint16_t vendor_id = 0x5333; // S3
	static uint16_t GetDeviceId();
	static uint8_t GetRevision();

public:

	PCI_VGADevice();

	bool InitializeRegisters(uint8_t registers[256]) override;
	Bits ParseReadRegister(const uint8_t reg_num) override;
	bool OverrideReadRegister(const uint8_t reg_num, uint8_t* rval,
	                          uint8_t* rval_mask) override;
	Bits ParseWriteRegister(const uint8_t reg_num,
	                        const uint8_t value) override;
};

PCI_VGADevice::PCI_VGADevice() : PCI_Device(vendor_id, GetDeviceId())
{
	// init (S3 graphics card)
	assert(svgaCard == SVGA_S3);
}

uint16_t PCI_VGADevice::GetDeviceId()
{
	switch (s3Card) {
	case S3_86C928:
		// FIXME: Datasheet does not list PCI info at all.
		// @TC1995 suggests the PCI version return 0xb0 for CR30,
		// so this is probably the same here.
		return 0x88b0;
	case S3_Vision864:
		return 0x88c0; // 0x88c0 or 0x88c1
	case S3_Vision868:
		// S3 didn't list this in their datasheet, but Windows 95
		// INF files listed it anyway
		return 0x8880; // 0x8880 or 0x8881
	case S3_Vision964:
		return 0x88d0; // 0x88d0, 0x88d1, 0x88d2 or 0x88d3
	case S3_Vision968:
		return 0x88f0; // 0x88f0, 0x88f1, 0x88f2 or 0x88f3
	case S3_Trio32:
		return 0x8810; // 0x8810 or 0x8811
	case S3_Trio64:
	case S3_Trio64V:
		return 0x8811; // Trio64 (rev 00h) / Trio64V+ (rev 40h)
	case S3_ViRGE:
		return 0x5631;
	case S3_ViRGEVX:
		return 0x883D;
	case S3_Generic:
		break;
	default:
		assert(false);
		break;
	};

	// Trio64 - DOSBox SVN claims to emulate this, even though SVN
	// is closer to Vision864 functionally
	return 0x8811;
}

uint8_t PCI_VGADevice::GetRevision()
{
	if (s3Card == S3_Trio64V) {
		// Trio64V+ datasheet, page 280, PCI "class code".
		// "Hardwired to 0300004xh" (revision is 40h or more)
		return 0x40;
	}

	// Trio32/Trio64 datasheet, page 242, PCI "class code".
	// "Hardwired to 03000000h"
	return 0x00;
}

bool PCI_VGADevice::InitializeRegisters(uint8_t registers[256])
{
	/* XXX
	registers[0x08] = GetRevision(); // revision ID
	registers[0x09] = 0x00; // interface
	registers[0x0a] = 0x00; // subclass type (vga compatible)
	registers[0x0b] = 0x03; // class type (display controller)
	registers[0x0c] = 0x00; // cache line size
	registers[0x0d] = 0x00; // latency timer
	registers[0x0e] = 0x00; // header type (other)

	registers[0x3c] = 0xff; // no irq

	// reset
	registers[0x04] = 0x23; // command register:
	                        // - vga palette snoop
	                        // - ports enabled
	                        // - memory space enabled
	registers[0x05] = 0x00;
	registers[0x06] = 0x80; // status register (medium timing, fast back-to-back)
	registers[0x07] = 0x02;

	// allow changing mem/io enable and VGA palette snoop
	host_writew(config_writemask + 0x04, 0x0023);

	switch (s3Card) {
	case S3_86C928:
	case S3_Vision864:
	case S3_Vision964:
	case S3_Trio32:
	case S3_Trio64:
		// BAR0: memory resource 8MB aligned [22:0 reserved]
		host_writed(config_writemask + 0x10, 0xff800000);
		break;
	case S3_Vision868:
	case S3_Vision968:
	case S3_Trio64V:
	case S3_ViRGE:
	case S3_ViRGEVX:
		// BAR0: memory resource 64MB aligned [25:0 reserved]
		host_writed(config_writemask + 0x10, 0xfc000000);
		break;
	default:
		// BAR0: memory resource 8MB aligned [22:0 reserved]
		host_writed(config_writemask + 0x10, 0xff800000);
		break;
	};

	const auto val = (static_cast<uint32_t>(S3_LFB_BASE) & 0xfffffff0) | 0x8;
	host_writed(config + 0x10, val);
	*/
}

Bits PCI_VGADevice::ParseReadRegister(const uint8_t reg_num)
{
	// XXX
	return 0;
}

bool PCI_VGADevice::OverrideReadRegister(const uint8_t reg_num,
                                         uint8_t* rval,
	                                     uint8_t* rval_mask)
{
	// XXX
	return false;
}

Bits ParseWriteRegister(const uint8_t reg_num, uint8_t value)
{
	// XXX
	return 0;
}

// ***************************************************************************
// Initialization
// ***************************************************************************

static PCI_Device *dev_SVGAS3 = nullptr;

void PCI_AddSVGAS3_Device()
{
	if (!dev_SVGAS3) {
		dev_SVGAS3 = new PCI_VGADevice();
		assert(dev_SVGAS3);
		if (!dev_SVGAS3) {
			return;
		}

		RegisterPCIDevice(dev_SVGAS3);
	}
}

void PCI_RemoveSVGAS3_Device()
{
	if (dev_SVGAS3) {
		UnregisterPCIDevice(dev_SVGAS3);
		delete dev_SVGAS3;
		dev_SVGAS3 = nullptr;
	}
}

PhysPt PCI_GetPModeInterface()
{
	if (!pci_interface) {
		return 0;
	}

	return pci_interface->GetPModeCallbackPointer();
}

bool PCI_IsInitialized()
{
	if (!pci_interface) {
		return false;
	}

	return pci_interface->IsInitialized();
}

void PCI_ShutDown(Section*)
{
	if (!pci_interface) {
		return;
	}

	delete pci_interface;
	pci_interface = nullptr;
}

void PCI_Init(Section* sec)
{
	assert(!pci_interface);

	pci_interface = new PCI(sec);
	sec->AddDestroyFunction(&PCI_ShutDown, false);
}


// XXX Port changes from (10 files):
//
// int10_video_state.cpp
// int10_char.cpp
// int10_modes.cpp
// memory.cpp
// pci_bus.cpp (partially done)
// vga_cpp
// vga_draw.cpp
// vga_seq.cpp
// vga_xga.cpp
// vga_s3.cpp (partially done)
