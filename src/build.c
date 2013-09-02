/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 * 
 * build.c
 * - Mesh construction
 */
#include <common.h>
#include <string.h>
#include <element.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define	USE_LINKS	1
#define SORTED_LINK_LIST	1

// === PROTOTYPES ===
void	Link_Ref(tLink *Link);
tLink	*CreateNamedLink(const char *Name);


// === GLOBALS ===
tLinkValue	gValue_Zero;
tLinkValue	gValue_One;
tElementDef	*gpElementDefs;
// - Mesh State
tExecUnit	gRootUnit = {.Name="_ROOT_",.RootBlock={.Unit=&gRootUnit}};
tUnitTemplate	*gpUnits;
tTestCase	*gpTests;
// - Parser State
tUnitTemplate	*gpCurUnit;
tTestCase	*gpCurTest;
tBlock  	*gpCurBlock = &gRootUnit.RootBlock;

// === CODE ===
int Build_IsConstValue(tLinkValue *Val)
{
	return (Val == &gValue_Zero || Val == &gValue_One);
}

/**
 * \brief Define a unit (#defunit)
 */
int Unit_DefineUnit(const char *Name)
{
	tUnitTemplate	*tpl, *prev = NULL;
	
	// Nesting is a no-no
	if(gpCurUnit || gpCurTest) {
		return -1;
	}
	
	// Check names
	for( tpl = gpUnits; tpl; prev = tpl, tpl = tpl->Next )
	{
		if( strcmp(Name, tpl->Name) == 0 )	return -2;
		if( strcmp(Name, tpl->Name) > 0)	break;
	}
	
	// Create new one
	tpl = calloc( 1, sizeof(tUnitTemplate) + strlen(Name) + 1 );
	strcpy(tpl->Name, Name);
	tpl->Internals.Name = tpl->Name;
	
	// Add to list
	if(prev) {
		tpl->Next = prev->Next;
		prev->Next = tpl;
	}
	else {
		tpl->Next = gpUnits;
		gpUnits = tpl;
	}
	
	gpCurUnit = tpl;
	gpCurBlock = &tpl->Internals.RootBlock;
	tpl->Internals.RootBlock.Unit = &tpl->Internals;
	
	return 0;
}

int Unit_AddSingleInput(const char *Name)
{
	if(!gpCurUnit)	return -1;
	List_AppendLine( &gpCurUnit->Internals.Inputs, Name );
	return 0;
}

int Unit_AddGroupInput(const char *Name, int Size)
{
	if(!gpCurUnit)	return -1;
	Build_CreateGroup( Name+1, Size );
	List_AppendGroup( &gpCurUnit->Internals.Inputs, Name );
	return 0;
}

int Unit_AddSingleOutput(const char *Name)
{
	if(!gpCurUnit)	return -1;
	List_AppendLine( &gpCurUnit->Internals.Outputs, Name );
	return 0;
}

int Unit_AddGroupOutput(const char *Name, int Size)
{
	if(!gpCurUnit)	return -1;
	Build_CreateGroup( Name+1, Size );
	List_AppendGroup( &gpCurUnit->Internals.Outputs, Name );
	return 0;
}

int Unit_CloseUnit(void)
{
	if(!gpCurUnit)	return -1;

	gpCurUnit = NULL;
	gpCurBlock = &gRootUnit.RootBlock;
	return 0;
}

int Unit_IsInUnit(void)
{
	return !!gpCurUnit;
}

// --------------------------------------------------------------------
int Test_CreateTest(int MaxLength, const char *Name, int NameLength)
{
	if( gpCurTest || gpCurUnit )
		return -1;
	
	gpCurTest = calloc(1, sizeof(tTestCase) + NameLength + 1);
	gpCurTest->MaxLength = MaxLength;
	memcpy(gpCurTest->Name, Name, NameLength);
	gpCurTest->Name[NameLength] = '\0';
	gpCurTest->Internals.Name = gpCurTest->Name;
	gpCurBlock = &gpCurTest->Internals.RootBlock;

	tTestCase *p;
	for( p = gpTests; p && p->Next; p = p->Next )
		;
	if(p)
		p->Next = gpCurTest;
	else
		gpTests = gpCurTest;
	
	return 0;
}

