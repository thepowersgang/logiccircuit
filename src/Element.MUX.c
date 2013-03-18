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
	
	if( NInputs != bits + (1 << bits)*busSize ) {
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
	 int	val = 0;
	
	// Parse inputs as a binary stream
	for( int i = 0; i < info->Bits; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			val |= 1 << i;
	}
	
	// If ENABLE, drive output
	for( int i = 0; i < Ele->NOutputs; i ++)
	{
		if( GetLink(Ele->Inputs[info->Bits + val*Ele->NOutputs + i]) )
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
