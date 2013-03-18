/*
 * VALUESET{size}
 * - Iterates through a set of `size` values (input lines)
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	 int	CurSet;
	 int	SetCount;
	 int	CurDelay;
	 
	 int	SetDelay;
	 
	 int	SetSize;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	size = 1;
	
	if( NParams > 0 )
		size = Params[0];

	if( size < 1 )
		size = 1;
	
	if( NInputs % size != 0 )
		return NULL;
	
	tElement *ret = EleHelp_CreateElement(NInputs, size, sizeof(t_info));
	if(!ret)	return NULL;
	t_info	*info = ret->Info;
	
	info->CurSet = 0;
	info->CurDelay = 1;
	
	info->SetSize = size;
	info->SetDelay = 1;
	
	info->SetCount = NInputs / size;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	
	// Single cycle delay
	this->CurDelay --;
	
	for( int i = 0; i < this->SetSize; i ++ )
	{
		if( GetLink(Ele->Inputs[this->CurSet*this->SetSize + i]) )
			RaiseLink(Ele->Outputs[i]);
	}
	
	if( this->CurDelay == 0 )
	{
		this->CurSet ++;
		if( this->CurSet == this->SetCount )
			this->CurSet = 0;
		this->CurDelay = this->SetDelay;
	}
}

tElementDef gElement_VALUESET = {
	NULL, "VALUESET",
	1, -1,
	_Create,
	NULL,
	_Update
};
