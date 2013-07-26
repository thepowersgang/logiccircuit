/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 * 
 * render.c
 * - Circuit visualisation
 */
#include <common.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#define UB_READ 	001
#define UB_WRITE	002
#define UB_EXTREAD	004
#define UB_EXTWRITE	010
#define UB_INPUT	020
#define UB_OUTPUT	040

#define COLOUR_SUBBLOCK	0xFF8800
#define COLOUR_GATE	0x0000FF
#define COLOUR_GENERIC	0x00FF00
#define COLOUR_LINE	0xFFFFFF
#define COLOUR_BUS	0xFFFF00

enum eDispElementType
{
	DISPELE_GENERIC,
	DISPELE_DELAY,	// NOT = Buffer with inverted output
	DISPELE_PULSE,
	DISPELE_HOLD,
	DISPELE_AND,
	DISPELE_OR,
	DISPELE_XOR,
	DISPELE_MUX,
	DISPELE_DEMUX,
};

// === TYPES ===
typedef struct sDispValue	tDispValue;
typedef struct sDispLine	tDispLine;
typedef struct sDispValRef	tDispValRef;
typedef struct sDispElement	tDispElement;
typedef struct sDispInfo	tDispInfo;

struct sDispLine
{
	tDispValue	*Value;
	 int	bVertical;
	 int	X, Y, Len;
};
struct sDispValue
{
	char	*Name;
	 int	UsageBits;
	 int	NumLines;
	tLinkValue	**Values;
	 int	NWrite, NRead;
};
struct sDispValRef
{
	tDispValue	*Value;
	 int	First, Last;
	 int	Ofs;
	uint64_t	Const;
};
struct sDispInfo
{
	const char	*Name;
	 int	nElements;
	tDispElement	**Elements;
	 int	nValues;
	tDispValue	**Values;
	 int	nLines;
	tDispLine	**Lines;
	
	 int	NInputs;
	tDispValRef	*Inputs;
	tDispValue	**LocalInputVals;
	 int	NOutputs;
	tDispValRef	*Outputs;
	tDispValue	**LocalOutputVals;	// Pointer to the parent's tDispValue for each output
	
	 int	X, Y, W, H;
};
struct sDispElement
{
	char	*Name;
	enum eDispElementType	Type;
//	void	*Ptr;

	 int	bInvertOut;

	 int	NInputs;
	tDispValRef	*Inputs;
	 int	NOutputs;
	tDispValRef	*Outputs;

	 int	X, Y, W, H;
};

typedef struct sRasterSuirface
{
	 int	Type;	// = 0
	 int	W, H;
	uint32_t	Data[];
} tRasterSurface;

// === CONSTANTS ===
#define SIZEBASE	30
const int	ciUnitXSize = SIZEBASE;
const int	ciUnitYSize = SIZEBASE;
const int	ciSubunitXSize = SIZEBASE/5;
const int	ciSubunitYSize = SIZEBASE/5;
const int	ciSingleLineWidth = 1;
const int	ciMultiLineWidth = 3;
tDispValue	gDispValueConst = {.Name = "-constant-", .UsageBits=UB_READ|UB_INPUT|UB_OUTPUT};

// === PROTOTYPES ===
void	Render_RenderBlockBMP(const char *DestFile, tBlock *Block, const char *Path);
void	*Render_RenderBlockRaster(tExecUnit *Unit, tBlock *Block, const char *Path);
void	Render_int_PrerenderBlockRec(tExecUnit *Unit, tBlock *Block);
void	Render_int_PrerenderBlock(tExecUnit *Unit, tBlock *Block);
void	Render_int_RasteriseBlock(tBlock *Block, void *Surface, int X, int Y, int MaxDepth);

// === CODE ===
void Render_RenderBlockBMP(const char *DestFile, tBlock *Block, const char *Path)
{
	tRasterSurface *imagedata = Render_RenderBlockRaster(Block->Unit, Block, Path);

	if( !imagedata ) {
		return ;
	}
	
	FILE *fp = fopen(DestFile, "wb");
	struct {
		char	Sig[2];
		uint32_t	Size;
		uint16_t	resvd;
		uint16_t	resvd2;
		uint32_t	DataOfs;
		
		uint32_t	HdrSize;
		 int32_t	ImgW;
		 int32_t	ImgH;
		uint16_t	NPlanes;
		uint16_t	BPP;
		uint32_t	Compression;
		uint32_t	DataSize;
		uint32_t	HRes;
		uint32_t	VRes;
		uint32_t	PaletteSize;
		uint32_t	NImportant;
		uint16_t	_pad;
	} __attribute__((packed)) hdr;

	hdr.Sig[0] = 'B';	hdr.Sig[1] = 'M';
	hdr.Size = sizeof(hdr) + imagedata->W*imagedata->H*4;
	hdr.DataOfs = sizeof(hdr);
	hdr.resvd = 0;
	hdr.resvd2 = 0;
	hdr.HdrSize = 40;
	hdr.ImgW = imagedata->W;
	hdr.ImgH = imagedata->H;
	hdr.NPlanes = 1;
	hdr.BPP = 32;
	hdr.Compression = 0;
	hdr.DataSize = imagedata->W*imagedata->H*4;
	hdr.HRes = 1000;
	hdr.VRes = 1000;
	hdr.PaletteSize = 0;
	hdr.NImportant = 0;
	hdr._pad = 0;

//	printf("fwrite(%p, %x, %x, %p)\n", &hdr, sizeof(hdr), 1, fp);
	fwrite(&hdr, sizeof(hdr), 1, fp);
//	printf("fwrite(%p, %x, %x, %p)\n", imagedata->Data, imagedata->H, imagedata->W*4, fp);
	for( int row = imagedata->H; row --; )
		fwrite(imagedata->Data+row*imagedata->W, imagedata->W, 4, fp);
	
	free(imagedata);
}

void *Render_RenderBlockRaster(tExecUnit *Unit, tBlock *Block, const char *Path)
{
	if( Path )
	{
		while( Path[0] )
		{
			char	*cpos = strchr(Path, ':');
			size_t	len = cpos ? cpos - Path : strlen(Path);
			 int	bMatch = 0;
			
			for( tBlock *blk = Block->SubBlocks; blk; blk = blk->Next )
			{
				if(strncmp(blk->Name, Path, len) != 0)
					continue;
				if(blk->Name[len] == '\0') {
					Block = blk;
					bMatch = 1;
					break;
				}
			}
			if( !bMatch )
				return NULL;
			Path += len + (Path[len] == ':');
		}
	}
	// Ensure all blocks are vectored
	Render_int_PrerenderBlockRec(Unit, Block);
	// Allocate image buffer
	int w = Block->DispInfo->W * ciUnitXSize;
	int h = Block->DispInfo->H * ciUnitYSize;
	// Apply some sane image ranges (8K each way)
	assert(w < (1<<13));
	assert(h < (1<<14));
	// Rasterise blocks (with optional max depth)
	tRasterSurface *ret = malloc( sizeof(tRasterSurface) + w * h * 4);
	for(int i = 0; i < w*h; i ++ )
		ret->Data[i] = 0xFF000000;
	assert(ret);
	ret->Type = 0;
	ret->W = w;
	ret->H = h;
	Render_int_RasteriseBlock(Block, ret, 0, 0, INT_MAX);
	printf("ret = %p\n", ret);
	return ret;
}

void Render_int_PrerenderBlockRec(tExecUnit *Unit, tBlock *Block)
{
	// Bottom-up vectorising of blocks
	// - Ensures that inputs/outputs of child blocks are known when parent is processed
	for( tBlock *child = Block->SubBlocks; child; child = child->Next )
		Render_int_PrerenderBlockRec(Unit, child);
	printf("Processing: '%s'\n", Block->Name);
	Render_int_PrerenderBlock(Unit, Block);
}

