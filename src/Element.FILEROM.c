/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>	// printf
#include <filerom.h>

typedef struct
{
	 int	DataLines;
	 int	AddressLines;
	 int	ROMID;
	const void	*Data;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	if( NParams != 3 )	return NULL;

	 int	n_data_lines = Params[0];
	 int	n_addr_lines = Params[1];
	 int	rom_id = Params[2];

	if( n_data_lines != 8 && n_data_lines != 16 && n_data_lines != 32 && n_data_lines != 64 ) {
		printf("Data lines value bad (%i)\n", n_data_lines);
		return NULL;
	}
	if( n_addr_lines > 22 ) {	// Max 16MiB (at 32 bit)
		printf("Too many address lines\n");
		return NULL;
	}

	if( NInputs != 1 + n_addr_lines ) {
		printf("Bad input line count (%i != %i)\n", NInputs, 1 + n_addr_lines);
		return NULL;
	}
	
	if( rom_id < 0 || rom_id >= giNumROMFiles ) {
		printf("Requested ROM ID %i, which was not defined\n", rom_id);
		return NULL;
	}

	size_t	rom_size = (1 << n_addr_lines) * (n_data_lines / 8);

	if( gaROMFileSizes[rom_id] != rom_size ) {
		printf("ROM %i is not the expected size (exp 0x%zx, got 0x%zx)\n",
			rom_id, rom_size, gaROMFileSizes[rom_id]);
		return NULL;
	}
	
	tElement *ret = EleHelp_CreateElement( NInputs, 1 + n_data_lines, sizeof(t_info) );
	if(!ret)	return NULL;
	t_info	*info = ret->Info;

	info->DataLines = n_data_lines;
	info->AddressLines = n_addr_lines;
	info->ROMID = rom_id;
	info->Data = gaROMFileData[rom_id];

	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	size_t	addr = 0;
	uint64_t	rv;

	// Check enable line
	if( !GetEleLink(Ele, 0) )
		return ;

	// Get address
	for( int i = 0; i < this->AddressLines; i ++ )
		addr |= GetEleLink(Ele, 1 + i) ? (1 << i) : 0;

	switch( this->DataLines )
	{
	case 32:
		rv = ((const uint32_t*)this->Data)[addr];
		break;
	case 16:
		rv = ((const uint16_t*)this->Data)[addr];
		break;
	default:
		rv = 0;
		break;
	}

	SetEleLink(Ele, 0, true);
	for( int i = 0; i < this->DataLines; i ++ ) {
		SetEleLink(Ele, 1+i, !!(rv & (1 << i)));
	}
}

tElementDef gElement_FILEROM = {
	NULL, "FILEROM",
	0, -1,
	1,
	_Create,
	NULL,
	_Update
};
