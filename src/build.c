/*
 *
 */
#include <common.h>
#include <string.h>
#include <element.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define	USE_LINKS	1

// === PROTOTYPES ===
void	Link_Ref(tLink *Link);


// === GLOBALS ===
tLinkValue	gValue_Zero;
tLinkValue	gValue_One;
tElementDef	*gpElementDefs;
// - Mesh State
tExecUnit	gRootUnit;
tUnitTemplate	*gpUnits;
tTestCase	*gpTests;
// - Parser State
tUnitTemplate	*gpCurUnit;
tTestCase	*gpCurTest;

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
	
	for( dispItem = *listHead; dispItem; prev = dispItem, dispItem = dispItem->Next )
	{
		if( strcmp(Name, dispItem->Label) == 0 ) {
			return NULL;
		}
	}

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
	tGroupDef	*ret, *def, *prev = NULL;

	ret = malloc( sizeof(tGroupDef) + 1 + strlen(Name) + 1 );
	ret->Size = Size;
	ret->Name[0] = '@';
	strcpy(&ret->Name[1], Name);
	
	listhead = &Build_int_GetCurExecUnit()->Groups;
	
	//printf("ret->Name = '%s', def = %p\n", ret->Name, def);
	for( def = *listhead; def; prev = def, def = def->Next )
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
		//printf("strcmp(%s, %s)\n", Name, def->Name);
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
	return ret;
}

void LinkValue_Ref(tLinkValue *Value)
{
	Value->ReferenceCount ++;
}

void LinkValue_Deref(tLinkValue *Value)
{
	Value->ReferenceCount --;
	if( !Value->ReferenceCount ) {
		if( !Build_IsConstValue(Value) )
			free(Value);
	}
}

void Link_Ref(tLink *Link)
{
	Link->ReferenceCount ++;
}

void Link_Deref(tLink *Link)
{
	assert(Link->ReferenceCount);
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

	first = &Build_int_GetCurExecUnit()->Links;
	ret->Next = *first;
	*first = ret;
	
	return ret;
}

/**
 * \brief Create a named link (line)
 */
