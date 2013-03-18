/*
 * De-Multiplexer
 * INPUTS:
 * - Data
 * - Select[bits]
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	bits = 1;

	if( NParams != 1 )	return NULL;
	bits = Params[0];
	
	if( NInputs < 1 + bits ) {
		return NULL;
	}
	
	tElement *ret = EleHelp_CreateElement(NInputs, 1 << bits, 0);
	if(!ret)	return NULL;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	 int	val = 0;
	
	// Parse inputs as a binary stream
	for( int i = 1; i < Ele->NInputs; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			val |= 1 << (i-1);
	}
	
	// If ENABLE, drive output
	if( GetLink(Ele->Inputs[0]) )
		RaiseLink(Ele->Outputs[val]);
}

tElementDef gElement_DEMUX = {
	NULL, "DEMUX",
	2, -1,
	_Create,
	NULL,
	_Update
};
