/*
 * PULSE element
 * - Raises the output for only one tick when input rises
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	 int	CurVal;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Param, int NInputs)
{
	tElement *ret = EleHelp_CreateElement(1, 1, sizeof(t_info));
	if(!ret)	return NULL;
	t_info *info = ret->Info;
	
	info->CurVal = 0;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	
	// Single pulse on rising edge
	if( !this->CurVal && GetLink(Ele->Inputs[0]) ) {
		RaiseLink(Ele->Outputs[0]);
	}
	this->CurVal = !!GetLink(Ele->Inputs[0]);
}

tElementDef gElement_PULSE = {
	NULL, "PULSE",
	1, 1,
	_Create,
	NULL,
	_Update
};