int Test_AddAssertion(const tList *Condition, const tList *Values, const tList *Expected)
{
	if(!gpCurTest)
		return -1;
	
	if( Values->NItems != Expected->NItems ) {
		printf("Count mismatch in assertion (%i != %i)\n", Values->NItems, Expected->NItems);
		return -1;
	}

	tAssertion *a = malloc( sizeof(tAssertion)
		+ (Condition->NItems + Values->NItems + Expected->NItems)*sizeof(tLink*) );
	
	a->Condition.NItems = Condition->NItems;
	a->Expected.NItems = Values->NItems;
	a->Values.NItems = Expected->NItems;
	a->Condition.Items = (void*)(a + 1);
	a->Expected.Items = a->Condition.Items + Condition->NItems;
	a->Values.Items = a->Expected.Items + Expected->NItems;
	for(int i = 0; i < Condition->NItems; i ++ )	Link_Ref(Condition->Items[i]);
	for(int i = 0; i < Expected->NItems; i ++ )	Link_Ref(Expected->Items[i]);
	for(int i = 0; i < Values->NItems; i ++ )	Link_Ref(Values->Items[i]);
	memcpy(a->Condition.Items, Condition->Items, Condition->NItems*sizeof(tLink*));
	memcpy(a->Expected.Items, Expected->Items, Expected->NItems*sizeof(tLink*));
	memcpy(a->Values.Items, Values->Items, Values->NItems*sizeof(tLink*));

	// Append on to the end of the assertion list
	tAssertion *p;
	for( p = gpCurTest->Internals.Assertions; p && p->Next; p = p->Next )
		;
	if(p)
		p->Next = a;
	else
		gpCurTest->Internals.Assertions = a;
	a->Next = NULL;

	return 0;
}

int Test_AddCompletion(const tList *Condition)
{
	if( !gpCurTest )
		return -1;
	if( Condition->NItems != 1 )
		return -1;
	
	if( gpCurTest->CompletionCondition )
		return -1;
	
	// TODO: Reference link
	gpCurTest->CompletionCondition = Condition->Items[0];
	return 0;
}

int Test_CloseTest(void)
{
	if(!gpCurTest)	return -1;

	gpCurTest = NULL;
	gpCurBlock = &gRootUnit.RootBlock;
	return 0;
}

int Test_IsInTest(void)
{
	return !!gpCurTest;
}

// --------------------------------------------------------------------

tExecUnit *Build_int_GetCurExecUnit(void)
{
	if( gpCurUnit )
		return &gpCurUnit->Internals;
	else if( gpCurTest )
		return &gpCurTest->Internals;
	else
		return &gRootUnit;
}

tBreakpoint *Build_AddBreakpoint(const char *Name, const tList *Condition)
{
	tBreakpoint	*bp, *prev = NULL;
	 int	pos, i;
	tBreakpoint	**listHead;
	
	
	listHead = &Build_int_GetCurExecUnit()->Breakpoints;
	
	for( bp = *listHead; bp; prev = bp, bp = bp->Next )
	{
		if( strcmp(Name, bp->Label) == 0 ) {
			return NULL;
		}
	}
	
	// Create new
	pos = strlen(Name)+1;
	bp = malloc( sizeof(tBreakpoint) + pos + (Condition->NItems)*sizeof(tLink*) );
	bp->Condition.NItems = Condition->NItems;
	bp->Condition.Items = (void*)&bp->Label[pos];
	for( i = 0; i < Condition->NItems; i ++ )
		Link_Ref(bp->Condition.Items[i] = Condition->Items[i]);
	
	strcpy(bp->Label, Name);
	
	if( prev ) {
		bp->Next = prev->Next;
		prev->Next = bp;
	}
	else {
		bp->Next = NULL;
		*listHead = bp;
	}
	
	return bp;
}