static inline void int_Prerender_MarkVal(int NumValues, tDispValue **Values, tLinkValue *Value, int Mask)
{
	if( Value->Info )
	{
		int ofs = (intptr_t)Value->Info - 1;
		//printf("Value->Info = %p\n", Value->Info);
		assert(Value->Info != (void*)-1);
		assert(0 <= ofs && ofs < NumValues);
		assert(Values[ofs]);
		Values[ofs]->UsageBits |= Mask;
#if 0
		if( strcmp(Values[ofs]->Name, "@stage_state") == 0 ) {
			printf("DBG Marking %s with 0%o (Now %s 0%o)\n",
				Value->FirstLink->Name, Mask,
				Values[ofs]->Name, Values[ofs]->UsageBits);
//			for( tLink *l = Value->FirstLink; l; l = l->ValueNext )
//				printf("> DBG %s\n", l->Name);
			for( int i = 0; i < Values[ofs]->NumLines; i ++ )
				printf("> DBG %s\n", Values[ofs]->Values[i]->FirstLink->Name);
		}
#endif
	}
}

int Render_int_ListToRefs(tDispInfo *Info, tDispValRef *Refs, int NItems, tLink *const*Items)
{
	 int	nRefs = 0;
	
	tDispValue	*grp = NULL;
	 int	dir = 0, count = 0;
	 int	first = 0, next = 0;
	for( int i = 0; i < NItems; i ++ )
	{
		tLinkValue	*val = Items[i]->Value;
		// Constant values are grouped
		if( val == &gValue_One || val == &gValue_Zero )
		{
			if( grp )
			{
				if( Refs ) {
					Refs[nRefs].Value = grp;
					Refs[nRefs].First = first;
					Refs[nRefs].Last = next-1;
					Refs[nRefs].Ofs = nRefs+2;
				}
				nRefs ++;
			}
			uint64_t	constval = 0;
			 int	const_ofs = 0;
			while( val == &gValue_One || val == &gValue_Zero ) {
				if( val == &gValue_One )
					constval |= (1ULL << const_ofs);
				if( ++const_ofs == 64 )
					break;
				if( ++i == NItems )
					break;
				val = Items[i]->Value;
			}
			if( Refs ) {
				Refs[nRefs].Value = &gDispValueConst;
				Refs[nRefs].First = 0;
				Refs[nRefs].Last = const_ofs-1;
				Refs[nRefs].Ofs = nRefs + 2;
				Refs[nRefs].Const = constval;
			}
			i --;
			nRefs ++;
			grp = NULL;
			continue ;
		}
		tDispValue	*dval = Info->Values[ (intptr_t)val->Info - 1 ];
		 int	idx = 0;
		for( int j = 0; j < dval->NumLines; j ++ ) {
			if(dval->Values[j] == val) {
				idx = j;
				break;
			}
		}
		// - n*$link
		if( grp && grp == dval && idx == next-1 && (dir == 0 || dir == 2) )
		{
			dir = 2;
			count ++;
		}
		// @list[n:n+m]
		else if( grp && grp == dval && idx == next && (dir == 0 || dir == 1) )
		{
			dir = 1;
			next ++;
		}
		// @list[n:n-m]
		else if( grp && grp == dval && idx == next-2 && (dir == 0 || dir == -1) )
		{
			dir = -1;
			next --;
		}
		else
		{
			// - Commit `grp`
			if( grp )
			{
				if( Refs ) {
					Refs[nRefs].Value = grp;
					Refs[nRefs].First = first;
					Refs[nRefs].Last = next-1;
					Refs[nRefs].Ofs = nRefs+2;
					Refs[nRefs].Const = count;
				}
				nRefs ++;
			}
			grp = dval;
			first = idx;
			next = first + 1;
			dir = 0;
			count = 0;
		}
	}
	if( grp )
	{
		if( Refs ) {
			Refs[nRefs].Value = grp;
			Refs[nRefs].First = first;
			Refs[nRefs].Last = next-1;
			Refs[nRefs].Ofs = nRefs+2;
			Refs[nRefs].Const = count;
		}
		nRefs ++;
	}
	return nRefs;
}

tDispValue *Render_int_PrerenderBlock_MakeVal(int nSubValues, const char *Name)
{
	tDispValue *ret = malloc(sizeof(tDispValue)
		+ nSubValues*sizeof(tLinkValue*)
		+ strlen(Name) + 1
		);
	assert(ret);
	ret->Values = (void*)(ret + 1);
	ret->Name = (void*)(ret->Values + nSubValues);
	
	strcpy(ret->Name, Name);
	ret->UsageBits = 0;
	ret->NumLines = nSubValues;
	ret->NWrite = 0;
	ret->NRead = 0;
	return ret;
}

tDispValue **Render_int_PrerenderBlock_GetValues(tBlock *Block, int *nValues)
{
	tExecUnit *Unit = Block->Unit;
	tDispValue	**vals;
	 int	nGroups = 0;

	for(tGroupDef *grp = Unit->Groups; grp; grp = grp->Next)
	{
		 int	nUsed = 0;
		// Check if a line from this group was used
		// - If so, set flag and exclude line from single check
		for(int i = 0; i < grp->Size; i ++) {
			if( grp->Links[i]->Value->Info ) {
				grp->Links[i]->Value->Info = (void*)-1;
				nUsed ++;
			}
		}
		// No links from group used
		if( nUsed == 0 )
			continue ;
		nGroups ++;
	}
	gValue_Zero.Info = (void*)-1;	nGroups ++;
	gValue_One.Info = (void*)-1;	nGroups ++;

	// Turn multiple anon links into groups
	// - Iterate elements and subunits looking for anon link outputs
	//  > Throw an assert if named and anon links are used in the same block.
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		if( ele->Block != Block )
			continue;
		if( ele->NOutputs == 0 || ele->Outputs[0]->Name[0] != '\0' )
			continue;
		//printf(">> %s{?}\n", ele->Type->Name);
		for( int i = 0; i < ele->NOutputs; i ++ ) {
			tLink	*link = ele->Outputs[i];
			tLinkValue	*val = link->Value;
			// NOTE: Assertions hold when cct is loaded from source
			// - Source does not allow mixing named and anon outputs
			assert(link->Name[0] == '\0');
			assert(val->FirstLink == link);
			assert(link->ValueNext == NULL);
			val->Info = (void*)-1;
		}
		nGroups ++;
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		if( subu->Block != Block )
			continue;
		if( subu->Outputs.NItems == 0 || subu->Outputs.Items[0]->Name[0] != '\0' )
			continue;
		//printf(">> %s\n", subu->Def->Name);
		for( int i = 0; i < subu->Outputs.NItems; i ++ ) {
			tLink	*link = subu->Outputs.Items[i];
			tLinkValue	*val = link->Value;
			val->Info = (void*)-1;
		}
		nGroups ++;
	}

	// Look for single-element groups (i.e. Lines)
	// - Count used single links
	// - Set index
	for(tLinkValue *val = Unit->Values; val; val = val->Next)
	{
		if( !val->Info )
			continue ;
		if( val->Info == (void*)-1 )
			continue ;
		
		
		val->Info = (void*)(intptr_t)(nGroups+1);
		nGroups ++;
	}
	
	vals = calloc(sizeof(tDispValue*),nGroups);
	
	// Create display value descriptors for groups
	// - Setting indexes too
	 int	grpnum = 0;
	for(tGroupDef *grp = Unit->Groups; grp; grp = grp->Next)
	{
		 int	nUsed = 0;
		for(int i = 0; i < grp->Size; i ++) {
			if( grp->Links[i]->Value->Info ) {
				grp->Links[i]->Value->Info = (void*)(intptr_t)(grpnum+1);
				nUsed ++;
			}
		}
		// No links from group used
		if( nUsed == 0 )
			continue ;
		// Create description
		vals[grpnum] = Render_int_PrerenderBlock_MakeVal(grp->Size, grp->Name);
		for(int i = 0; i < grp->Size; i ++)
			vals[grpnum]->Values[i] = grp->Links[i]->Value;
		grpnum ++;
	}
	vals[grpnum] = Render_int_PrerenderBlock_MakeVal(1, "0");
	vals[grpnum]->Values[0] = &gValue_Zero;
	gValue_Zero.Info = (void*)(intptr_t)(grpnum+1);	grpnum ++;
	vals[grpnum] = Render_int_PrerenderBlock_MakeVal(1, "1");
	vals[grpnum]->Values[0] = &gValue_One;
	gValue_One.Info = (void*)(intptr_t)(grpnum+1);	grpnum ++;
	// - Anon groups
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		if( ele->Block != Block )
			continue;
		if( ele->NOutputs == 0 || ele->Outputs[0]->Name[0] != '\0' )
			continue;
		
		vals[grpnum] = Render_int_PrerenderBlock_MakeVal(ele->NOutputs, "");
		for( int i = 0; i < ele->NOutputs; i ++ ) {
			tLinkValue *val = ele->Outputs[i]->Value;
			val->Info = (void*)(intptr_t)(grpnum+1);
			vals[grpnum]->Values[i] = val;
		}
		grpnum ++;
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		if( subu->Block != Block )
			continue;
		if( subu->Outputs.NItems == 0 || subu->Outputs.Items[0]->Name[0] != '\0' )
			continue;
		vals[grpnum] = Render_int_PrerenderBlock_MakeVal(subu->Outputs.NItems, "");
		for( int i = 0; i < subu->Outputs.NItems; i ++ ) {
			tLink	*link = subu->Outputs.Items[i];
			tLinkValue	*val = link->Value;
			val->Info = (void*)(intptr_t)(grpnum+1);
			vals[grpnum]->Values[i] = val;
		}
		grpnum ++;
	}
	
	// Single links
	for(tLinkValue *val = Unit->Values; val; val = val->Next)
	{
		if( !val->Info )
			continue ;
		 int ofs = (intptr_t)val->Info - 1;
		assert(0 <= ofs && ofs < nGroups);
		if( vals[ofs] )
			continue ;
		
		// Determine name to use
		const char *prefname = NULL;
		for(tLink *link = val->FirstLink; link; link = link->ValueNext) {
			if( link->Name[0] == '$' )
				prefname = link->Name;
		}
		assert(prefname != NULL);
		
		// Allocate
		vals[ofs] = Render_int_PrerenderBlock_MakeVal(1, prefname);
		vals[ofs]->Values[0] = val;
	}
