/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

typedef struct
{
	 int	Delay;
	 int	Pos;
	int8_t	*Cache;
}	t_info;

tElementDef	gElement_DELAY_1_n;
tElementDef	gElement_DELAY_1_1;
tElementDef	gElement_DELAY_2_n;
tElementDef	gElement_DELAY_2_1;

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

	if( delay_time == 1 ) {
		ret->Type = (NInputs == 1 ? &gElement_DELAY_1_1 : &gElement_DELAY_1_n);
	}
	else if( delay_time == 2 ) {
		ret->Type = (NInputs == 1 ? &gElement_DELAY_2_1 : &gElement_DELAY_2_n);
	}
	else {
		// ret->Type = NULL;
		// default
	}
	
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
	
	// Single cycle delay
	assert(this->Delay);
	 int	ofs = this->Pos * Ele->NInputs;
	
	for( int i = 0; i < Ele->NInputs; i ++ ) {
		SetEleLink(Ele, i, this->Cache[ ofs + i ]);
		this->Cache[ ofs + i ] = GetEleLink(Ele, i);
	}
	this->Pos ++;
	if(this->Pos == this->Delay)	this->Pos = 0;
}

static void _Update_1_1(tElement *Ele)
{
	SetEleLink(Ele, 0, GetEleLink(Ele, 0));
}
static void _Update_1_n(tElement *Ele)
{
	for( int i = 0; i < Ele->NInputs; i ++ )
		SetEleLink(Ele, i, GetEleLink(Ele, i));
}
static void _Update_2_1(tElement *Ele)
{
	t_info	*this = Ele->Info;
	SetEleLink(Ele, 0, this->Cache[0]);
	this->Cache[0] = GetEleLink(Ele, 0);
}
static void _Update_2_n(tElement *Ele)
{
	t_info	*this = Ele->Info;
	for( int i = 0; i < Ele->NInputs; i ++ ) {
		SetEleLink(Ele, i, this->Cache[i]);
		this->Cache[i] = GetEleLink(Ele, i);
	}
}

tElementDef gElement_DELAY = {
	NULL, "DELAY",
	1, -1,
	0,	// Settle disabled (time is runtime-determined)
	_Create,
	_Duplicate,
	_Update
};

tElementDef gElement_DELAY_2_n = {
	NULL, "DELAY{1,n}",
	1, -1,
	2,
	_Create, _Duplicate,
	_Update_2_n
};
tElementDef gElement_DELAY_2_1 = {
	NULL, "DELAY{1,1}",
	1, 1,
	2,
	_Create, _Duplicate,
	_Update_2_1
};
tElementDef gElement_DELAY_1_n = {
	NULL, "DELAY{1,n}",
	1, -1,
	1,
	_Create, _Duplicate,
	_Update_1_n
};
tElementDef gElement_DELAY_1_1 = {
	NULL, "DELAY{1,1}",
	1, 1,
	1,
	_Create, _Duplicate,
	_Update_1_1
};
