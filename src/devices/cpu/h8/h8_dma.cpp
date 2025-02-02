#include "emu.h"
#include "h8_dma.h"

DEFINE_DEVICE_TYPE(H8_DMA,         h8_dma_device,         "h8_dma",         "H8 DMA controller")
DEFINE_DEVICE_TYPE(H8_DMA_CHANNEL, h8_dma_channel_device, "h8_dma_channel", "H8 DMA channel")

h8_dma_device::h8_dma_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, H8_DMA, tag, owner, clock),
	dmach0(*this, "0"),
	dmach1(*this, "1")
{
}

void h8_dma_device::device_start()
{
	dmach0->set_id(0<<1);
	dmach1->set_id(1<<1);

	save_item(NAME(dmabcr));
	save_item(NAME(dmawer));
	save_item(NAME(dreq));
}

void h8_dma_device::device_reset()
{
	dmabcr = 0x0000;
	dmawer = 0x00;
	dreq[0] = dreq[1] = false;
}

bool h8_dma_device::trigger_dma(int vector)
{
	// Don't shortcut! Both dmas may be started
	bool start0 = dmach0->start_test(vector);
	bool start1 = dmach1->start_test(vector);

	return start0 || start1;
}

void h8_dma_device::count_last(int id)
{
	if(id & 2)
		dmach1->count_last(id & 1);
	else
		dmach0->count_last(id & 1);
}

void h8_dma_device::count_done(int id)
{
	if(id & 2)
		dmach1->count_done(id & 1);
	else
		dmach0->count_done(id & 1);
}

void h8_dma_device::clear_dte(int id)
{
	dmabcr &= ~(0x0010 << id);
}

void h8_dma_device::set_input(int inputnum, int state)
{
	if(inputnum == H8_INPUT_LINE_DREQ0) {
		if(state == ASSERT_LINE) {
			dmach0->start_test(h8_dma_channel_device::DREQ_LEVEL);
			if(!dreq[0])
				dmach0->start_test(h8_dma_channel_device::DREQ_EDGE);
		}
		dreq[0] = (state == ASSERT_LINE);
	} else if(inputnum == H8_INPUT_LINE_DREQ1) {
		if(state == ASSERT_LINE) {
			dmach1->start_test(h8_dma_channel_device::DREQ_LEVEL);
			if(!dreq[1])
				dmach1->start_test(h8_dma_channel_device::DREQ_EDGE);
		}
		dreq[1] = (state == ASSERT_LINE);
	} else
		logerror("input line %d not supported for h8_dma_device\n", inputnum);
}

uint8_t h8_dma_device::dmawer_r()
{
	logerror("dmawer_r %02x\n", dmawer);
	return dmawer;
}

void h8_dma_device::dmawer_w(uint8_t data)
{
	dmawer = data;
	logerror("dmawer_w %02x\n", data);
}

uint8_t h8_dma_device::dmatcr_r()
{
	logerror("dmatcr_r %02x\n", dmatcr);
	return dmatcr;
}

void h8_dma_device::dmatcr_w(uint8_t data)
{
	dmatcr = data;
	logerror("dmatcr_w %02x\n", data);
}

uint16_t h8_dma_device::dmabcr_r()
{
	logerror("dmabcr_r %04x\n", dmabcr);
	return dmabcr;
}

void h8_dma_device::dmabcr_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&dmabcr);
	logerror("dmabcr_w %04x\n", dmabcr);
	dmach0->set_bcr(dmabcr & 0x4000, dmabcr & 0x1000, dmabcr >>  8, dmabcr >> 4, dmabcr >> 0);
	dmach1->set_bcr(dmabcr & 0x8000, dmabcr & 0x2000, dmabcr >> 10, dmabcr >> 6, dmabcr >> 2);
}



h8_dma_channel_device::h8_dma_channel_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, H8_DMA_CHANNEL, tag, owner, clock),
	dmac(*this, "^"),
	cpu(*this, "^^"),
	intc(*this, finder_base::DUMMY_TAG)
{
}