//	gValue_One && gValue_Zero;
	
	*nValues = nGroups;
	return vals;
}

static inline int MAX(int a, int b)
{
	return (a > b) ? a : b;
}

static inline int MIN(int a, int b)
{
	return (a < b) ? a : b;
}

tDispElement *Render_int_MakeDispEle(tDispInfo *Info, const char *Name, int NParams, int *Params,
	int NInputs, tLink * const *Inputs, int NOutputs, tLink * const *Outputs)
{
	tDispElement	*dispele;

	 int	namelen = strlen(Name);
	if( Params )
	{
		namelen ++;	// opening brace
		for( int i = 0; i < NParams; i ++ ) {
			namelen += snprintf(NULL, 0, "%i", Params[i]);
			if( i != NParams-1 )
				namelen ++;
		}
		namelen ++;	// closing brace
	}

	dispele = malloc( sizeof(tDispElement) + namelen + 1);
	assert(dispele);
	dispele->Name = (void*)(dispele + 1);
	
	// Displayed name of element
	namelen = sprintf(dispele->Name, "%s", Name);
	if( Params )
	{
		dispele->Name[namelen++] = '{';
		for( int i = 0; i < NParams; i ++ ) {
			namelen += sprintf(dispele->Name + namelen, "%i", Params[i]);
			if( i != NParams-1 )
				dispele->Name[namelen ++] = ',';
		}
		dispele->Name[namelen++] = '}';
	}
	dispele->Name[namelen] = '\0';

	// Inputs	
	dispele->NInputs = Render_int_ListToRefs(Info, NULL, NInputs, Inputs);
	dispele->Inputs = malloc( dispele->NInputs * sizeof(tDispValRef) );
	Render_int_ListToRefs(Info, dispele->Inputs, NInputs, Inputs);
	for( int i = 0; i < dispele->NInputs; i ++ )
		dispele->Inputs[i].Value->NRead ++;
	
	// Outputs
	dispele->NOutputs = Render_int_ListToRefs(Info, NULL, NOutputs, Outputs);
	dispele->Outputs = malloc( dispele->NOutputs * sizeof(tDispValRef) );
	Render_int_ListToRefs(Info, dispele->Outputs, NOutputs, Outputs);
	for( int i = 0; i < dispele->NOutputs; i ++ )
		dispele->Outputs[i].Value->NWrite ++;

	// Dimensions and type
	 int	bDontCenter = 0;
	dispele->W = 0;
	dispele->H = 0;
	if( strcmp(Name, "AND") == 0 ) {
		dispele->Type = DISPELE_AND;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "NAND") == 0 ) {
		dispele->Type = DISPELE_AND;
		dispele->bInvertOut = 1;
	}
	else if( strcmp(Name, "OR") == 0 ) {
		dispele->Type = DISPELE_OR;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "NOR") == 0 ) {
		dispele->Type = DISPELE_OR;
		dispele->bInvertOut = 1;
	}
	else if( strcmp(Name, "XOR") == 0 ) {
		dispele->Type = DISPELE_XOR;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "XNOR") == 0 || strcmp(Name, "NXOR") == 0 ) {
		dispele->Type = DISPELE_XOR;
		dispele->bInvertOut = 1;
	}
	else if( strcmp(Name, "DELAY") == 0 ) {
		dispele->Type = DISPELE_DELAY;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "NOT") == 0 ) {
		dispele->Type = DISPELE_DELAY;
		dispele->bInvertOut = 1;
	}
	else if( strcmp(Name, "PULSE") == 0 ) {
		dispele->Type = DISPELE_PULSE;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "HOLD") == 0 ) {
		dispele->Type = DISPELE_HOLD;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "MUX") == 0 ) {
		dispele->Type = DISPELE_MUX;
		dispele->bInvertOut = 0;
	}
	else if( strcmp(Name, "DEMUX") == 0 ) {
		dispele->Type = DISPELE_DEMUX;
		dispele->bInvertOut = 0;
	}
	else {
		dispele->Type = DISPELE_GENERIC;
		dispele->bInvertOut = 0;
		dispele->W = 2;
		bDontCenter = 1;
	}
	if( dispele->W == 0 )
		dispele->W = 1;
	if( dispele->H == 0 ) {
		int	subu_size = MAX(dispele->NInputs, dispele->NOutputs) + 3;
		dispele->H = (subu_size * ciSubunitYSize + ciUnitYSize-1) / ciUnitYSize;
	}
	dispele->X = 0;
	dispele->Y = 0;

	switch( dispele->Type )
	{
	case DISPELE_DEMUX:
	case DISPELE_MUX:
		if( dispele->H == 1 )
			dispele->H = 2;
		break;
	default:
		break;
	}

	if( !bDontCenter )
	{
		 int	ofs;
		printf("%s:\n", dispele->Name);
		ofs = (dispele->H*ciUnitYSize - dispele->NInputs*ciSubunitYSize) / (2 * ciSubunitYSize);
		for( int i = 0; i < dispele->NInputs; i ++ ) {
			tDispValRef	*vr = &dispele->Inputs[i];
			printf(" %s[%i:%i]\n", vr->Value->Name, vr->First, vr->Last);
			dispele->Inputs[i].Ofs = ofs + i;
		}
		ofs = (dispele->H*ciUnitYSize - dispele->NOutputs*ciSubunitYSize) / (2 * ciSubunitYSize);
		for( int i = 0; i < dispele->NOutputs; i ++ ) {
			dispele->Outputs[i].Ofs = ofs + i;
		}
	}

	return dispele;
}

#define _dolayout(code)	do { \
	for( int eleidx = 0; eleidx < DispInfo->nElements; eleidx ++ ) { \
		tDispElement	*de = DispInfo->Elements[eleidx]; \
		{code}\
	}\
	for( tBlock *child = Block->SubBlocks; child; child = child->Next ) {\
		tDispInfo	*de = child->DispInfo; \
		{code}\
	}\
} while(0)

void Render_int_PrerenderBlock_LayoutDiagonal(tBlock *Block, tDispInfo *DispInfo, int x, int y)
{
	_dolayout(
		if( de->X || de->Y )
			continue ;
		de->X = x;
		de->Y = y;
		
		x += (de->W+1)/2;
		y += de->H;
	);
}