tDisplayItem *Build_AddDisplayItem(const char *Name, const tList *Condition, const tList *Values)
{
	tDisplayItem	*dispItem, *prev = NULL;
	 int	namelen;
	tDisplayItem	**listHead;
	
	listHead = &Build_int_GetCurExecUnit()->DisplayItems;
	
//	for( dispItem = *listHead; dispItem; prev = dispItem, dispItem = dispItem->Next )
//	{
//		if( strcmp(Name, dispItem->Label) == 0 ) {
//			return NULL;
//		}
//	}

	// Create new
	namelen = strlen(Name)+1;
	dispItem = malloc( sizeof(tDisplayItem) + namelen + (Condition->NItems+Values->NItems)*sizeof(tLink*) );
	dispItem->Condition.NItems = Condition->NItems;
	dispItem->Condition.Items = (void*)&dispItem->Label[namelen];
	dispItem->Values.NItems = Values->NItems;
	dispItem->Values.Items = dispItem->Condition.Items + Condition->NItems;
	for( int i = 0; i < Condition->NItems; i ++ ) {
		Link_Ref(Condition->Items[i]);
		dispItem->Condition.Items[i] = Condition->Items[i];
	}
	for( int i = 0; i < Values->NItems; i++ ) {
		Link_Ref(Values->Items[i]);
		dispItem->Values.Items[i] = Values->Items[i];
	}
	
	strcpy(dispItem->Label, Name);
	
	if( prev ) {
		dispItem->Next = prev->Next;
		prev->Next = dispItem;
	}
	else {
		dispItem->Next = NULL;
		*listHead = dispItem;
	}
	
	return dispItem;
}

/**
 * \brief Create a new group
 */
void Build_CreateGroup(const char *Name, int Size)
{
	tGroupDef	**listhead;
	tGroupDef	*ret, *prev = NULL;

	ret = malloc( sizeof(tGroupDef) + Size*sizeof(tLink*) + (1 + strlen(Name)) + 1 );
	ret->Size = Size;
	ret->Links = (void*)(ret + 1);
	ret->Name = (void*)(ret->Links + Size);
	ret->Name[0] = '@';
	strcpy(&ret->Name[1], Name);
	
	listhead = &Build_int_GetCurExecUnit()->Groups;
	
	//printf("ret->Name = '%s', def = %p\n", ret->Name, def);
	for( tGroupDef *def = *listhead; def; prev = def, def = def->Next )
	{
		if(strcmp(ret->Name, def->Name) == 0) {
			fprintf(stderr, "ERROR: Redefinition of %s\n", ret->Name);
			if( gpCurUnit )
				fprintf(stderr, " in unit '%s'\n", gpCurUnit->Name);
			else if( gpCurTest )
				fprintf(stderr, " in test '%s'\n", gpCurTest->Name);
			else
				fprintf(stderr, " in root scope\n");
			free(ret);
			return;
		}
		//if(strcmp(Name, def->Name) > 0)	break;
	}
	
	if(prev) {
		ret->Next = prev->Next;
		prev->Next = ret;
	}
	else {
		ret->Next = *listhead;
		*listhead = ret;
	}

	// Create link set
	size_t digits = snprintf(NULL, 0, "%i", Size-1);
	size_t len = strlen(Name)+1+digits+1;
	for( int i = 0; i < Size; i ++ )
	{
		char	newname[len + 1];
		snprintf(newname, len+1, "%s[%*i]", Name, (int)digits, i);
		ret->Links[i] = CreateNamedLink(newname);
	}
}

/**
 * \param Name	Name including leading '@'
 */
tGroupDef *GetGroup(const char *Name)
{
	tGroupDef	*def;
	
	def = Build_int_GetCurExecUnit()->Groups;
	
	for( ; def; def = def->Next )
	{
		if(strcmp(Name, def->Name) == 0)	return def;
		//if(strcmp(Name, def->Name) < 0)	return NULL;
	}
	return NULL;
}

/**
 * \brief Create a new link value
 */
tLinkValue *LinkValue_Create(void)
{
	tLinkValue	*ret;
	ret = calloc(sizeof(tLinkValue),1);
	assert(ret);
	ret->ReferenceCount = 1;

	tExecUnit *unit = Build_int_GetCurExecUnit();
	ret->Next = unit->Values;
	unit->Values = ret;

	return ret;
}

