/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>	// printf

typedef struct
{
	 int	DataLines;
	 int	AddressLines;
	void	*Data;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	if( NParams != 2 )	return NULL;

	 int	n_data_lines = Params[0];
	 int	n_addr_lines = Params[1];

	if( n_data_lines != 8 && n_data_lines != 16 && n_data_lines != 32 && n_data_lines != 64 ) {
		printf("Data lines value bad (%i)\n", n_data_lines);
		return NULL;
	}
	if( n_addr_lines > 22 ) {	// Max 16MiB (at 32 bit)
		printf("Too many address lines\n");
		return NULL;
	}

	if( NInputs != 1 + n_addr_lines + 1 + 2*n_data_lines ) {
		printf("Bad input line count (%i != %i)\n", NInputs, 1 + n_addr_lines + 1 + 2*n_data_lines);
		return NULL;
	}

	tElement *ret = EleHelp_CreateElement( NInputs, 1 + n_data_lines,
		sizeof(t_info) + (1 << n_addr_lines) * (n_data_lines / 8) );
	if(!ret)	return NULL;
	t_info	*info = ret->Info;

	info->DataLines = n_data_lines;
	info->AddressLines = n_addr_lines;
	info->Data = (void*)(info + 1);
	return ret;
}

static int _Duplicate(const tElement *Source, tElement *New)
{
	t_info	*info = New->Info;
	info->Data = (void*)(info + 1);
	return 0;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	size_t	addr = 0;
	uint64_t	rv;
	uint64_t	val = 0, mask = 0;
	const int	iWriteEnable = 1 + this->AddressLines;
	const int	iWriteMaskFirst = iWriteEnable + 1;
	const int	iWriteValFirst = iWriteMaskFirst + this->DataLines;

	// Check enable line
	if( !GetLink(Ele->Inputs[0]) )
		return ;

	for( int i = 0; i < this->AddressLines; i ++ )
		addr |= GetLink(Ele->Inputs[1 + i]) ? (1 << i) : 0;

	 int	bWrite = GetLink(Ele->Inputs[iWriteEnable]);
	for( int i = 0; i < this->DataLines; i ++ ) {
		val |= GetLink(Ele->Inputs[iWriteMaskFirst + i]) ? (1 << i) : 0;
		mask |= GetLink(Ele->Inputs[iWriteValFirst + i]) ? (1 << i) : 0;
	}

	switch( this->DataLines )
	{
	case 32:
		if( bWrite ) {
			((uint32_t*)this->Data)[addr] &= ~mask;
			((uint32_t*)this->Data)[addr] |= val;
		}
		rv = ((uint32_t*)this->Data)[addr];
		break;
	default:
		rv = 0;
		break;
	}

	for( int i = 0; i < this->DataLines; i ++ ) {
		if( rv & (1 << i) )
			RaiseLink(Ele->Outputs[1 + i]);
	}
	RaiseLink(Ele->Outputs[0]);
}

tElementDef gElement_MEMORY_DRAM = {
	NULL, "MEMORY_DRAM",
	0, -1,
	_Create,
	_Duplicate,
	_Update
};