/**
 * \brief Set the offsets of each block output
 */
void Render_int_PrerenderBlock_SetBlockLinePos(tBlock *Block, int Flag, int NSrc, tDispValRef *SrcRefs, int NDst, tDispValRef *DstRefs, tDispValue **DstVals, int BaseY)
{
	for( int i = 0; i < NSrc; i ++ )
	{
		//printf(" %p (%s): %o\n", SrcRefs[i].Value, SrcRefs[i].Value->Name, SrcRefs[i].Value->UsageBits);
		if( !(SrcRefs[i].Value->UsageBits & Flag) )
			continue ;
		for(int j = 0; j < NDst; j ++ )
		{
			if( DstVals[j] == SrcRefs[i].Value ) {
				if( DstRefs[j].Ofs != 0 ) {
					printf("%s:%o:%i %s already at %i\n", Block->Name, Flag, j,
						DstVals[j]->Name,
						DstRefs[j].Ofs);
					continue ;
				}
				//printf("%p (%s) ?== %p (%s)\n", DstRefs[j].Value, DstRefs[j].Value->Name,
				//	SrcRefs[i].Value, SrcRefs[i].Value->Name);
				DstRefs[j].Ofs = BaseY * ciUnitYSize / ciSubunitYSize + SrcRefs[i].Ofs;
				//printf("%s: %i\n", DstRefs[j].Value->Name, DstRefs[j].Ofs);
				break;
			}
		}
	}
}
#define LAYOUTTREE_BLOCKOUTS()	Render_int_PrerenderBlock_SetBlockLinePos(Block, UB_OUTPUT, \
			de->NOutputs, de->Outputs, \
			DispInfo->NOutputs, DispInfo->Outputs, DispInfo->LocalOutputVals, \
			de->Y)
#define LAYOUTTREE_BLOCKINS()	Render_int_PrerenderBlock_SetBlockLinePos(Block, UB_INPUT, \
			de->NInputs, de->Inputs, \
			DispInfo->NInputs, DispInfo->Inputs, DispInfo->LocalInputVals, \
			de->Y)

int Render_int_PrerenderBlock_LayoutTree_AnonOutput(tBlock *Block, tDispInfo *DispInfo, tDispValue *OutVal, int RX, int TY, int *MinX)
{
	_dolayout(
		if(de->X != 0)	continue;
		 int	bFoundVal = 0;
		for( int i = 0; i < de->NOutputs; i ++ ) {
			if( de->Outputs[i].Value == &gDispValueConst )
				continue ;
			if( de->Outputs[i].Value != OutVal ) {
				bFoundVal = 0;
				break ;
			}
			bFoundVal ++;
		}
		if( bFoundVal == 0 )
			continue;
		assert(bFoundVal == 1);
		
		de->X = RX - de->W;
		de->Y = TY;
		*MinX = MIN(*MinX, de->X);

		LAYOUTTREE_BLOCKINS();
		//return de->H;
		printf("%p %s-%s: Y=%i\n", de, Block->Name, de->Name, TY);
		int sy = Render_int_PrerenderBlock_LayoutTree_ProcessSingleInputs(Block, de->NInputs, de->Inputs,
			de->X - 1, de->Y, MinX);
		printf("%p %s-%s: ly=%i,sy=%i\n", de, Block->Name, de->Name, de->Y+de->H, sy);
		return MAX(sy, de->Y + de->H);
	);
	return TY;
}

int Render_int_PrerenderBlock_LayoutTree_ProcessSingleInputs(tBlock *Block, int NInputs, const tDispValRef *Inputs, int BaseX, int BaseY, int *MinX)
{
	 int	dinput_y = BaseY;
	for( int i = 0; i < NInputs; i ++ ) {
		tDispValue	*val = Inputs[i].Value;
		//if( !(val->NWrite == 1 /*&& val->NRead == 1*/) )
		if( !(val->NWrite == 1 && val->NRead == 1) )
			continue ;
		printf("%p[%i]: dinput_y = %i\n", Inputs, i, dinput_y);
		dinput_y += Render_int_PrerenderBlock_LayoutTree_AnonOutput(
			Block, Block->DispInfo, val,
			BaseX, dinput_y, MinX) - dinput_y;
	}
	return dinput_y;
}

void Render_int_PrerenderBlock_LayoutTree(tBlock *Block, tDispInfo *DispInfo)
{
	 int	x = -1;
	 int	min_x = 0;
	 int	out_y = 1;
	 int	neles = 0;
	// 1. Directly connected to output
	// 1a. All are outputs and inputs
	_dolayout(
		neles ++;
		
		 int	nDirectOut = 0;
		for( int i = 0; i < de->NOutputs; i ++ ) {
			if( de->Outputs[i].Value->UsageBits & UB_OUTPUT )
				nDirectOut ++;
		}
		if( nDirectOut != de->NOutputs )
			continue ;
		 int	nDirectIn = 0;
		for( int i = 0; i < de->NInputs; i ++ ) {
			if( de->Inputs[i].Value->UsageBits & UB_INPUT )
				nDirectIn ++;
		}
		if( nDirectIn != de->NInputs )
			continue ;
		de->X = x - de->W;
		de->Y = out_y;
		out_y += de->H;
		min_x = MIN(min_x, de->X);
	
		// Set block output offsets
		LAYOUTTREE_BLOCKOUTS();
		LAYOUTTREE_BLOCKINS();
	);
	// 1b. All are outputs
	_dolayout(
		if( de->X != 0 )	continue;
		 int	nDirectOut = 0;
		for( int i = 0; i < de->NOutputs; i ++ ) {
			if( de->Outputs[i].Value->UsageBits & UB_OUTPUT )
				nDirectOut ++;
		}
		if( nDirectOut != de->NOutputs )
			continue ;
		de->X = x - de->W;
		de->Y = out_y;
		out_y += de->H;
		min_x = MIN(min_x, de->X);
		
		// Force anon inputs to be placed to left of element
		int dinput_y = Render_int_PrerenderBlock_LayoutTree_ProcessSingleInputs(
			Block, de->NInputs, de->Inputs, x - de->W - 1, de->Y, &min_x);
		out_y = MAX(out_y, dinput_y);
	
		// Set block output offsets
		LAYOUTTREE_BLOCKOUTS();
		LAYOUTTREE_BLOCKINS();
	);
	x = min_x-1;
	out_y = 1;
	#if 1
	// 1c. At least one is an output
	_dolayout(
		if( de->X != 0 )	continue;
		 int	nDirectOut = 0;
		for( int i = 0; i < de->NOutputs; i ++ ) {
			if( de->Outputs[i].Value->UsageBits & UB_OUTPUT )
				nDirectOut ++;
		}
		if( nDirectOut == 0 )
			continue ;
		de->X = x - de->W;
		de->Y = out_y;
		out_y += de->H;
		min_x = MIN(min_x, de->X);
		
		// Single-output elements serving as inputs
		int dinput_y = Render_int_PrerenderBlock_LayoutTree_ProcessSingleInputs(
			Block, de->NInputs, de->Inputs, x - de->W - 1, de->Y, &min_x);
		out_y = MAX(out_y, dinput_y);

		// Set block output offsets
		LAYOUTTREE_BLOCKOUTS();
		LAYOUTTREE_BLOCKINS();
	);
	#endif
	
	// 2. Directly connected to input (change start X for Diagonal)
	x = 1;
	 int	in_y = 1;
	 int	maxx = 0;
	// 2a. All inputs
	_dolayout(
		if( de->X != 0 )	continue;
		 int	nDirectIn = 0;
		for( int i = 0; i < de->NInputs; i ++ ) {
			if( de->Inputs[i].Value->UsageBits & UB_INPUT )
				nDirectIn ++;
		}
		if( nDirectIn != de->NInputs )
			continue ;
		de->X = x;
		de->Y = in_y;
		in_y += de->H;
		maxx = MAX(maxx, x + de->W);
		LAYOUTTREE_BLOCKOUTS();
		LAYOUTTREE_BLOCKINS();
	);
	
	// 2b. At least one input
	// - Create a bitmap of what elements are not yet positioned
	bool upos_bitmap[neles];
	neles = 0;
	_dolayout(
		upos_bitmap[neles++] = (de->X == 0);
	);
	// - Run this with a base X of -1, processing single inputs
	x = -1;
	in_y = 1;
	min_x = -1;
	_dolayout(
		if( de->X )	continue;
		 int	nDirectIn = 0;
		for( int i = 0; i < de->NInputs; i ++ ) {
			if( de->Inputs[i].Value->UsageBits & UB_INPUT )
				nDirectIn ++;
		}
		if( nDirectIn == 0 )
			continue ;
		de->X = x;
		de->Y = in_y;
		in_y += de->H;

		// Single-output elements serving as inputs
		int dinput_y = Render_int_PrerenderBlock_LayoutTree_ProcessSingleInputs(
			Block, de->NInputs, de->Inputs, x - 1, de->Y, &min_x);
		in_y = MAX(in_y, dinput_y);

		LAYOUTTREE_BLOCKOUTS();
		LAYOUTTREE_BLOCKINS();
	);
	// - Re-parse bitmap and adjust all values by -minx
	neles = 0;
	_dolayout(
		if(de->X && upos_bitmap[neles])
		{
			de->X = (maxx + 1 + -min_x) - (-de->X + de->W) + 1;
		}
		neles ++;
	);

	// ? Second indirect?	
	
	// Final. Internals done using diagonal layout
	Render_int_PrerenderBlock_LayoutDiagonal(Block, DispInfo, maxx+1, 1);
}