void LinkValue_Free(tLinkValue *Value)
{
	assert(Value->ReferenceCount != -1);
	Value->ReferenceCount = -1;
}

void Link_Ref(tLink *Link)
{
	Link->ReferenceCount ++;
}

void Link_Deref(tLink *Link)
{
	//assert(Link->ReferenceCount);
	if( Link->ReferenceCount > 0 )
		Link->ReferenceCount --;
	//if( Link->ReferenceCount == 0 )
	//{
	//	free(Link);
	//}
}

/**
 * \brief Create an unnamed link
 */
tLink *CreateAnonLink(void)
{
	tLink	**first;
	tLink	*ret = calloc(sizeof(tLink)+1, 1);
	ret->Name[0] = '\0';
	ret->Value = LinkValue_Create();
	ret->Value->FirstLink = ret;
	ret->ReferenceCount = 1;

	tExecUnit *unit = Build_int_GetCurExecUnit();
	first = &unit->Links;
	ret->Next = *first;
	*first = ret;
	if( !unit->LastAnonLink )
		unit->LastAnonLink = ret;
		
	
	return ret;
}

/**
 * \brief Create a named link (line)
 */
tLink *CreateNamedLink(const char *Name)
{
	tLink	**first;
	tLink	*ret, *prev = NULL;
	tExecUnit	*unit = Build_int_GetCurExecUnit();

	first = &unit->Links;
	
	for(ret = (unit->LastAnonLink ? unit->LastAnonLink : *first); ret; prev = ret, ret = ret->Next )
	{
		if(strcmp(Name, ret->Name) == 0)	return ret;
		#if SORTED_LINK_LIST
		if(strcmp(Name, ret->Name) < 0)	break;
		#endif
	}
	
	ret = malloc(sizeof(tLink) + strlen(Name) + 1);
	strcpy(ret->Name, Name);
	ret->Link = NULL;
	ret->ReferenceCount = 1;

	if( strcmp(Name, "0") == 0 ) {
		ret->Value = &gValue_Zero;
	}
	else if( strcmp(Name, "$NULL") == 0 ) {
		ret->Value = &gValue_Zero;
	}
	else if( strcmp(Name, "1") == 0 ) {
		ret->Value = &gValue_One;
	}
	else
		ret->Value = LinkValue_Create();
	
	ret->ValueNext = ret->Value->FirstLink;
	ret->Value->FirstLink = ret;
	
	
	if(prev) {
		ret->Next = prev->Next;
		prev->Next = ret;
	}
	else {
		ret->Next = *first;
		*first = ret;
	}
	
	return ret;
}

/**
 * \brief Append a #defunit unit to the state
 */
tList *Build_AppendUnit(tUnitTemplate *Unit, const tList *Inputs, const tList *Outputs)
{
	tList	*ret = (void*)-1;
	
	// Check input counts
	if( Inputs->NItems != Unit->Internals.Inputs.NItems ) {
		// TODO: Error message?
		fprintf(stderr, "ERROR: %s takes %i lines, %i given\n",
			Unit->Name, Unit->Internals.Inputs.NItems, Inputs->NItems);
		return NULL;
	}
	if( !Outputs ) {
		 int	nOut = Unit->Internals.Outputs.NItems;
		ret = malloc(sizeof(tList) + nOut * sizeof(tLink*) );
		ret->NItems = nOut;
		ret->Items = (void*)(ret + 1);
		for( int i = 0; i < nOut; i ++ )
			ret->Items[i] = CreateAnonLink();
		Outputs = ret;
	}
	if( Outputs->NItems != Unit->Internals.Outputs.NItems ) {
		fprintf(stderr, "ERROR: %s outputs %i lines, %i read\n",
			Unit->Name, Unit->Internals.Outputs.NItems, Outputs->NItems);
		return NULL;
	}

	// - Allocate reference
	size_t	extrasize = (Unit->Internals.Outputs.NItems + Inputs->NItems)*sizeof(tLink*)
		+ strlen(Unit->Name) + 1;
	tExecUnitRef	*ref;
	ref = malloc(sizeof(tExecUnitRef) + extrasize);
	assert(ref);
	// - Base values
//	assert(gpCurBlock);
	ref->Block = gpCurBlock;
	ref->Inputs.NItems = Inputs->NItems;
	ref->Outputs.NItems = Outputs->NItems;
	// - Set up pointers
	ref->Inputs.Items = (void*)(ref + 1);
	ref->Outputs.Items = (void*)(ref->Inputs.Items + Inputs->NItems);
	ref->Def = Unit;
	// - Initialise lists
	for( int i = 0; i < Inputs->NItems; i ++ )
	{
		Link_Ref(Inputs->Items[i]);
		ref->Inputs.Items[i] = Inputs->Items[i];
	}
	for( int i = 0; i < Outputs->NItems; i ++ )
	{
		if(ret != (void*)-1)
			Link_Ref(Outputs->Items[i]);
		ref->Outputs.Items[i] = Outputs->Items[i];
	}

	// Append to list
	tExecUnitRef	**imphead = &Build_int_GetCurExecUnit()->SubUnits;
	ref->Next = *imphead;
	*imphead = ref;
	
	return ret;	
}

