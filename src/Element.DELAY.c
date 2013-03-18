/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	 int	Delay;
	 int	Pos;
	int8_t	*Cache;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	delay_time = 0;
	
	if( NParams > 1 )	return NULL;
	
	if( NParams >= 1 )	delay_time = Params[0];
	
	if( delay_time < 1 )	delay_time = 1;

	size_t	cache_size = (delay_time-1)*NInputs;
	tElement *ret = EleHelp_CreateElement( NInputs, NInputs, sizeof(t_info) + cache_size);
	if(!ret)	return NULL;
	t_info	*info = ret->Info;
	
	info->Pos = 0;
	info->Delay = delay_time - 1;
	info->Cache = (void*)(info + 1);
	memset(info->Cache, 0, cache_size);
	
	return ret;
}

static int _Duplicate(const tElement *Source, tElement *New)
{
	t_info	*info = New->Info;
	info->Cache = (void*)(info + 1);
	return 0;
}

// DELAY{1}:
// if(GetLink()) RaiseLink()
// DELAY{2}:
// if(saved) RaiseLink()
// saved = GetLink();
// DELAY{n}:
// if(saved[(pos+1)%(n-1)])
//   RaiseLink()
// saved[pos%(n-1)] = GetLink();

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	 int	i;
	
	// Single cycle delay
	if( this->Delay == 0 )
	{
		for( i = 0; i < Ele->NInputs; i ++ )
		{
			if( GetLink(Ele->Inputs[i]) )
				RaiseLink(Ele->Outputs[i]);
		}
	}
	else
	{
		 int	ofs = this->Pos * Ele->NInputs;
		
		for( i = 0; i < Ele->NInputs; i ++ )
		{
			if( this->Cache[ ofs + i ] )
				RaiseLink(Ele->Outputs[i]);
	
			this->Cache[ ofs + i ] = GetLink(Ele->Inputs[i]);
		}
		this->Pos ++;
		if(this->Pos == this->Delay)	this->Pos = 0;
	}
}

tElementDef gElement_DELAY = {
	NULL, "DELAY",
	1, -1,
	_Create,
	_Duplicate,
	_Update
};