void h8_dma_channel_device::set_info(int _irq_base, int v0, int v1, int v2, int v3, int v4, int v5, int v6, int v7, int v8, int v9, int va, int vb, int vc, int vd, int ve, int vf)
{
	irq_base = _irq_base;
	activation_vectors[ 0] = v0;
	activation_vectors[ 1] = v1;
	activation_vectors[ 2] = v2;
	activation_vectors[ 3] = v3;
	activation_vectors[ 4] = v4;
	activation_vectors[ 5] = v5;
	activation_vectors[ 6] = v6;
	activation_vectors[ 7] = v7;
	activation_vectors[ 8] = v8;
	activation_vectors[ 9] = v9;
	activation_vectors[10] = va;
	activation_vectors[11] = vb;
	activation_vectors[12] = vc;
	activation_vectors[13] = vd;
	activation_vectors[14] = ve;
	activation_vectors[15] = vf;
	memset(state, 0, sizeof(state));
}

void h8_dma_channel_device::device_start()
{
	save_item(STRUCT_MEMBER(state, source));
	save_item(STRUCT_MEMBER(state, dest));
	save_item(STRUCT_MEMBER(state, incs));
	save_item(STRUCT_MEMBER(state, incd));
	save_item(STRUCT_MEMBER(state, count));
	save_item(STRUCT_MEMBER(state, id));
	save_item(STRUCT_MEMBER(state, autoreq));
	save_item(STRUCT_MEMBER(state, suspended));
	save_item(STRUCT_MEMBER(state, mode_16));
	save_item(NAME(mar));
	save_item(NAME(ioar));
	save_item(NAME(etcr));
	save_item(NAME(dmacr));
	save_item(NAME(dtcr));
	save_item(NAME(dta));
	save_item(NAME(dte));
	save_item(NAME(dtie));
	save_item(NAME(fae));
	save_item(NAME(sae));
}

void h8_dma_channel_device::device_reset()
{
	dmacr = 0x0000;
	mar[0] = mar[1] = 0;
	ioar[0] = ioar[1] = 0;
	etcr[0] = etcr[1] = 0;
	dtcr[0] = dtcr[1] = 0;
	fae = sae = false;
	dta = dte = dtie = 0;
}

void h8_dma_channel_device::set_id(int id)
{
	state[0].id = id;
	state[1].id = id | 1;
}

uint16_t h8_dma_channel_device::marah_r()
{
	logerror("marah_r %06x\n", mar[0]);
	return mar[0] >> 16;
}

void h8_dma_channel_device::marah_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if(ACCESSING_BITS_0_7)
		mar[0] = ((data & 0x00ff) << 16) | (mar[0] & 0xffff);
	logerror("marah_w %06x\n", mar[0]);
}

uint16_t h8_dma_channel_device::maral_r()
{
	logerror("maral_r %06x\n", mar[0]);
	return mar[0];
}

void h8_dma_channel_device::maral_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	mar[0] = (mar[0] & ~mem_mask) | (data & mem_mask);
	logerror("maral_w %06x\n", mar[0]);
}

uint16_t h8_dma_channel_device::ioara_r()
{
	return ioar[0];
}

uint8_t h8_dma_channel_device::ioara8_r()
{
	return ioar[0] & 0x00ff;
}

void h8_dma_channel_device::ioara_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&ioar[0]);
	logerror("ioara_w %04x\n", ioar[0]);
}

void h8_dma_channel_device::ioara8_w(uint8_t data)
{
	ioar[0] = data | 0xff00;
	logerror("ioara_w %04x\n", ioar[0]);
}

uint16_t h8_dma_channel_device::etcra_r()
{
	logerror("etcra_r %04x\n", etcr[0]);
	return etcr[0];
}

void h8_dma_channel_device::etcra_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&etcr[0]);
	logerror("etcra_w %04x\n", etcr[0]);
}

uint16_t h8_dma_channel_device::marbh_r()
{
	logerror("marbh_r %06x\n", mar[1]);
	return mar[1] >> 16;
}