void Render_int_PrerenderBlock_Layout_FixNegs(tBlock *Block, tDispInfo *DispInfo)
{
	int min_neg_x = 0, min_neg_y = 0;
	int max_pos_x = 0, max_pos_y = 0;

	_dolayout(
		if(de->X < 0)
			min_neg_x = MIN(min_neg_x, de->X);
		else
			max_pos_x = MAX(max_pos_x, de->X + de->W);
		
		if(de->Y < 0)
			min_neg_y = MIN(min_neg_y, de->Y);
		else
			max_pos_y = MAX(max_pos_y, de->Y + de->H);
	);
	
	// TODO: Handle cases where elements from opposite sides will fit together
	
	int w = max_pos_x + (-min_neg_x) + 1;
	int h = max_pos_y + (-min_neg_y) + 1;
	
	_dolayout(
		if(de->X < 0)
			de->X = w - (-de->X);
		if(de->Y < 0)
			de->Y = h - (-de->Y);
	);
	
	DispInfo->W = w;
	DispInfo->H = h;
	
	printf("%ix%i: '%s'\n", DispInfo->W, DispInfo->H, Block->Name);
}

void Render_int_PrerenderBlock(tExecUnit *Unit, tBlock *Block)
{
	// Early return if already processed
	if( Block->DispInfo )
		return ;

	// Nuke value pointers
	for( tLinkValue *val = Unit->Values; val; val = val->Next )
		val->Info = NULL;
	gValue_One.Info = NULL;
	gValue_Zero.Info = NULL;

	// 1. Make list of used values in this block
	//  - Count elements while we're at it
	 int	numElements = 0;
	#define _fv(_val)	do{tLinkValue*val=_val; if(!val->Info) val->Info=(void*)(intptr_t)(1);}while(0)
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		if( ele->Block != Block )
			continue;
		numElements ++;
		for( int i = 0; i < ele->NInputs; i ++ )
			_fv(ele->Inputs[i]->Value);
		for( int i = 0; i < ele->NOutputs; i ++ )
			_fv(ele->Outputs[i]->Value);
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		if( subu->Block != Block )
			continue;
		numElements ++;
		for( int i = 0; i < subu->Inputs.NItems; i ++ )
			_fv(subu->Inputs.Items[i]->Value);
		for( int i = 0; i < subu->Outputs.NItems; i ++ )
			_fv(subu->Outputs.Items[i]->Value);
	}
	for( tBlock *child = Block->SubBlocks; child; child = child->Next )
	{
		for( int i = 0; i < child->DispInfo->NInputs; i ++ ) {
			tDispValue	*dv = child->DispInfo->LocalInputVals[i];
			for( int j = 0; j < dv->NumLines; j ++ )
				_fv(dv->Values[j]);
		}
		for( int i = 0; i < child->DispInfo->NOutputs; i ++ ) {
			tDispValue	*dv = child->DispInfo->LocalOutputVals[i];
			for( int j = 0; j < dv->NumLines; j ++ )
				_fv(dv->Values[j]);
		}
	}
	#undef _fv

	// -- Build up list of 'local' lines --
	 int	nGroups = 0;
	tDispValue **vals = Render_int_PrerenderBlock_GetValues(Block, &nGroups);

	// 2. Find all input / output lines (by usage)
	// 3. Locate rw lines that are used outside
	// 4. Locate lines that are double-set/read
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		 int	shift = 0;
		
		if( ele->Block != Block ) {
			// Check if the element is in a sub-block of this
			tBlock	*b = ele->Block->Parent;
			while(b && b != Block)
				b = b->Parent;
			// If so, ignore it
			if( b )
				continue ;
			// Referenced outside of subblocks
			shift = 2;
		}
		for( int i = 0; i < ele->NInputs; i ++ )
			int_Prerender_MarkVal(nGroups,vals, ele->Inputs[i]->Value, UB_READ<<shift);
		for( int i = 0; i < ele->NOutputs; i ++ )
			int_Prerender_MarkVal(nGroups,vals, ele->Outputs[i]->Value, UB_WRITE<<shift);
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		 int	shift = 0;
		if( subu->Block != Block ) {
			// Check if the element is in a sub-block of this
			tBlock	*b = subu->Block->Parent;
			while(b && b != Block)
				b = b->Parent;
			// If so, ignore it
			if( b )
				continue ;
			// Referenced outside of subblocks
			shift = 2;
		}
		for( int i = 0; i < subu->Inputs.NItems; i ++ )
			int_Prerender_MarkVal(nGroups,vals, subu->Inputs.Items[i]->Value, UB_READ<<shift);
		for( int i = 0; i < subu->Outputs.NItems; i ++ )
			int_Prerender_MarkVal(nGroups,vals, subu->Outputs.Items[i]->Value, UB_WRITE<<shift);
	}
	for( tBlock *child = Block->SubBlocks; child; child = child->Next )
	{
		tDispInfo	*cdi = child->DispInfo;
		for( int i = 0; i < cdi->NInputs; i ++ ) {
			tDispValue	*dv = cdi->LocalInputVals[i];
			for( int j = 0; j < dv->NumLines; j ++ )
				int_Prerender_MarkVal(nGroups,vals, dv->Values[j], UB_READ);
			// Clobber input value pointer, I feel bad
			cdi->Inputs[i].Value = vals[ (intptr_t)dv->Values[0]->Info - 1 ]; 
			cdi->Inputs[i].Value->NRead ++;
		}
		for( int i = 0; i < cdi->NOutputs; i ++ ) {
			tDispValue	*dv = cdi->LocalOutputVals[i];
			for( int j = 0; j < dv->NumLines; j ++ )
				int_Prerender_MarkVal(nGroups,vals, dv->Values[j], UB_WRITE);
			cdi->Outputs[i].Value = vals[ (intptr_t)dv->Values[0]->Info - 1 ]; 
			cdi->Outputs[i].Value->NWrite ++;
		}
	}
	for( int i = 0; i < Unit->Inputs.NItems; i ++ )
		int_Prerender_MarkVal(nGroups,vals, Unit->Inputs.Items[i]->Value, UB_EXTWRITE);
	for( int i = 0; i < Unit->Outputs.NItems; i ++ )
		int_Prerender_MarkVal(nGroups,vals, Unit->Outputs.Items[i]->Value, UB_EXTREAD);
	// - Count inputs/outputs
	 int	nInputs = 0;
	 int	nOutputs = 0;
	for( int i = 0; i < nGroups; i ++ )
	{
		int bits = vals[i]->UsageBits;
		
		if( vals[i]->Values[0] == &gValue_One )
			continue ;
		if( vals[i]->Values[0] == &gValue_Zero )
			continue ;
		
		// Read locally and written outside : Input
		if( (bits & UB_READ) && (bits & UB_EXTWRITE) ) {
			nInputs ++;
		}
		// Written locally and read outside : Output
		if( (bits & UB_WRITE) && (bits & UB_EXTREAD) ) {
			nOutputs ++;
		}
	}
	
	// Create rendered description
	tDispInfo	*dispinfo;
	dispinfo = malloc(sizeof(tDispInfo) + (nInputs + nOutputs)*(sizeof(tDispValRef)+sizeof(tDispValue*)));
	dispinfo->NInputs = nInputs;
	dispinfo->Inputs = (void*)(dispinfo + 1);
	dispinfo->NOutputs = nOutputs;
	dispinfo->Outputs = dispinfo->Inputs + nInputs;
	dispinfo->LocalInputVals = (void*)(dispinfo->Outputs + nOutputs);
	dispinfo->LocalOutputVals = dispinfo->LocalInputVals + nInputs;
	dispinfo->nValues = nGroups;
	dispinfo->Values = vals;
	dispinfo->X = 0;
	dispinfo->Y = 0;
	Block->DispInfo = dispinfo;

	// - Populate inputs/outputs
	 int	in_ofs = 0, out_ofs = 0;
	for( int i = 0; i < nGroups; i ++ )
	{
		int bits = vals[i]->UsageBits;
		if( vals[i]->Values[0] == &gValue_One )
			continue ;
		if( vals[i]->Values[0] == &gValue_Zero )
			continue ;
		
		// Read locally and written outside : Input
		if( (bits & UB_READ) && (bits & UB_EXTWRITE) ) {
			dispinfo->Inputs[in_ofs].Value = NULL;
			dispinfo->Inputs[in_ofs].First = 0;
			dispinfo->Inputs[in_ofs].Last = vals[i]->NumLines-1;
			dispinfo->Inputs[in_ofs].Ofs = 0;
			dispinfo->LocalInputVals[in_ofs] = vals[i];
			vals[i]->UsageBits |= UB_INPUT;
			vals[i]->NWrite ++;
			in_ofs ++;
		}
		// Written locally and read outside : Output
		if( (bits & UB_WRITE) && (bits & UB_EXTREAD) ) {
			dispinfo->Outputs[out_ofs].Value = NULL;
			dispinfo->Outputs[out_ofs].First = 0;
			dispinfo->Outputs[out_ofs].Last = vals[i]->NumLines-1;
			dispinfo->Outputs[out_ofs].Ofs = 0;
			dispinfo->LocalOutputVals[out_ofs] = vals[i];
			vals[i]->UsageBits |= UB_OUTPUT;
			vals[i]->NRead ++;
			out_ofs ++;
		}
	}

	// 0. Convert elements into a compacted form
	//  - Line groups as one 'link'
	//  - Convert blocks of 0/1 into an constant source
	//  - ?Prefixed NOT elements as a dot
	//  - Handle assignment to '$NULL' and convert to unconnected output
	dispinfo->nElements = numElements;
	dispinfo->Elements = malloc( numElements * sizeof(tDispElement*) );
	 int	eleidx = 0;
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		if( ele->Block != Block )
			continue ;
		dispinfo->Elements[eleidx] =
			Render_int_MakeDispEle(dispinfo, ele->Type->Name, ele->NParams, ele->Params,
				ele->NInputs, ele->Inputs, ele->NOutputs, ele->Outputs);
		eleidx ++;
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		if( subu->Block != Block )
			continue ;
		dispinfo->Elements[eleidx] =
			Render_int_MakeDispEle(dispinfo, subu->Def->Name, 0, NULL,
				subu->Inputs.NItems,  subu->Inputs.Items,
				subu->Outputs.NItems, subu->Outputs.Items);
		eleidx ++;
	}
	assert(eleidx == numElements);

	// (HACK) Eliminate elements that have entirely unused outputs
	for( int i = 0; i < dispinfo->nElements; i ++ )
	{
		tDispElement	*de = dispinfo->Elements[i];
		 int	nUnread = 0;
		for( int j = 0; j < de->NOutputs; j ++ )
		{
			if( !(de->Outputs[j].Value->UsageBits & (UB_READ|UB_OUTPUT)) )
				nUnread ++;
		}
		if( de->NOutputs == nUnread )
		{
			printf("Eliminated %s with %i/%i outputs unread\n",
				de->Name, nUnread, de->NOutputs);
			for( int j = 0; j < de->NInputs; j ++ )
				de->Inputs[j].Value->NRead --;
			// TODO: Decrement read count of inputs
			free(de);
			memmove(&dispinfo->Elements[i], &dispinfo->Elements[i+1], (eleidx - (i+1))*sizeof(tDispElement*));
			i --;
			dispinfo->nElements --;
		}
	}

	// DEBUG!
	#if 1
	printf("%i values\n", dispinfo->nValues);
	for( int i = 0; i < dispinfo->nValues; i ++ ) {
		printf("- %s[%i] (0%o) w%i:r%i\n",
			dispinfo->Values[i]->Name,
			dispinfo->Values[i]->NumLines,
			dispinfo->Values[i]->UsageBits,
			dispinfo->Values[i]->NWrite,
			dispinfo->Values[i]->NRead
			);
	}
	printf("%i elements\n", dispinfo->nElements);
	for( int i = 0; i < dispinfo->nElements; i ++ ) {
		printf("- %s\n", dispinfo->Elements[i]->Name);
	}
	#endif
	// /DEBUG

	// -- Layout --
	//Render_int_PrerenderBlock_LayoutDiagonal(Block, dispinfo, 1, 1);
	Render_int_PrerenderBlock_LayoutTree(Block, dispinfo);
	Render_int_PrerenderBlock_Layout_FixNegs(Block, dispinfo);

	// Reposition inputs/outputs to shorten distance from set
	for( int i = 0; i < dispinfo->NInputs; i ++ )
	{
		tDispInfo	*DispInfo = dispinfo;
		tDispValRef	*ref = &dispinfo->Inputs[i];
		if( ref->Ofs != 0 )	continue;

		int minofs = INT_MAX;

		_dolayout(
			for( int j = 0; j < de->NInputs; j ++ ) {
				if( dispinfo->LocalInputVals[i] == de->Inputs[j].Value )
					minofs = MIN(de->Y*ciUnitYSize/ciSubunitYSize + de->Inputs[j].Ofs, minofs);
			}
		);

		ref->Ofs = minofs;
	}
	for( int i = 0; i < dispinfo->NOutputs; i ++ )
	{
		tDispValRef	*ref = &dispinfo->Outputs[i];
		if( ref->Ofs != 0 )	continue;
		ref->Ofs = i+1;
		printf("Output %s:%i defaulted to %i\n", Block->Name, i, ref->Ofs);
	}

	// Route lines around elements:
	// - Prefer vertical junctions
	// TODO: Line routing
}

