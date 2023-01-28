#include <stdio.h>
#include <stdlib.h>
#include "intel8080.h"
#include "88dcdd.h"
	// socket
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <fcntl.h>

	// strcat
#include <string.h>
#include "pi_panel.h"
#include <wiringPi.h>

int sock;
int client_sock;

void dump_regs(intel8080_t *cpu)
{
	printf("Adr:%04x\t DB:%02x\t PC:%04x\t C:%02x\t D:%02x\t E:%02x\n", cpu->address_bus, cpu->data_bus, cpu->registers.pc, cpu->registers.c, cpu->registers.d, cpu->registers.e);
}

uint8_t term_in()
{
	uint8_t b;

	if(recv(client_sock, (char*)&b, 1, 0) != 1)
	{
		return 0;
	}
	else
	{
		return b;
	}
}

void term_out(uint8_t b)
{
	b = b & 0x7f;
	send(client_sock, (char*)&b, 1, 0);
}

uint8_t memory[64*1024];
uint16_t cmd_switches;
uint16_t bus_switches;

void load_file(intel8080_t *cpu)
{
	size_t size = 0;
	FILE* fp = fopen("software/input.com", "rb");

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(&memory[0x100], 1, size, fp);
	fclose(fp);
}

const char *byte_to_binary(int x)
{
	int z;
    static char b[9];
    b[0] = '\0';

    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}


void load_mem_file(const char* filename, size_t offset)
{
	size_t size;
	FILE* fp = fopen(filename, "rb");
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(&memory[offset], 1, size, fp);
	fclose(fp);
}

uint8_t sense()
{
	return bus_switches >> 8;
}

void load_raw_data(uint8_t program[], int s, int offset) {
   for (int i=0; i<s; i++) {
     memory[i+offset] = program[i];
   }
}

void load_roms()
{
       load_mem_file("software/ROMs/DBL.bin", 0xff00);
       /*uint8_t bootldr[] = {
       0x21,0x13,0xFF,0x11,0x00,0x2C,0x0E,0xEB,
       0x7E,0x12,0x23,0x13,0x0D,0xC2,0x08,0xFF,0xEC,
       0xC3,0x00,0x2C,0xF3,0xAF,0xD3,0x22,0x2F,0xD3,
       0x23,0x3E,0x2C,0xD3,0x22,0x3E,0x03,0x96,
       0xD3,0x10,0xDB,0xFF,0xE6,0x10,0x0F,0x0F,
       0xC6,0x10,0xD3,0x10,0x31,0x79,0x2D,0xAF,0xC1,
       0xD3,0x08,0xDB,0x08,0xE6,0x08,0xC2,0x1C,0x2C,
       0x3E,0x04,0xD3,0x09,0xC3,0x38,0x2C,0xC6,
       0xDB,0x08,0xE6,0x02,0xC2,0x2D,0x2C,0x3E,0x02,
       0xD3,0x09,0xDB,0x08,0xE6,0x40,0xC2,0xE4,
       0x2D,0x2C,0x11,0x00,0x00,0x06,0x00,0x3E,0x10,
       0xF5,0xD5,0xC5,0xD5,0x11,0x86,0x80,0x68,
       0x21,0xEB,0x2C,0xDB,0x09,0x1F,0xDA,0x50,
       0x2C,0xE6,0x1F,0xB8,0xC2,0x50,0x2C,0xDB,0x2A,
       0x8B,0x7F,0xA5,0xC2,0xCD,0xB0,0xA7,0x72,0x31,
       0xDC,0xA7,0x22,0xC1,0xDD,0xB0,0xA3A
       7723C25C2CE111EE2C0180001A77BEC2EF
       CB2C804713230DC2792C1AFEFFC2902C64
       131AB8C1EBC2C22CF1F12AEC2CCDE52C0E
       D2BB2C040478FE20DA442C0601CA442C5F
       DB08E602C2AD2C3E01D309C3422C3E80C1
       D308C30000D1F13DC2462C3E43013E4D43
       FB320000220100473E80D30878D301D3C2
       11D305D323C3DA2C7ABCC07BBDC9000062
       load_raw_data(bootldr,sizeof(bootldr),0xff00);
       */
       load_mem_file("software/ROMs/8KBasic/8kBas_e0.bin", 0xe000);
       load_mem_file("software/ROMs/8KBasic/8kBas_e8.bin", 0xe800);
       load_mem_file("software/ROMs/8KBasic/8kBas_f0.bin", 0xf000);
       load_mem_file("software/ROMs/8KBasic/8kBas_f8.bin", 0xf800);
}

