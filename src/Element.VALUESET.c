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
	tElement	Ele;
	 int	CurSet;
	 int	SetCount;
	 int	CurDelay;
	 
	 int	SetDelay;
	 
	 int	SetSize;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	
	if( Param < 1 )
		Param = 1;
	
	ret = calloc( 1, sizeof(t_element) + (Param+NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	if( NInputs % Param != 0 )
		return NULL;
	
	ret->CurSet = 0;
	ret->CurDelay = 1;
	
	ret->SetSize = Param;
	ret->SetDelay = 1;
	
	ret->SetCount = NInputs / Param;
	
	ret->Ele.NOutputs = Param;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[Param];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	t_element	*this = (t_element *)Ele;
	 int	i;
	
	// Single cycle delay
	this->CurDelay --;
	
	for( i = 0; i < this->SetSize; i ++ )
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
	_Update
};