void int_memsetc(uint32_t *dst, int w, uint32_t colour)
{
	while(w --)
		*dst++ = colour;
}

void Render_int_FillRect(void *Surface, int X, int Y, int W, int H, uint32_t Colour)
{
	if( *(int*)Surface == 0 )
	{
		tRasterSurface	*srf = Surface;
		assert(X+W <= srf->W);
		assert(Y+H <= srf->H);
		
		uint32_t	*dst = srf->Data + Y*srf->W + X;
		for(int row = 0; row < H; row ++)
		{
			int_memsetc(dst, W, Colour);
			dst += srf->W;
		}
	}
	else
	{
		assert( *(int*)Surface == 0 );
	}
}

void Render_int_DrawRect(void *Surface, int X, int Y, int W, int H, uint32_t Colour)
{
	if( *(int*)Surface == 0 )
	{
		tRasterSurface	*srf = Surface;
		if( !(X+W <= srf->W) ) {
			fprintf(stderr, "Render_int_DrawRect: X(%i)+W(%i) > srf->W(%i) [%p]\n",
				X, W, srf->W, __builtin_return_address(0));
			exit(1);
		}
		assert(Y+H <= srf->H);
		assert(W); assert(H);
		
		//if( X + W > srf->W )	W = srf->W - X;
		//if( Y + H > srf->H )	H = srf->H - Y;
		
		uint32_t	*dst = srf->Data + Y*srf->W + X;
		int_memsetc(dst, W, Colour);
		if(H == 1)	return;
		dst += srf->W;
		for(int row = 1; row < H-1; row ++)
		{
			dst[0] = Colour;
			dst[W-1] = Colour;
			dst += srf->W;
		}
		int_memsetc(dst, W, Colour);
	}
	else
	{
		assert( *(int*)Surface == 0 );
	}
}

