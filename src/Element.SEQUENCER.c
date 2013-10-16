/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 *
 * Element.SEQUENCER.c
 * - Sequencer Element
 *
 * A combination of a multiplexer and a counter, on a pulse it sets the
 * next output line and clears the previous.
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	 int	Position;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	if( NParams != 1 )	return NULL;

	tElement *ret = EleHelp_CreateElement(3, Params[0], sizeof(t_info));
	if(!ret)	return NULL;	
	t_info *info = ret->Info;

	info->Position = 0;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	if( !GetEleLink(Ele, 0) )
		return ;
	
	// [1]: Reset
	if( GetEleLink(Ele, 1) ) {
		this->Position = 0;
	}
	// [2]: Next!
	else if( GetEleLink(Ele, 2) )
	{
		this->Position ++;
		if( this->Position == Ele->NOutputs )
			this->Position = 0;
	}
	// State maintained
	else {
	}

	SetEleLink(Ele, this->Position, true);
}

tElementDef gElement_SEQUENCER = {
	NULL, "SEQUENCER",
	3, 3,
	1,
	_Create,
	NULL,
	_Update
};

