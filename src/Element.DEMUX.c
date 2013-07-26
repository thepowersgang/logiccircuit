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
	 int	bits;
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
	
	if( NInputs != 1 + databits + bits ) {
		fprintf(stderr, "DEMUX: Input count %i != expected %i\n", NInputs, 1 + databits + bits);
		return NULL;
	}
	
	tElement *ret = EleHelp_CreateElement(NInputs, databits * (1 << bits), sizeof(struct info));
	if(!ret)	return NULL;
	struct info	*info = ret->Info;
	info->databits = databits;
	info->bits = bits;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	struct info	*info = Ele->Info;
	unsigned int	sel = 0;
	
	// Parse inputs as a binary stream
	for( int i = 1; i < 1+info->bits; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			sel |= 1 << (i-info->databits);
	}
	
	// If ENABLE, drive output
	if( GetLink(Ele->Inputs[0]) )
	{
		const int in_ofs = 1+info->bits;
		const int out_ofs = sel*info->databits;
		for( int i = 0; i < info->databits; i ++ )
		{
			if( GetLink(Ele->Inputs[in_ofs + i]) )
				RaiseLink(Ele->Outputs[out_ofs+i]);
		}
	}
}

tElementDef gElement_DEMUX = {
	NULL, "DEMUX",
	2, -1,
	_Create,
	NULL,
	_Update
};