void h8_dma_channel_device::marbh_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if(ACCESSING_BITS_0_7)
		mar[1] = ((data & 0x00ff) << 16) | (mar[1] & 0xffff);
	logerror("marbh_w %06x\n", mar[1]);
}

uint16_t h8_dma_channel_device::marbl_r()
{
	logerror("marbl_r %06x\n", mar[1]);
	return mar[1];
}

void h8_dma_channel_device::marbl_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	mar[1] = (mar[1] & ~mem_mask) | (data & mem_mask);
	logerror("marbl_w %06x\n", mar[1]);
}

uint16_t h8_dma_channel_device::ioarb_r()
{
	return ioar[1];
}

uint8_t h8_dma_channel_device::ioarb8_r()
{
	return ioar[1] & 0x00ff;
}

void h8_dma_channel_device::ioarb_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&ioar[1]);
	logerror("ioarb_w %04x\n", ioar[1]);
}

void h8_dma_channel_device::ioarb8_w(uint8_t data)
{
	ioar[1] = data | 0xff00;
	logerror("ioarb_w %04x\n", ioar[1]);
}

uint16_t h8_dma_channel_device::etcrb_r()
{
	logerror("etcrb_r %04x\n", etcr[1]);
	return etcr[1];
}

void h8_dma_channel_device::etcrb_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&etcr[1]);
	logerror("etcrb_w %04x\n", etcr[1]);
}

uint16_t h8_dma_channel_device::dmacr_r()
{
	logerror("dmacr_r %04x\n", dmacr);
	return dmacr;
}

void h8_dma_channel_device::dmacr_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&dmacr);
	logerror("dmacr_w %04x\n", dmacr);
	start_test(-1);
}

// H8H DMA
uint8_t h8_dma_channel_device::dtcra_r()
{
	logerror("dtcra_r %02x\n", dtcr[0]);
	return dtcr[0];
}

void h8_dma_channel_device::dtcra_w(uint8_t data)
{
	dtcr[0] = data;
	logerror("dtcra_w %02x\n", dtcr[0]);
	if((dtcr[0] & 0x80) && (dtcr[1] & 0x80)) { // if both DTME and DTE are set, start  DMA
		h8h_sync();
	}
}

uint8_t h8_dma_channel_device::dtcrb_r()
{
	logerror("dtcrb_r %02x\n", dtcr[1]);
	return dtcr[1];
}

void h8_dma_channel_device::dtcrb_w(uint8_t data)
{
	dtcr[1] = data;
	logerror("dtcrb_w %02x\n", dtcr[1]);
	if((dtcr[0] & 0x80) && (dtcr[1] & 0x80)) { // if both DTME and DTE are set, start  DMA
		h8h_sync();
	}
}