tList *Build_AppendElement(tElementDef *def, int NParams, const int *Params, const tList *Inputs, const tList *Outputs)
{
	tElement	*ele;
	tList	*ret = (void*)-1;
	
	// Sanity check input numbers
	if( Inputs->NItems < def->MinInput ) {
		fprintf(stderr, "%s requires at least %i input lines\n", def->Name, def->MinInput);
		return NULL;
	}
	if( def->MaxInput != -1 && Inputs->NItems > def->MaxInput ) {
		fprintf(stderr, "%s takes at most %i input lines\n", def->Name, def->MaxInput);
		return NULL;
	}
	if( NParams > MAX_PARAMS ) {
		fprintf(stderr, "%s passed %i params (max %i)\n", def->Name, NParams, MAX_PARAMS);
		return NULL;
	}
	
	// Create an instance
	ele = def->Create(NParams, (int*)Params, Inputs->NItems);
	if(!ele) {
		fprintf(stderr, "Error in creating '%s'\n", def->Name);
		return NULL;
	}
	ele->Type = def;	// Force type to be set
	ele->Block = gpCurBlock;
	
	if( Inputs->NItems != ele->NInputs ) {
		fprintf(stderr, "%s requested %i inputs, %i passed\n", def->Name, ele->NInputs, Inputs->NItems);
		free(ele);
		return NULL;
	}
	if( Outputs && Outputs->NItems != ele->NOutputs ) {
		fprintf(stderr, "%s requested %i outputs, %i passed\n", def->Name, ele->NOutputs, Outputs->NItems);
		free(ele);
		return NULL;
	}
	if( !Outputs ) {
		ret = malloc( sizeof(tList) + ele->NOutputs*sizeof(tLink*) );
		ret->NItems = ele->NOutputs;
		ret->Items = (void*)(ret + 1);
		for( int i = 0; i < ele->NOutputs; i ++ )
			ret->Items[i] = CreateAnonLink();
		Outputs = ret;
	}

	// Populate
	for( int i = 0; i < Inputs->NItems; i ++ ) {
		Link_Ref(Inputs->Items[i]);
		ele->Inputs[i] = Inputs->Items[i];
	}
	for( int i = 0; i < ele->NOutputs; i ++ ) {
		if(ret != (void*)-1)
			Link_Ref(Outputs->Items[i]);
		ele->Outputs[i] = Outputs->Items[i];
	}
	ele->NParams = NParams;
	for( int i = 0; i < NParams; i ++ )
		ele->Params[i] = Params[i];
	
	// Append to element list
	tElement **listhead = &Build_int_GetCurExecUnit()->Elements;
	ele->Next = *listhead;
	*listhead = ele;
	
	return ret;
	
}

/**
 * \brief Create a new unit instance
 */