//static inline int abs(int a) { return (a < 0) ? -a : a; }

void Render_int_DrawLine(void *Surface, int X, int Y, int W, int H, uint32_t Colour)
{
	if( *(int*)Surface == 0 )
	{
		tRasterSurface	*srf = Surface;

		assert(X+W >= 0);
		assert(X+W <= srf->W);
		assert(Y+H >= 0);
		assert(Y+H <= srf->H);
		
		if( abs(W) > abs(H) )
		{
			if(W < 0) {
				X += W;
				Y += H;
				W = -W;
				H = -H;
			}
			uint32_t	*dst = srf->Data + Y*srf->W + X;
			for(int i = 0; i < W; i ++ )
				dst[i + (i * H / W)*srf->W] = Colour;
		}
		else
		{
			if(H < 0) {
				X += W;
				Y += H;
				W = -W;
				H = -H;
			}
			uint32_t	*dst = srf->Data + Y*srf->W + X;
			for(int i = 0; i < H; i ++ )
				dst[i*srf->W + (i * W / H)] = Colour;
		}
	}
	else
	{
		assert(*(int*)Surface == 0);
	}
}

void Render_int_DrawArch(void *Surface, int X, int Y, int W, int H, uint32_t Colour)
{
	 int	half_h = H/2;
	 int	half_h_2 = half_h*half_h;
	 int	last_x;
	
	for( int i = 0; i <= half_h; i ++ )
	{
		int end_x = W*sqrt(half_h_2 - i*i)/half_h;
		if(i > 0)
		{
			Render_int_DrawLine(Surface,
				X + last_x, Y + (half_h-i),
				end_x - last_x, -1,
				Colour
				);
			Render_int_DrawLine(Surface,
				X + last_x, Y + (half_h+i-1),
				end_x - last_x, 1,
				Colour
				);
		}
		last_x = end_x;
	}
	
}

void Render_int_FillCircle(void *Surface, int X, int Y, int W, int H, uint32_t Colour)
{
	 int	half_h = H/2;
	 int	half_h_2 = half_h*half_h;

	X += W/2;	

	for( int i = 0; i <= half_h; i ++ )
	{
		int end_x = W*sqrt(half_h_2 - i*i)/H;
		Render_int_DrawLine(Surface, X, Y + (half_h-i ),   end_x, -1, Colour );
		Render_int_DrawLine(Surface, X, Y + (half_h+i-1),  end_x,  1, Colour);
		Render_int_DrawLine(Surface, X, Y + (half_h-i ),  -end_x, -1, Colour );
		Render_int_DrawLine(Surface, X, Y + (half_h+i-1), -end_x,  1, Colour );
	}
	
}

void Render_int_RasteriseBlock_DrawLine(void *Surface, const tDispValRef *Ref, const tDispValue *Val, int X, int Y, int DirV, int SubuLen)
{
	 int	bIsBus = (Val->NumLines > 1) && (Ref->First != Ref->Last);
	if( DirV )
	{
		 int	w = (bIsBus ? ciMultiLineWidth : ciSingleLineWidth);
		Render_int_FillRect(Surface,
			X + (ciSubunitXSize - w)/2, Y,
			w, ciSubunitYSize*SubuLen,
			(bIsBus ? COLOUR_BUS : COLOUR_LINE)
			);
	}
	else
	{
		 int	h = (bIsBus ? ciMultiLineWidth : ciSingleLineWidth);
		Render_int_FillRect(Surface,
			X, Y + (ciSubunitYSize-h)/2,
			ciSubunitXSize*SubuLen, h,
			(bIsBus ? COLOUR_BUS : COLOUR_LINE)
			);
	}
}

void Render_int_RasteriseBlock_DrawLink(tBlock *Block, void *Surface, int X, int Y, int DX, int DY, const tDispValue *Val, int bForceSingle)
{
	 int	nLinks = 0;
	tDispInfo	*DispInfo = Block->DispInfo;

	if( Val == &gDispValueConst )
		return ;
	 int	bIsBus = (Val->NumLines > 1) && !bForceSingle;	

	DY += (ciSubunitYSize-1)/2;
	_dolayout(
		tDispValRef	*ref = NULL;
		for( int i = 0; i < de->NOutputs; i ++ ) {
//			printf(" %p %p (%s) == %p\n", de, de->Outputs[i].Value, de->Outputs[i].Value->Name, Val);
			if(de->Outputs[i].Value == Val) {
				ref = &de->Outputs[i];
				break;
			}
		}
		if( !ref )
			continue ;

		 int	sx = X + (de->X+de->W)*ciUnitXSize;
		 int	sy = Y + de->Y*ciUnitYSize + ref->Ofs*ciSubunitYSize;
		
		sy += (ciSubunitYSize-1)/2;
		
		Render_int_DrawLine(Surface, sx, sy, DX - sx, DY - sy, (bIsBus ? COLOUR_BUS : COLOUR_LINE));
		if( bIsBus ) {
			Render_int_DrawLine(Surface, sx, sy-1, DX - sx, DY-sy, COLOUR_BUS);
			Render_int_DrawLine(Surface, sx, sy+1, DX - sx, DY-sy, COLOUR_BUS);
		}
		// TODO: Multi-set lines?
		// - Continue and 
		//break;
		nLinks ++;
	);
	#if 1
	for( int i = 0; i < DispInfo->NInputs; i ++ ) {
		tDispValRef	*ref = &DispInfo->Inputs[i];
		if( DispInfo->LocalInputVals[i] != Val )
			continue ;
		
		 int	sx = X + ciSubunitXSize;
		 int	sy = Y + ref->Ofs * ciSubunitYSize;
		sy += (ciSubunitYSize-1)/2;
		
		Render_int_DrawLine(Surface, sx, sy, DX - sx, DY - sy, (bIsBus ? COLOUR_BUS : COLOUR_LINE));
		if( Val->NumLines > 1 && !bForceSingle ) {
			Render_int_DrawLine(Surface, sx, sy-1, DX - sx, DY-sy, COLOUR_BUS);
			Render_int_DrawLine(Surface, sx, sy+1, DX - sx, DY-sy, COLOUR_BUS);
		}
		nLinks ++;
	}
	#endif
	if( nLinks == 0 ) {
		printf(" > No lines\n");
	}
//	else
//		printf(" > %i lines\n", nLinks);
}

