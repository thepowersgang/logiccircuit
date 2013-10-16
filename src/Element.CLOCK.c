/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	 int	Period;
	 int	Pos;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	tElement *ret;

	if( NParams != 1 )	return NULL;

	ret = EleHelp_CreateElement(0, 1, sizeof(t_info));
	if(!ret)	return NULL;	

	t_info	*info = ret->Info;
	info->Period = Params[0];
	info->Pos = 0;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	SetEleLink(Ele, 0, (this->Pos == 0));
	this->Pos ++;
	if(this->Pos == this->Period)	this->Pos = 0;
}

tElementDef gElement_CLOCK = {
	NULL, "CLOCK",
	0, 0, 0,
	_Create,
	NULL,
	_Update
};
