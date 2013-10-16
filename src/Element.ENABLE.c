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
	if( !GetEleLink(Ele, 0) )
		return ;

	for( int i = 0; i < Ele->NInputs-1; i ++ )
	{
		SetEleLink(Ele, i, GetEleLink(Ele, 1+i));
	}
}

tElementDef gElement_ENABLE = {
	NULL, "ENABLE",
	2, -1,
	1,
	_Create,
	NULL,
	_Update
};