void h8_dma_channel_device::h8h_sync()
{
	// update DMACR
	dmacr = 0;
	if(BIT(dtcr[0], 6)) dmacr |= 1 << 15; // DTSZ
	dmacr |= ((dtcr[0] & 0b110000) >> 4) << 13; // SAID/DTID, SAIDE/RPE
	dmacr |= ((dtcr[1] & 0b110000) >> 4) << 5; // DAID/DTID, DAIDE/RPE

	uint8_t _dte = 0;
	if(BIT(dtcr[0], 7)) _dte |= 0b01; // DTE
	if(BIT(dtcr[1], 7)) _dte |= 0b10; // DTME/DTE

	bool _fae = (dtcr[0] & 0b110) == 0b110; // A channel operates in full address mode when DTS2A and DTS1A are both set to 1.
	bool _sae = false; // don't support
	uint8_t _dta = 0; // don't support
	uint8_t _dtie = 0;
	if(_fae) {
		// Full address mode
		if(BIT(dtcr[0], 3)) _dtie = 0b11;
		if(BIT(dtcr[0], 0)) {
			// Block Transfer Mode
			dmacr |= 1 << 11; // BLKE
			if(BIT(dtcr[1], 3)) dmacr |= 1 << 12; // BLKDIR (TMS)
			switch(dtcr[1] & 0b111) { // DTP (DTS)
			case 0b000: dmacr |= 0b1000; break; // ITU channel 0
			case 0b001: dmacr |= 0b1001; break; // ITU channel 1
			case 0b010: dmacr |= 0b1010; break; // ITU channel 2
			case 0b011: dmacr |= 0b1011; break; // ITU channel 3
			case 0b110: dmacr |= 0b1000; break; // DREQ falling edge
			}
		} else {
			// Normal Mode
			switch(dtcr[1] & 0b111) { // DTP (DTS)
			case 0b000: dmacr |= 0b0111; break; // Auto-request (burst mode)
			case 0b010: dmacr |= 0b0110; break; // Auto-request (cycle-steal mode)
			case 0b110: dmacr |= 0b0010; break; // DREQ falling edge
			case 0b111: dmacr |= 0b0011; break; // DREQ low-level
			}
		}
	} else {
		// Short address mode
		if(BIT(dtcr[0], 3)) _dtie |= 0b01;
		if(BIT(dtcr[1], 3)) _dtie |= 0b10;
		for(int submodule = 0; submodule < 2; submodule++) {
			switch(dtcr[submodule] & 0b111) { // DTP, DTDIR (DTS)
			case 0b000: dmacr |= 0b01000 << (submodule ? 0 : 8); break; // ITU channel 0
			case 0b001: dmacr |= 0b01001 << (submodule ? 0 : 8); break; // ITU channel 1
			case 0b010: dmacr |= 0b01010 << (submodule ? 0 : 8); break; // ITU channel 2
			case 0b011: dmacr |= 0b01011 << (submodule ? 0 : 8); break; // ITU channel 3
			//case 0b011: dmacr |= 0b10001 << (submodule ? 0 : 8); break; // A/D converter conversion end (H8/3006)
			case 0b100: dmacr |= 0b00100 << (submodule ? 0 : 8); break; // SCI channel 0 transmission data empty
			case 0b101: dmacr |= 0b10101 << (submodule ? 0 : 8); break; // SCI channel 0 receive data full
			case 0b110: dmacr |= 0b00010 << (submodule ? 0 : 8); break; // DREQ falling edge (B only)
			case 0b111: dmacr |= 0b00011 << (submodule ? 0 : 8); break; // DREQ low-level (B only)
			}
		}
	}

	set_bcr(_fae, _sae, _dta, _dte, _dtie);

	start_test(-1);
}

void h8_dma_channel_device::set_bcr(bool _fae, bool _sae, uint8_t _dta, uint8_t _dte, uint8_t _dtie)
{
	fae  = _fae;
	sae  = _sae;
	dta  = _dta  & 3;
	dte  = _dte  & 3;
	dtie = _dtie & 3;
	logerror("fae=%d sae=%d dta=%d dte=%d dtie=%d\n", fae, sae, dta & 3, dte & 3, dtie & 3);
	start_test(-1);
}

bool h8_dma_channel_device::start_test(int vector)
{
	if(fae) {
		if(dte != 3)
			return false;

		if(dmacr & 0x0800)
			throw emu_fatalerror("%s: DMA startup test in full address/block mode unimplemented.\n", tag());
		else {
			// Normal Mode
			if(vector == -1) {
				start(0);
				return true;
			} else {
				// DREQ trigger
				if(((dmacr & 0b111) == 0b0010 && vector == DREQ_EDGE) || ((dmacr & 0b111) == 0b0011 && vector == DREQ_LEVEL)) {
					state[0].suspended = false;
					return true;
				}
			}
			return false;
		}
	} else {
		if(dte == 0)
			return false;

		if(vector == -1) {
			// A has priority over B
			if(dte & 2)
				start(1);
			if(dte & 1)
				start(0);
			return true;
		} else if(dte & 2) {
			// DREQ trigger (B only)
			if(((dmacr & 0b111) == 0b0010 && vector == DREQ_EDGE) || ((dmacr & 0b111) == 0b0011 && vector == DREQ_LEVEL)) {
				state[1].suspended = false;
				return true;
			}
		}
		return false;
	}
}

