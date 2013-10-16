/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	tElement *ret = EleHelp_CreateElement(NInputs, NInputs, 0);
	if(!ret)	return NULL;
	return ret;
}

static void _Update(tElement *Ele)
{
	for( int i = 0; i < Ele->NInputs; i ++ )
	{
		SetEleLink(Ele, i, !GetEleLink(Ele, i));
	}
}

tElementDef gElement_NOT = {
	NULL, "NOT",
	1, -1, 1,
	_Create,
	NULL,
	_Update
};
