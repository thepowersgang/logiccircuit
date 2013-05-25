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
#include <stdio.h>

struct info
{
	 int	databits;
};

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	bits = 1;
	 int	databits = 1;

	if( NParams < 1 || NParams > 2 )	return NULL;
	bits = Params[0];
	if( NParams == 2 )
		databits = Params[1]; 
	
	if( NInputs < databits + bits ) {
		fprintf(stderr, "DEMUX: Input count %i < expected %i", NInputs, databits + bits);
		return NULL;
	}
	
	tElement *ret = EleHelp_CreateElement(NInputs, databits * (1 << bits), sizeof(struct info));
	if(!ret)	return NULL;
	struct info	*info = ret->Info;
	info->databits = databits;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	struct info	*info = Ele->Info;
	unsigned int	sel = 0;
	
	// Parse inputs as a binary stream
	for( int i = info->databits; i < Ele->NInputs; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			sel |= 1 << (i-info->databits);
	}
	
	// If ENABLE, drive output
	for( int i = 0; i < info->databits; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			RaiseLink(Ele->Outputs[sel*info->databits+i]);
	}
}

tElementDef gElement_DEMUX = {
	NULL, "DEMUX",
	2, -1,
	_Create,
	NULL,
	_Update
};
