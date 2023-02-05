#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "types.h"

extern uint8_t cmd_switches;
extern uint16_t bus_switches;
extern uint16_t bus_status;

#include "pi_panel.h"

extern uint8_t memory[64*1024];

uint8_t read8(uint16_t address)
{
	uint8_t data;
        if(address < 64*1024)
                data =  memory[address];
	else
		data = 0;
        return data;
}

void write8(uint16_t address, uint8_t val)
{
        if(address < 64*1024)
                memory[address] = val;
}

uint16_t read16(uint16_t address)
{
        uint16_t result = 0;
        result = read8(address);
        result |= read8(address+1) << 8;
        return result;
}

void write16(uint16_t address, uint16_t val)
{
        write8(address, val & 0xff);
        write8(address+1, (val >> 8) & 0xff);
}

#endif