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

#define UB_READ 	001
#define UB_WRITE	002
#define UB_EXTREAD	004
#define UB_EXTWRITE	010

enum eDispElementType
{
	DISPELE_GENERIC,
	DISPELE_DELAY,	// NOT = Buffer with inverted output
	DISPELE_PULSE,
	DISPELE_AND,
	DISPELE_OR,
	DISPELE_XOR,
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
};
struct sDispValRef
{
	tDispValue	*Value;
	 int	First, Last;
	 int	Ofs;
};
struct sDispInfo
{
	 int	nElements;
	tDispElement	**Elements;
	 int	nValues;
	tDispValue	**Values;
	 int	nLines;
	tDispLine	**Lines;
	
	 int	NInputs;
	tDispValRef	*Inputs;
	 int	NOutputs;
	tDispValRef	*Outputs;
	
	 int	W, H;
};
struct sDispElement
{
	char	*Name;

	 int	NInputs;
	tDispValRef	*Inputs;
	 int	NOutputs;
	tDispValRef	*Outputs;

	 int	X, Y, W, H;
};

// === CONSTANTS ===
const int	ciUnitXSize = 32;
const int	ciUnitYSize = 32;
const int	ciSubunitXSize = 4;
const int	ciSubunitYSize = 4;
const int	ciSingleLineWidth = 1;
const int	ciMultiLineWidth = 3;

// === PROTOTYPES ===
void	*Render_RenderBlockRaster(tExecUnit *Unit, tBlock *Block, int *W, int *H);
void	Render_int_PrerenderBlockRec(tExecUnit *Unit, tBlock *Block);
void	Render_int_PrerenderBlock(tExecUnit *Unit, tBlock *Block);
void	Render_int_RasteriseBlock(tBlock *Block, void *Surface, int W, int H, int X, int Y, int MaxDepth);

// === CODE ===
void *Render_RenderBlockRaster(tExecUnit *Unit, tBlock *Block, int *W, int *H)
{
	// Ensure all blocks are vectored
	Render_int_PrerenderBlockRec(Unit, Block);
	// Allocate image buffer
	*W = Block->DispInfo->W * ciUnitXSize;
	*H = Block->DispInfo->H * ciUnitYSize;
	// Rasterise blocks (with optional max depth)
	void *ret = malloc( *W * *H * 4);
	Render_int_RasteriseBlock(ret, Block, *W, *H, 0, 0, INT_MAX);
	return ret;
}

void Render_int_PrerenderBlockRec(tExecUnit *Unit, tBlock *Block)
{
	// Bottom-up vectorising of blocks
	// - Ensures that inputs/outputs of child blocks are known when parent is processed
	for( tBlock *child = Block->SubBlocks; child; child = child->Next )
		Render_int_PrerenderBlockRec(Unit, child);
	Render_int_PrerenderBlock(Unit, Block);
}

static inline void int_Prerender_MarkVal(int NumValues, tDispValue **Values, tLinkValue *Value, int Mask)
{
	if( Value->Info )
	{
		int ofs = (intptr_t)Value->Info - 1;
		assert(0 <= ofs && ofs < NumValues);
		assert(Values[ofs]);
		Values[ofs]->UsageBits |= Mask;
	}
}

int Render_int_ListToRefs(tDispValRef *Refs, int NItems, tLink *const*Items)
{
	// TODO: Render_int_ListToRefs
	return 0;
}

tDispElement *Render_int_MakeDispEle(const char *Name, int NParams, int *Params,
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
	dispele->Name[namelen++] = '\0';
	
	dispele->NInputs = Render_int_ListToRefs(NULL, NInputs, Inputs);
	dispele->Inputs = malloc( dispele->NInputs * sizeof(tDispValRef) );
	Render_int_ListToRefs(dispele->Inputs, NInputs, Inputs);
	
	dispele->NOutputs = Render_int_ListToRefs(NULL, NOutputs, Outputs);
	dispele->Outputs = malloc( dispele->NOutputs * sizeof(tDispValRef) );
	Render_int_ListToRefs(dispele->Outputs, NOutputs, Outputs);
	
	return dispele;
}

