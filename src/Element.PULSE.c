/*
 * PULSE element
 * - Raises the output for only one tick when input rises
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	bool	CurVal;
	bool	Dir;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Param, int NInputs)
{
	tElement *ret = EleHelp_CreateElement(1, 1, sizeof(t_info));
	if(!ret)	return NULL;
	t_info *info = ret->Info;

	if( NParams >= 1 && Param[0] != 0 )
		info->Dir = 1;
	else
		info->Dir = 0;

	info->CurVal = 0;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	bool	nextval = GetEleLink(Ele, 0);
	
	if( this->Dir )
	{
		// Single pulse on trailing edge
		SetEleLink( Ele, 0, (this->CurVal && !nextval) );
	}
	else
	{
		// Single pulse on rising edge
		SetEleLink( Ele, 0, (!this->CurVal && nextval) );
	}
	this->CurVal = nextval;
}

tElementDef gElement_PULSE = {
	NULL, "PULSE",
	1, 1, 2,	// settles in two ticks (Raise, lower)
	_Create,
	NULL,
	_Update
};
