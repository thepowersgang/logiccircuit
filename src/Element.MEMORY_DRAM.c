/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	 int	DataLines;
	 int	AddressLines;
	void	*Data;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;

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

	ret = calloc( 1, sizeof(t_element) + (NInputs + 1 + n_data_lines) * sizeof(tLink*)
		+ (1 << n_addr_lines) * (n_data_lines / 8) );
	if(!ret)	return NULL;

	ret->DataLines = n_data_lines;
	ret->AddressLines = n_addr_lines;
	
	ret->Ele.NOutputs = 1 + n_data_lines;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->Ele.Outputs[ret->Ele.NOutputs];
	ret->Data = &ret->Ele.Inputs[NInputs];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	t_element	*this = (t_element*)Source;
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*)
		+ (1 << this->AddressLines) * (this->DataLines / 8);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->Ele.Outputs[ret->Ele.NOutputs];
	ret->Data = &ret->Ele.Inputs[ret->Ele.NInputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
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