void Render_int_PrerenderBlock(tExecUnit *Unit, tBlock *Block)
{
	// Early return if already processed
	if( Block->DispInfo )
		return ;

	// Nuke value pointers
	for( tLinkValue *val = Unit->Values; val; val = val->Next )
		val->Info = NULL;

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
			tDispValue	*dv = child->DispInfo->Inputs[i].Value;
			for( int j = 0; j < dv->NumLines; j ++ )
				_fv(dv->Values[j]);
		}
		for( int i = 0; i < child->DispInfo->NOutputs; i ++ ) {
			tDispValue	*dv = child->DispInfo->Outputs[i].Value;
			for( int j = 0; j < dv->NumLines; j ++ )
				_fv(dv->Values[j]);
		}
	}
	#undef _fv

	// -- Build up list of 'local' lines --
	 int	nGroups = 0;
	tDispValue **vals;
	{
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
			vals[grpnum] = malloc(sizeof(tDispValue)
				+ grp->Size*sizeof(tLinkValue)
				+ 1+strlen(grp->Name) + 1
				);
			assert(vals[grpnum]);
			vals[grpnum]->Values = (void*)(vals[grpnum] + 1);
			vals[grpnum]->Name = (void*)(vals[grpnum]->Values + grp->Size);
			
			vals[grpnum]->Name[0] = '@';
			strcpy(vals[grpnum]->Name+1, grp->Name);
			vals[grpnum]->UsageBits = 0;
			vals[grpnum]->NumLines = grp->Size;
			for(int i = 0; i < grp->Size; i ++)
				vals[grpnum]->Values[i] = grp->Links[i]->Value;
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
			const char *prefname = "";
			for(tLink *link = val->FirstLink; link; link = link->Next) {
				if( link->Name[0] == '$' )
					prefname = link->Name;
			}
			
			// Allocate
			vals[ofs] = malloc(sizeof(tDispValue) + 1*sizeof(tLinkValue*) + strlen(prefname)+1);
			assert(vals[ofs]);
			
			vals[ofs]->Values = (void*)(vals[ofs] + 1);
			vals[ofs]->Name = (void*)(vals[ofs]->Values + 1);
			strcpy(vals[ofs]->Name, prefname);
			vals[ofs]->UsageBits = 0;
			vals[ofs]->NumLines = 1;
			vals[ofs]->Values[0] = val;
		}
	}

	// 2. Find all input / output lines (by usage)
	// 3. Locate rw lines that are used outside
	// 4. Locate lines that are double-set/read
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		 int	shift = 0;
		if( ele->Block != Block )
			shift = 2;
		for( int i = 0; i < ele->NInputs; i ++ )
			int_Prerender_MarkVal(nGroups,vals, ele->Inputs[i]->Value, UB_READ<<shift);
		for( int i = 0; i < ele->NOutputs; i ++ )
			int_Prerender_MarkVal(nGroups,vals, ele->Outputs[i]->Value, UB_WRITE<<shift);
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		 int	shift = 0;
		if( subu->Block != Block )
			shift = 2;
		for( int i = 0; i < subu->Inputs.NItems; i ++ )
			int_Prerender_MarkVal(nGroups,vals, subu->Inputs.Items[i]->Value, UB_READ<<shift);
		for( int i = 0; i < subu->Outputs.NItems; i ++ )
			int_Prerender_MarkVal(nGroups,vals, subu->Outputs.Items[i]->Value, UB_WRITE<<shift);
	}
	for( tBlock *child = Block->SubBlocks; child; child = child->Next )
	{
		for( int i = 0; i < child->DispInfo->NInputs; i ++ ) {
			tDispValue	*dv = child->DispInfo->Inputs[i].Value;
			for( int j = 0; j < dv->NumLines; j ++ )
				int_Prerender_MarkVal(nGroups,vals, dv->Values[j], UB_READ);
		}
		for( int i = 0; i < child->DispInfo->NOutputs; i ++ ) {
			tDispValue	*dv = child->DispInfo->Outputs[i].Value;
			for( int j = 0; j < dv->NumLines; j ++ )
				int_Prerender_MarkVal(nGroups,vals, dv->Values[j], UB_WRITE);
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
	dispinfo = malloc(sizeof(tDispInfo) + (nInputs + nOutputs)*sizeof(tDispValRef));
	dispinfo->Inputs = (void*)(dispinfo + 1);
	dispinfo->Outputs = dispinfo->Inputs + nInputs;

	// - Populate inputs/outputs
	 int	in_ofs = 0, out_ofs = 0;
	for( int i = 0; i < nGroups; i ++ )
	{
		int bits = vals[i]->UsageBits;
		// Read locally and written outside : Input
		if( (bits & UB_READ) && (bits & UB_EXTWRITE) ) {
			dispinfo->Inputs[in_ofs].Value = vals[i];
			dispinfo->Inputs[in_ofs].First = 0;
			dispinfo->Inputs[in_ofs].Last = vals[i]->NumLines-1;
			dispinfo->Inputs[in_ofs].Ofs = in_ofs;
			in_ofs ++;
		}
		// Written locally and read outside : Output
		if( (bits & UB_WRITE) && (bits & UB_EXTREAD) ) {
			dispinfo->Outputs[out_ofs].Value = vals[i];
			dispinfo->Outputs[out_ofs].First = 0;
			dispinfo->Outputs[out_ofs].Last = vals[i]->NumLines-1;
			dispinfo->Outputs[out_ofs].Ofs = out_ofs;
			out_ofs ++;
		}
	}

	// 0. Convert elements into a compacted form
	//  - Line groups as one 'link'
	//  - Convert blocks of 0/1 into an constant source
	//  - ?Prefixed NOT elements as a dot
	//  - Handle assignment to '$NULL' and convert to unconnected output
	dispinfo->Elements = malloc( numElements * sizeof(tDispElement*) );
	 int	eleidx = 0;
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		if( ele->Block != Block )
			continue ;
		dispinfo->Elements[eleidx] =
			Render_int_MakeDispEle(ele->Type->Name, ele->NParams, ele->Params,
				ele->NInputs, ele->Inputs, ele->NOutputs, ele->Outputs);
		eleidx ++;
	}
	for( tExecUnitRef *subu = Unit->SubUnits; subu; subu = subu->Next )
	{
		if( subu->Block != Block );
			continue ;
		dispinfo->Elements[eleidx] =
			Render_int_MakeDispEle(subu->Def->Name, 0, NULL,
				subu->Inputs.NItems,  subu->Inputs.Items,
				subu->Outputs.NItems, subu->Outputs.Items);
		eleidx ++;
	}

	// -- Layout --	
	// 1. Directly connected to input and output
	// 2. Directly connected to input or output
	// 3. Any others internally, working from the output
	// TODO: Element layout
	
	// Route lines around elements:
	// - Prefer vertical junctions
	// TODO: Line routing
}

void Render_int_RasteriseBlock(tBlock *Block, void *Surface, int W, int H, int X, int Y, int MaxDepth)
{
	// TODO: Render_int_RasteriseBlock
}