tList *Build_ReferenceUnit(const char *Name, int NParams, const int *Params, const tList *Inputs, const tList *Outputs)
{
	// First, check for template units (#defunit)
	for( tUnitTemplate *tpl = gpUnits; tpl; tpl = tpl->Next )
	{
		if( strcmp(Name, tpl->Name) == 0 )
		{
			// We can't recurse, so also check that
			if( tpl == gpCurUnit )
				break;
			return Build_AppendUnit(tpl, Inputs, Outputs);
		}
	}

	// Next, check for builtins
	for( tElementDef *def = gpElementDefs; def; def = def->Next )
	{
		if( strcmp(Name, def->Name) == 0 )
		{
			return Build_AppendElement(def, NParams, Params, Inputs, Outputs);
		}
	}
	
	return NULL;
}

// -------------------------------------------------
// Blocks / Groupings
// -------------------------------------------------
void Build_int_StartBlock(const char *Name, int X, int Y, int W, int H)
{
	tExecUnit	*cur = Build_int_GetCurExecUnit();
	size_t	namelen = (Name ? strlen(Name) + 1 : 0);
	tBlock	*newblock = calloc(1, sizeof(tBlock) + namelen);
	if( Name ) {
		newblock->Name = (char*)(newblock + 1);
		strcpy(newblock->Name, Name);
	}
	newblock->Unit = cur;
	
	newblock->Next = NULL;
	newblock->Parent = gpCurBlock;
	if( gpCurBlock->SubBlocks )
		gpCurBlock->LastChild->Next = newblock;
	else
		gpCurBlock->SubBlocks = newblock;
	gpCurBlock->LastChild = newblock;
	
	gpCurBlock = newblock;
}

void Build_StartBlock(const char *Name, int X, int Y, int W, int H)
{
	// - Empty name is reserved for codeline blocks
	if( Name && Name[0] == '\0' )
		return ;
	Build_int_StartBlock(Name, X, Y, W, H);
}
void Build_EndBlock(void)
{
	tExecUnit	*cur = Build_int_GetCurExecUnit();
	if( gpCurBlock == NULL || gpCurBlock == &cur->RootBlock ) {
		// Oops?
		fprintf(stderr, "Block closed while at root\n");
		return ;
	}
	gpCurBlock = gpCurBlock->Parent;
}
void Build_LinePos(int X, int Y, int W, int H)
{
	Build_int_StartBlock("", X, Y, W, H);
}
void Build_EndLine(void)
{
	tExecUnit	*cur = Build_int_GetCurExecUnit();
	if( gpCurBlock == NULL || gpCurBlock == &cur->RootBlock )
		return ;
	if( !gpCurBlock->Name || gpCurBlock->Name[0] != '\0' ) {
		// Ignore
		return ;
	}
	gpCurBlock = gpCurBlock->Parent;
}

// ------------------------------------------------
// List manipulation
// ------------------------------------------------
/**
 */
void List_Free(tList *List)
{
	for( int i = 0; i < List->NItems; i ++ )
		Link_Deref(List->Items[i]);
	if( List->Items != (void *)( (intptr_t)List + sizeof(tList) ) )
		free(List->Items);
	else
		free(List);
}

/**
 * \brief Append one list to another
 */
void List_AppendList(tList *Dest, const tList *Src)
{
	 int	oldOfs = Dest->NItems;
	Dest->NItems += Src->NItems;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	for( int i = 0; i < Src->NItems; i ++ )
	{
		Link_Ref(Src->Items[i]);
		Dest->Items[oldOfs+i] = Src->Items[i];
	}
}

/**
 * \brief Append a named line to a list
 */
void List_AppendLine(tList *Dest, const char *Name)
{
	Dest->NItems ++;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	Dest->Items[Dest->NItems-1] = CreateNamedLink(Name);
}

/**
 * \brief Append a single line of a group to a list
 */
