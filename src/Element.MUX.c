/*
 * Multiplexer
 * INPUTS:
 * - Select[bits]
 * - Data[1 << bits]
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	 int	Bits;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	bits = 1;
	 int	busSize = 1;

	if( NParams > 2 )	return NULL;
	if( NParams >= 1 )	bits = Params[0];
	if( NParams >= 2 )	busSize = Params[1];
	
	if( bits < 1 )	bits = 1;
	if( busSize < 1 )	busSize = 1;
	
	 int	exp_inputs = 1 + bits + (1 << bits)*busSize;
	if( NInputs != exp_inputs ) {
		printf("Input count invalid (%i != %i)\n", NInputs, exp_inputs);
		return NULL;
	}
	
	tElement *ret = EleHelp_CreateElement(NInputs, busSize, sizeof(t_info));
	if(!ret)	return NULL;	
	t_info	*info = ret->Info;
	
	info->Bits = bits;
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*info = Ele->Info;
	unsigned int	val = 0;
	const int	first_sel_bit = 1;
	const int	first_data_bit = first_sel_bit + info->Bits;

	if( !GetLink(Ele->Inputs[0]) )
		return ;

	// Parse inputs as a binary stream
	for( int i = 0; i < info->Bits; i ++ )
	{
		if( GetLink(Ele->Inputs[first_sel_bit+i]) )
			val |= 1 << i;
	}
	
	const int data_start = first_data_bit + val*Ele->NOutputs;
	
	// If ENABLE, drive output
	for( int i = 0; i < Ele->NOutputs; i ++)
	{
		if( GetLink(Ele->Inputs[data_start + i]) )
			RaiseLink(Ele->Outputs[i]);
	}
}

tElementDef gElement_MUX = {
	NULL, "MUX",
	3, -1,
	_Create,
	NULL,
	_Update
};
