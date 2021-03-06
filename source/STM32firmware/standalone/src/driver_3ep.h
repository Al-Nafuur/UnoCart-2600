#include <stdbool.h>
#include <stdint.h>

#include "cartridge_io.h"

/* 3E+ Bankswitching
 * ------------------------------
 * by Thomas Jentzsch, mostly based on the 'DASH' scheme (by Andrew Davie)
 * with the following changes:
 * RAM areas:
 *   - read $x000, write $x200
 *   - read $x400, write $x600
 *   - read $x800, write $xa00
 *   - read $xc00, write $xe00
 */
void emulate_cartridge_generic(uint8_t* buffer, uint32_t image_size, uint8_t* cart_ram)
{
	int cartRAMPages = 64;
	int cartROMPages = image_size / 1024;
	uint16_t addr, addr_prev = 0, data = 0, data_prev = 0;
	uint16_t last_addr = 0;
	bool banking_locked = true;
	uint16_t act_bank = 0;
	uint8_t* cart_rom = buffer;

	bool bankIsRAM[4] = { false, false, false, false };
	uint8_t *bankPtr[4] = { &cart_rom[0], &cart_rom[0], &cart_rom[0], &cart_rom[0] };

	__disable_irq();	// Disable interrupts

	while (1)
	{
		while ((addr = ADDR_IN) != addr_prev)
			addr_prev = addr;

		if ((last_addr & 0x1fff) == 0x1ffc && (addr & 0x1fff) == 0x1ffd) banking_locked = false;
		last_addr = addr;

		// got a stable address
		if (addr & 0x1000)
		{ // A12 high
			act_bank = (( addr & 0x0C00 ) >> 10); // bit 10 an 11 define the bank
			if (bankIsRAM[act_bank] && (addr & 0x200) )
			{	// we are accessing a RAM write address ($1200-$13FF, $1600-$17FF, $1A00-$1BFF or $1E00-$1FFF)
				// read last data on the bus before the address lines change
				while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }
				bankPtr[act_bank][addr & 0x1FF] = data_prev >>8;
			}
			else
			{	// reads to either RAM or ROM
				if (bankIsRAM[act_bank]){
					DATA_OUT = ((uint16_t)(bankPtr[act_bank][addr & 0x1FF]))<<8;	// RAM read
				}else{
					DATA_OUT = ((uint16_t)(bankPtr[act_bank][addr & 0x3FF]))<<8;	// ROM read
				}

				SET_DATA_MODE_OUT
				// wait for address bus to change
				while (ADDR_IN == addr) ;
				SET_DATA_MODE_IN
			}
		} else if (!banking_locked) {
			while (ADDR_IN == addr) { data_prev = data; data = DATA_IN; }

			data_prev >>= 8;

			if (addr == 0x3e) {
				act_bank = (data_prev & 0x0C0) >> 6; // bit 6 and 7 define the bank
				bankIsRAM[act_bank] = true;
				bankPtr[act_bank] =  cart_ram + ( ((data_prev & 0x03F) % cartRAMPages) << 9 );	// * 512 switch in a RAM bank
			}
			else if ( addr == 0x3f ){
				act_bank = (data_prev & 0x0C0) >> 6; // bit 6 and 7 define the bank
				bankIsRAM[act_bank] = false;
				bankPtr[act_bank] = cart_rom + ( ((data_prev & 0x03F) % cartROMPages) << 10);	// * 1024 switch in a ROM bank
			}

		}
	}
	__enable_irq();
}