tLink *CreateNamedLink(const char *Name)
{
	tLink	**first;
	tLink	*ret, *prev = NULL;

	first = &Build_int_GetCurExecUnit()->Links;
	
	for(ret = *first; ret; prev = ret, ret = ret->Next )
	{
		if(strcmp(Name, ret->Name) == 0)	return ret;
		#if SORTED_LINK_LIST
		if(strcmp(Name, ret->Name) < 0)	break;
		#endif
	}
	
	ret = malloc(sizeof(tLink) + strlen(Name) + 1);
	strcpy(ret->Name, Name);
	ret->Link = NULL;
	ret->Backlink = NULL;
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
tList *Build_AppendUnit(tUnitTemplate *Unit, const tList *Inputs)
{
	// Check input counts
	if( Inputs->NItems != Unit->Internals.Inputs.NItems ) {
		// TODO: Error message?
		fprintf(stderr, "ERROR: %s takes %i lines, %i given\n",
			Unit->Name, Unit->Internals.Inputs.NItems, Inputs->NItems);
		return NULL;
	}

	// - Allocate reference
	size_t	extrasize = (Unit->Internals.Outputs.NItems + Inputs->NItems)*sizeof(tLink*)
		+ strlen(Unit->Name) + 1;
	tExecUnitRef	*ref;
	ref = malloc(sizeof(tExecUnitRef) + extrasize);
	assert(ref);
	// - Base values
	ref->Inputs.NItems = Inputs->NItems;
	ref->Outputs.NItems = Unit->Internals.Outputs.NItems;
	// - Set up pointers
	ref->Inputs.Items = (void*)(ref + 1);
	ref->Outputs.Items = (void*)(ref->Inputs.Items + Inputs->NItems);
	ref->Name = (void*)(ref->Outputs.Items + Unit->Internals.Outputs.NItems);
	// - Initialise lists
	for( int i = 0; i < Inputs->NItems; i ++ )
	{
		Link_Ref(Inputs->Items[i]);
		ref->Inputs.Items[i] = Inputs->Items[i];
	}
	for( int i = 0; i < ref->Outputs.NItems; i ++ )
	{
		ref->Outputs.Items[i] = CreateAnonLink();
	}

	// Append to list
	tExecUnitRef	**imphead = &Build_int_GetCurExecUnit()->SubUnits;
	ref->Next = *imphead;
	*imphead = ref;

	// Create output return
	tList *ret = malloc( sizeof(tList) + ref->Outputs.NItems * sizeof(tLink*) );
	ret->NItems = ref->Outputs.NItems;
	ret->Items = (void *)( ret + 1 );
	for( int i = 0; i < ref->Outputs.NItems; i ++ ) {
		Link_Ref(ref->Outputs.Items[i]);
		ret->Items[i] = ref->Outputs.Items[i];
	}
	
	return ret;	
}

tList *Build_AppendElement(tElementDef *def, int NParams, const int *Params, const tList *Inputs)
{
	tElement	*ele;
	tList	*ret = NULL;
	
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
	ele = def->Create(NParams, (int*)Params, Inputs->NItems, Inputs->Items);
	if(!ele) {
		fprintf(stderr, "Error in creating '%s'\n", def->Name);
		return NULL;
	}
	ele->Type = def;	// Force type to be set
	for( int i = 0; i < Inputs->NItems; i ++ ) {
		Link_Ref(Inputs->Items[i]);
		ele->Inputs[i] = Inputs->Items[i];
	}
	for( int i = 0; i < ele->NOutputs; i ++ )
		ele->Outputs[i] = CreateAnonLink();
	ele->NParams = NParams;
	for( int i = 0; i < NParams; i ++ )
		ele->Params[i] = Params[i];
	
	// Append to element list
	tElement **listhead = &Build_int_GetCurExecUnit()->Elements;
	ele->Next = *listhead;
	*listhead = ele;
	
	// Create return list
	ret = malloc( sizeof(tList) + ele->NOutputs*sizeof(tLink*) );
	ret->NItems = ele->NOutputs;
	ret->Items = (void *)( ret + 1 );
	for( int i = 0; i < ele->NOutputs; i ++ ) {
		Link_Ref(ele->Outputs[i]);
		ret->Items[i] = ele->Outputs[i];
	}
	
	return ret;
	
}

/**
 * \brief Create a new unit instance
 */
tList *Build_ReferenceUnit(const char *Name, int NParams, const int *Params, const tList *Inputs)
{
	// First, check for template units (#defunit)
	for( tUnitTemplate *tpl = gpUnits; tpl; tpl = tpl->Next )
	{
		// We can't recurse, so also check that
		if( strcmp(Name, tpl->Name) == 0 )
		{
			if( tpl == gpCurUnit )
				break;
			return Build_AppendUnit(tpl, Inputs);
		}
	}

	// Next, check for builtins
	for( tElementDef *def = gpElementDefs; def; def = def->Next )
	{
		if( strcmp(Name, def->Name) == 0 )
		{
			return Build_AppendElement(def, NParams, Params, Inputs);
		}
	}
	
	return NULL;
}

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
	 int	len, i, digits;
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
	
	digits = 0;
	for( i = grp->Size-1; i; i /= 10 )	digits ++;
	if( digits == 0 )	digits = 1;
	
	// Get string length
	len = snprintf(NULL, 0, "%s[%*i]", Name, digits, Index);
	
	{
		char	newname[len + 1];
		snprintf(newname, len+1, "%s[%*i]", Name, digits, Index);
		
		Dest->NItems ++;
		Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
		Dest->Items[Dest->NItems-1] = CreateNamedLink(newname);
	}
	return 0;
}

/**
 * \brief Append every line in a group to a list
 */
int List_AppendGroup(tList *Dest, const char *Name)
{
	tGroupDef	*grp = GetGroup(Name);
	 int	i, ofs, len;
	 int	digits = 0;
	
	//printf("AppendGroup: (%p, %s)\n", Dest, Name);
	if(!grp) {
		// ERROR:
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	for( i = grp->Size-1; i; i /= 10 )	digits ++;
	if( digits == 0 )	digits = 1;	
	
	len = snprintf(NULL, 0, "%s[%*i]", Name, digits, grp->Size-1);

	ofs = Dest->NItems;
	Dest->NItems += grp->Size;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	
	for( i = 0; i < grp->Size; i ++ )
	{
		char	newname[len + 1];
		snprintf(newname, len+1, "%s[%*i]", Name, digits, i);
		
		Dest->Items[ofs+i] = CreateNamedLink(newname);
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
			if( nTrans > 1 )
				DEBUG_S("%i links moved to value of '%s'", nTrans, left->Name);
			// rval should now be unused
			free(rval);
		}
	}
	return 0;
}
