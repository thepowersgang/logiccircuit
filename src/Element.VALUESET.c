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
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret;
	 int	size = 1;
	
	if( NParams > 0 )
		size = Params[0];

	if( size < 1 )
		size = 1;
	
	if( NInputs % size != 0 )
		return NULL;
	
	ret = calloc( 1, sizeof(t_element) + (size+NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->CurSet = 0;
	ret->CurDelay = 1;
	
	ret->SetSize = size;
	ret->SetDelay = 1;
	
	ret->SetCount = NInputs / size;
	
	ret->Ele.NOutputs = size;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[size];
	return &ret->Ele;
}

static tElement *_Duplicate(tElement *Source)
{
	 int	size = sizeof(t_element) + (Source->NOutputs+Source->NInputs)*sizeof(tLink*);
	t_element *ret = malloc( size );
	memcpy(ret, Source, size);
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[size];
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
	_Duplicate,
	_Update
};
