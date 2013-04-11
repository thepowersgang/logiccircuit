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

	if( NParams && *Param )
		info->Dir = 1;
	else
		info->Dir = 0;

	info->CurVal = 0;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	bool	nextval = GetLink(Ele->Inputs[0]);
	
	if( this->Dir )
	{
		// Single pulse on trailing edge
		if( this->CurVal & !nextval )
			RaiseLink(Ele->Outputs[0]);
	}
	else
	{
		// Single pulse on rising edge
		if( !this->CurVal && nextval ) {
			RaiseLink(Ele->Outputs[0]);
		}
	}
	this->CurVal = nextval;
}

tElementDef gElement_PULSE = {
	NULL, "PULSE",
	1, 1,
	_Create,
	NULL,
	_Update
};