void Render_int_RasteriseBlock(tBlock *Block, void *Surface, int X, int Y, int MaxDepth)
{
	tDispInfo	*DispInfo = Block->DispInfo;
	if( MaxDepth == 0 )
		return ;

	Render_int_DrawRect(Surface, X, Y, DispInfo->W*ciUnitXSize, DispInfo->H*ciUnitYSize, COLOUR_SUBBLOCK);

	for( int eleid = 0; eleid < DispInfo->nElements; eleid ++ )
	{
		tDispElement	*de = DispInfo->Elements[eleid];
		 int	base_x = X + de->X * ciUnitXSize;
		 int	base_y = Y + de->Y * ciUnitYSize;
		switch( de->Type )
		{
		case DISPELE_PULSE:
			// _|-
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+ciSubunitXSize/2,
				base_y+ciSubunitYSize+2*ciSubunitYSize,
				ciSubunitXSize/2, 1,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+ciSubunitXSize,
				base_y+ciSubunitYSize+ciSubunitYSize,
				1, ciSubunitYSize,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+ciSubunitXSize,
				base_y+ciSubunitYSize+ciSubunitYSize,
				ciSubunitXSize/2, 1,
				COLOUR_GATE
				);
			// fall
		case DISPELE_DELAY: {
			 int	h = de->H * ciUnitYSize - 2*ciSubunitYSize;
			 int	half_h = h / 2;
			Render_int_DrawLine(Surface,
				base_x + ciSubunitXSize,
				base_y + ciSubunitYSize,
				1, h,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface, 
				base_x + ciSubunitXSize,
				base_y + ciSubunitYSize,
				de->W * ciUnitXSize - 2*ciSubunitXSize,
				half_h,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface, 
				base_x + ciSubunitXSize,
				base_y + ciSubunitYSize + h,
				de->W * ciUnitXSize - 2*ciSubunitXSize,
				-half_h,
				COLOUR_GATE
				);
			break; }
		case DISPELE_HOLD: {
			// _|-|_
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+ciSubunitXSize/2,
				base_y+ciSubunitYSize+2*ciSubunitYSize,
				ciSubunitXSize/2, 1,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+ciSubunitXSize,
				base_y+ciSubunitYSize+ciSubunitYSize,
				1, ciSubunitYSize,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+ciSubunitXSize,
				base_y+ciSubunitYSize+ciSubunitYSize,
				ciSubunitXSize, 1,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+2*ciSubunitXSize,
				base_y+ciSubunitYSize+ciSubunitYSize,
				1, ciSubunitYSize,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x+ciSubunitXSize+2*ciSubunitXSize,
				base_y+ciSubunitYSize+2*ciSubunitYSize,
				ciSubunitXSize/2, 1,
				COLOUR_GATE
				);
			Render_int_DrawRect(Surface,
				base_x + ciSubunitXSize,
				base_y + ciSubunitYSize,
				de->W * ciUnitXSize - 2*ciSubunitXSize,
				de->H * ciUnitYSize - 2*ciSubunitYSize,
				COLOUR_GATE
				);
			break; }
		case DISPELE_MUX: {
			 int	h = de->H * ciUnitYSize - 2*ciSubunitYSize;
			 int	w = de->W * ciUnitXSize - 2*ciSubunitXSize;
			 int	slope = (de->H > 1 ? ciSubunitXSize*2 : ciSubunitXSize);
			Render_int_DrawLine(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				1, h,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface, 
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				w,
				slope,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x + w + ciSubunitXSize, base_y+ciSubunitYSize+slope,
				1, h-2*slope,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface, 
				base_x + ciSubunitXSize, base_y + ciSubunitYSize + h,
				de->W * ciUnitXSize - 2*ciSubunitXSize,
				-slope,
				COLOUR_GATE
				);
			
			break; }
		case DISPELE_DEMUX: {
			 int	h = de->H * ciUnitYSize - 2*ciSubunitYSize;
			 int	w = de->W * ciUnitXSize - 2*ciSubunitXSize;
			 int	slope = (de->H > 1 ? ciSubunitXSize*2 : ciSubunitXSize);
			Render_int_DrawLine(Surface,
				base_x + ciSubunitXSize + w, base_y + ciSubunitYSize,
				1, h,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface, 
				base_x + ciSubunitXSize + w, base_y + ciSubunitYSize,
				-w,
				slope,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface,
				base_x + ciSubunitXSize, base_y+ciSubunitYSize+slope,
				1, h-2*slope,
				COLOUR_GATE
				);
			Render_int_DrawLine(Surface, 
				base_x + w + ciSubunitXSize, base_y + ciSubunitYSize + h,
				-w,
				-slope,
				COLOUR_GATE
				);
			
			break; }
		case DISPELE_AND: {
			 int	h = de->H * ciUnitYSize - 2*ciSubunitYSize;
			Render_int_DrawLine(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				1, h,
				COLOUR_GATE
				);
			Render_int_DrawArch(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				ciUnitXSize-2*ciSubunitXSize, h,
				COLOUR_GATE
				);
			break; }
		case DISPELE_XOR: {
			 int	h = de->H * ciUnitYSize - 2*ciSubunitYSize;
			Render_int_DrawArch(Surface,
				base_x + ciSubunitXSize/2, base_y + ciSubunitYSize,
				ciSubunitXSize, h,
				COLOUR_GATE
				);
			Render_int_DrawArch(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				ciSubunitXSize, h,
				COLOUR_GATE
				);
			Render_int_DrawArch(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				ciUnitXSize-2*ciSubunitXSize, h,
				COLOUR_GATE
				);
			break; }
		case DISPELE_OR: {
			 int	h = de->H * ciUnitYSize - 2*ciSubunitYSize;
			Render_int_DrawArch(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				ciSubunitXSize, h,
				COLOUR_GATE
				);
			Render_int_DrawArch(Surface,
				base_x + ciSubunitXSize, base_y + ciSubunitYSize,
				ciUnitXSize-2*ciSubunitXSize, h,
				COLOUR_GATE
				);
			break; }
		case DISPELE_GENERIC:
		default:
			Render_int_DrawRect(Surface,
				base_x + ciSubunitXSize,
				base_y + ciSubunitYSize,
				de->W * ciUnitXSize - 2*ciSubunitXSize,
				de->H * ciUnitYSize - 2*ciSubunitYSize,
				COLOUR_GENERIC
				);
			break;
		}
		for( int i = 0; i < de->NInputs; i ++ )
			Render_int_RasteriseBlock_DrawLine(Surface, &de->Inputs[i], de->Inputs[i].Value,
				base_x, base_y + (de->Inputs[i].Ofs)*ciSubunitYSize,
				0, 1
				);
		for( int i = 0; i < de->NOutputs; i ++ ) {
			 int	x = base_x + de->W * ciUnitXSize - ciSubunitXSize;
			 int	y = base_y + (de->Outputs[i].Ofs)*ciSubunitYSize;
			Render_int_RasteriseBlock_DrawLine(Surface, &de->Outputs[i], de->Outputs[i].Value,
				x, y, 0, 1
				);
			if( de->bInvertOut ) {
				Render_int_FillCircle(Surface,
					x - ciSubunitXSize / 2, y, ciSubunitXSize, ciSubunitYSize,
					COLOUR_GATE
					);
			}
		}
	}

	for(tBlock *child = Block->SubBlocks; child; child = child->Next)
	{
		Render_int_RasteriseBlock(child, Surface,
			X + child->DispInfo->X*ciUnitXSize,
			Y + child->DispInfo->Y*ciUnitYSize,
			MaxDepth-1);
	}

	// TODO: Properly routed lines
	// HACK - Direct lines
	#if 1
	_dolayout(
		for( int i = 0; i < de->NInputs; i ++ ) {
			tDispValRef	*ref = &de->Inputs[i];
			Render_int_RasteriseBlock_DrawLink(Block, Surface, X, Y,
				X + de->X*ciUnitXSize, Y + de->Y*ciUnitYSize + ref->Ofs*ciSubunitYSize,
				ref->Value, ref->First == ref->Last
				);
		}
	);
	#endif
	for( int i = 0; i < DispInfo->NOutputs; i ++ ) {
		tDispValRef	*ref = &DispInfo->Outputs[i];
		tDispValue	*val = DispInfo->LocalOutputVals[i];
//		printf("- %s #%i - %p '%s' %i:%i Ofs=%i\n", Block->Name, i, ref->Value, val->Name,
//			val->NWrite, val->NRead, ref->Ofs);
		Render_int_RasteriseBlock_DrawLink(Block, Surface, X, Y,
			X + DispInfo->W*ciUnitXSize-ciSubunitXSize, Y + ref->Ofs*ciSubunitYSize,
			val, ref->First == ref->Last
			);
	}

	for( int i = 0; i < DispInfo->NInputs; i ++ )
		Render_int_RasteriseBlock_DrawLine(Surface, &DispInfo->Inputs[i], DispInfo->LocalInputVals[i],
			X, Y + (DispInfo->Inputs[i].Ofs)*ciSubunitYSize,
			0, 1
			);
	for( int i = 0; i < DispInfo->NOutputs; i ++ )
		Render_int_RasteriseBlock_DrawLine(Surface, &DispInfo->Outputs[i], DispInfo->LocalOutputVals[i],
			X + DispInfo->W*ciUnitXSize-ciSubunitXSize, Y + (DispInfo->Outputs[i].Ofs)*ciSubunitYSize,
			0, 1
			);
}