int List_AppendGroupItem(tList *Dest, const char *Name, int Index)
{
	tGroupDef	*grp = GetGroup(Name);
	
	//printf("AppendGroupItem: (%p, %s, %i)\n", Dest, Name, Index);
	if(!grp) {
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	if( Index < 0 || Index >= grp->Size ) {
		fprintf(stderr, "ERROR: Index out of bounds on %s[%i > %i]\n",
			Name, Index, grp->Size - 1);
		return 2;
	}
	
	Dest->NItems ++;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	Dest->Items[Dest->NItems-1] = grp->Links[Index];
	Link_Ref(grp->Links[Index]);
	
	return 0;
}

/**
 * \brief Append every line in a group to a list
 */
int List_AppendGroup(tList *Dest, const char *Name)
{
	tGroupDef	*grp = GetGroup(Name);
	 int	ofs;
	
	//printf("AppendGroup: (%p, %s)\n", Dest, Name);
	if(!grp) {
		// ERROR:
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	ofs = Dest->NItems;
	Dest->NItems += grp->Size;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	
	for( int i = 0; i < grp->Size; i ++ )
	{
		Dest->Items[ofs+i] = grp->Links[i];
		Link_Ref(grp->Links[i]);
	}
	
	return 0;
}

/**
 * \brief Link two sets of links (Sets the aliases for \a Src to \a Dest)
 * \return Boolean Failure
 */
int List_EquateLinks(tList *Dest, const tList *Src)
{
	 int	i;
	
	if( Dest->NItems != Src->NItems ) {
		fprintf(stderr, "ERROR: MergeLinks - Mismatch of source and destination counts\n");
		return -1;
	}
	
	for( i = 0; i < Dest->NItems; i ++ )
	{
		tLink	*left = Dest->Items[i];
		tLink	*right = Src->Items[i];

		if( left->Value == right->Value )
		{
			fprintf(stderr, "Note: Equating same-valued links ('%s' = '%s' %p)\n",
				left->Name, right->Name, left->Value);
		}
		else
		{
			tLinkValue	*lval = left->Value;
			tLinkValue	*rval = right->Value;

			// Never destroy a constant
//			if( Build_IsConstValue(rval) ) {
//				tLinkValue	*tmp;
//				tmp = rval;
//				rval = lval;
//				lval = tmp;
//			}
			// If both are constant, return error
			if( Build_IsConstValue(rval) ) {
				fprintf(stderr, "ERROR: Merging '0' and '1'\n");
				return 1;
			}

			// Transfer links from rval to lval
			int nTrans = 0;
			while( rval->FirstLink )
			{
				tLink	*link = rval->FirstLink;
				// Remove from rval
				rval->FirstLink = link->ValueNext;
				// Set to lval
				link->Value = lval;
				// Add to lval
				link->ValueNext = lval->FirstLink;
				lval->FirstLink = link;
				nTrans ++;
			}
			//if( nTrans > 1 )
			//	DEBUG_S("%i links moved to value of '%s'", nTrans, left->Name);
			// rval should now be unused
			
			LinkValue_Free(rval);
		}
	}
	return 0;
}


tElement *EleHelp_CreateElement(size_t NInputs, size_t NOutputs, size_t ExtraData)
{
	tElement *ret = malloc(sizeof(tElement) + (NInputs + NOutputs)*sizeof(tLink*) + ExtraData);
	assert(ret);

	ret->NInputs = NInputs;
	ret->Inputs = (void*)(ret + 1);

	ret->NOutputs = NOutputs;
	ret->Outputs = ret->Inputs + NInputs;
	
	ret->InfoSize = ExtraData;
	ret->Info = ret->Outputs + NOutputs;
	return ret;
}

tElement *Build_DuplicateElement(const tElement *Ele)
{
	size_t	size = sizeof(tElement) + (Ele->NInputs + Ele->NOutputs)*sizeof(tLink*) + Ele->InfoSize;
	tElement *ret = malloc(size);
	assert(ret);
	memcpy(ret, Ele, size);
	ret->Inputs = (void*)(ret + 1);
	ret->Outputs = ret->Inputs + ret->NInputs;
	ret->Info = ret->Outputs + ret->NOutputs;
	
	if( Ele->Type && Ele->Type->Duplicate )
		Ele->Type->Duplicate(Ele, ret);
	return ret;
}