void h8_dma_channel_device::start(int submodule)
{
	if(fae) {
		if(dmacr & 0x0800)
			throw emu_fatalerror("%s: DMA start in full address/block mode unimplemented.\n", tag());
		else {
			state[submodule].source = mar[0];
			state[submodule].dest = mar[1];
			state[submodule].count = etcr[0] ? etcr[0] : 0x10000;
			state[submodule].mode_16 = dmacr & 0x8000;
			state[submodule].autoreq = (dmacr & 6) == 6;
			state[submodule].suspended = !state[submodule].autoreq; // non-auto-request transfers start suspended
			int32_t step = state[submodule].mode_16 ? 2 : 1;
			state[submodule].incs = dmacr & 0x2000 ? dmacr & 0x4000 ? -step : step : 0;
			state[submodule].incd = dmacr & 0x0020 ? dmacr & 0x0040 ? -step : step : 0;
			cpu->set_current_dma(state + submodule);
		}
	} else {
		uint8_t cr = submodule ? dmacr & 0x00ff : dmacr >> 8;
		state[submodule].mode_16 = cr & 0x80;
		state[submodule].autoreq = false;
		state[submodule].suspended = true;
		int32_t step = state[submodule].mode_16 ? 2 : 1;
		if(!(cr & 0x20)) {
			// Sequential mode
			state[submodule].count = etcr[submodule] ? etcr[submodule] : 0x10000;
			state[submodule].incs = cr & 0x40 ? -step : step;
		} else if(dtie & (1 << submodule)) {
			// Idle mode
			state[submodule].count = etcr[submodule] ? etcr[submodule] : 0x10000;
			state[submodule].incs = 0;
		} else {
			// Repeat mode
			state[submodule].count = etcr[submodule] & 0x00ff ? etcr[submodule] & 0x00ff : 0x100;
			state[submodule].incs = cr & 0x40 ? -step : step;
		}
		if(cr & 0x10) {
			state[submodule].source = 0xff0000 | ioar[submodule];
			state[submodule].dest = mar[submodule];
			state[submodule].incd = state[submodule].incs;
			state[submodule].incs = 0;
		} else {
			state[submodule].source = mar[submodule];
			state[submodule].dest = 0xff0000 | ioar[submodule];
			state[submodule].incd = 0;
		}
		cpu->set_current_dma(state + submodule);
	}
}

void h8_dma_channel_device::count_last(int submodule)
{
	logerror("count last on %d\n", submodule);
	if(!state[submodule].autoreq) // "The TEND signal goes low during the last write cycle."
		cpu->set_input_line(H8_INPUT_LINE_TEND0 + (state[submodule].id >> 1), ASSERT_LINE);
}

void h8_dma_channel_device::count_done(int submodule)
{
	logerror("count done on %d\n", submodule);
	if(!state[submodule].autoreq)
		cpu->set_input_line(H8_INPUT_LINE_TEND0 + (state[submodule].id >> 1), CLEAR_LINE);
	if(fae) {
		if(dmacr & 0x0800)
			throw emu_fatalerror("%s: DMA count done full address/block mode unimplemented.\n", tag());
		else {
			dte &= ~1;
			dmac->clear_dte(state[0].id);
			dtcr[0] &= ~0x80; // clear DTE (for H8H)
			if(dtie & 1)
				intc->internal_interrupt(irq_base + submodule);
		}
	} else {
		uint8_t cr = submodule ? dmacr & 0x00ff : dmacr >> 8;
		if((cr & 0x20) && !(dtie & (1 << submodule))) {
			// Repeat mode
			state[submodule].count = etcr[submodule] & 0x00ff ? etcr[submodule] & 0x00ff : 0x100;
			if(cr & 0x10)
				state[submodule].dest = mar[submodule];
			else
				state[submodule].source = mar[submodule];
		} else {
			dte &= ~(1 << submodule);
			dmac->clear_dte(state[0].id + submodule);
			dtcr[submodule] &= ~0x80; // clear DTE (for H8H)
			if(dtie & (1 << submodule))
				intc->internal_interrupt(irq_base + submodule);
		}
	}
}
