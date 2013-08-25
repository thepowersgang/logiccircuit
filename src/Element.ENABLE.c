/*
 * LogicCircuit
 * 
 * Element.ENABLE.c
 * - Line enable switch (for busses)
 *
 * @bus = ENABLE $enable, @lines
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	if( NParams != 0 )	return NULL;

	return EleHelp_CreateElement( NInputs, NInputs-1, 0);
}

static void _Update(tElement *Ele)
{
	if( GetLink(Ele->Inputs[0]) )
	{
		for( int i = 0; i < Ele->NInputs-1; i ++ )
		{
			if( GetLink(Ele->Inputs[i+1]) )
				RaiseLink(Ele->Outputs[i]);
		}
	}
	else
	{
		// No raise
	}
}

tElementDef gElement_ENABLE = {
	NULL, "ENABLE",
	2, -1,
	_Create,
	NULL,
	_Update
};