int main(int argc, char *argv[])
{
	uint32_t counter = 0;
	unsigned long ok = 1;
	char yes = 1;
	struct sockaddr_in listen_addr;
	struct sockaddr client_addr;
	int sock_size;
	uint16_t breakpoint = 0x0;
	disk_controller_t disk_controller;
	intel8080_t cpu;

	rpi_init();

	memset(memory, 0, 64*1024);
	sock = socket(AF_INET, SOCK_STREAM, 0);

	setsockopt(sock, SOL_SOCKET, SO_LINGER, &yes, sizeof(char));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = INADDR_ANY;
	listen_addr.sin_port = htons(8800);
	memset(&(listen_addr.sin_zero), '\0', 8);

	if(bind(sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) == -1)
	{
		printf("Could not bind\n");
	}

       //Need to remove the wait for the telnet connction
	printf("Waiting for terminal on port 8800...\n");
	do
	{
		listen(sock, 1);
		sock_size = sizeof(client_addr);
		client_sock = accept(sock, &client_addr, &sock_size);
	}while(client_sock == -1);

	printf("Got connection. %d\n", client_sock);

	disk_controller.disk_function = disk_function;
	disk_controller.disk_select = disk_select;
	disk_controller.disk_status = disk_status;
	disk_controller.read = read;
	disk_controller.write = write;
	disk_controller.sector = sector;

	i8080_reset(&cpu, term_in, term_out, sense, &disk_controller);

	disk_drive.nodisk.status = 0xff;

	i8080_examine(&cpu, 0x0000); //This sets CPU start to 0x000

	uint8_t cmd_state;
	uint8_t last_cmd_state = 0;
	uint8_t mode = STOP;
	uint32_t last_debounce = 0;

	uint32_t cycle_counter = 0;

	while(1)
	{
                //dump_regs(&cpu);
                //printf("Cmd:%04x\tBus:%04x\n",cmd_switches,bus_switches);
		printf("Mode:%02x\n",mode);
		if(mode == RUN)
		{
			i8080_cycle(&cpu);
			cycle_counter++;
			if(cycle_counter % 10 == 0)
				read_write_panel(0, cpu.data_bus, cpu.address_bus, &bus_switches, &cmd_switches, 1);
		}
		else
		{
			read_write_panel(0, cpu.data_bus, cpu.address_bus, &bus_switches, &cmd_switches, 1);
		}

		if(cmd_switches != last_cmd_state)
		{
			last_debounce = millis();
		}

		if((millis() - last_debounce) > 50)
		{
			if(cmd_switches != cmd_state)
			{
				cmd_state = cmd_switches;
				if(mode == STOP)
				{
					if(cmd_switches & STOP)
					{
						i8080_examine(&cpu, 0);
					}
					if(cmd_switches & SINGLE_STEP)
					{
						i8080_cycle(&cpu);
					}
					if(cmd_switches & EXAMINE)
					{
						printf("Examine %x\n", bus_switches);
						i8080_examine(&cpu, bus_switches);
					}
					if(cmd_switches & EXAMINE_NEXT)
					{
						i8080_examine_next(&cpu);
					}
					if(cmd_switches & DEPOSIT)
					{
						i8080_deposit(&cpu, bus_switches & 0xff);
					}
					if(cmd_switches & DEPOSIT_NEXT)
					{
						i8080_deposit(&cpu, bus_switches & 0xff);
					}
					if(cmd_switches & RUN_CMD)
					{
						mode = RUN;
					}
					if(cmd_switches & AUX1_UP)
					{
					printf("Aux1 Up: Load ROMs");
        				load_roms();
					}
					if(cmd_switches & AUX1_DOWN)
					{
					printf("Aux1 Down: Load ROMs and Software");
        				load_roms();
					// Mount diskette 1 (CP/M OS) and 2 (Tools)
					disk_drive.disk1.fp = fopen("software/CPM2.2/cpm63k.dsk", "r+b");
					disk_drive.disk1.fp = fopen("software/BASIC/Disk Basic Ver 300-5-F.dsk", "r+b");
					disk_drive.disk2.fp = fopen("software/CPM2.2/zork.dsk", "r+b");
					disk_drive.disk2.fp = fopen("software/BASIC/Floppy Disk/Games on 300-5-F.dsk", "r+b");
					// Do an examine to 0xff00
					}
				}
				if(mode == RUN)
				{
					if(cmd_switches & STOP_CMD)
					{
						mode = STOP;
					}
				}
			}
		}
		last_cmd_state = cmd_switches;
	}

	return 0;
}
