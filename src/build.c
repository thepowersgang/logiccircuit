/*
 *
 */
#include <common.h>
#include <string.h>
#include <element.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct sGroupDef
{
	struct sGroupDef	*Next;
	 int	Size;
	char	Name[];
} tGroupDef;

typedef struct sUnitTemplate
{
	struct sUnitTemplate	*Next;
	
	tLink	*Links;
	tGroupDef	*Groups;
	tElement	*Elements;
	tList	Inputs;
	tList	Outputs;
	
	 int	InstanceCount;
	
	char	Name[];
}	tUnitTemplate;

// === GLOBALS ===
tLink	*gpLinks = NULL;
tGroupDef	*gpGroups = NULL;
tElement	*gpElements;
tElement	*gpLastElement = (tElement*)&gpElements;
tElementDef	*gpElementDefs;
tUnitTemplate	*gpUnits;
tUnitTemplate	*gpCurUnit;

// === CODE ===
/**
 * \brief Define a unit (#defunit)
 */
int Unit_DefineUnit(const char *Name)
{
	tUnitTemplate	*tpl, *prev = NULL;
	
	// Nesting is a no-no
	if(gpCurUnit) {
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
	AppendLine( &gpCurUnit->Inputs, Name );
	return 0;
}

int Unit_AddGroupInput(const char *Name, int Size)
{
	if(!gpCurUnit)	return -1;
	CreateGroup( Name+1, Size );
	AppendGroup( &gpCurUnit->Inputs, Name );
	return 0;
}

int Unit_AddSingleOutput(const char *Name)
{
	if(!gpCurUnit)	return -1;
	AppendLine( &gpCurUnit->Outputs, Name );
	return 0;
}

int Unit_AddGroupOutput(const char *Name, int Size)
{
	if(!gpCurUnit)	return -1;
	CreateGroup( Name+1, Size );
	AppendGroup( &gpCurUnit->Outputs, Name );
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

void CreateGroup(const char *Name, int Size)
{
	tGroupDef	*ret, *def, *prev = NULL;
	
	ret = malloc( sizeof(tGroupDef) + 1 + strlen(Name) + 1 );
	ret->Size = Size;
	ret->Name[0] = '@';
	strcpy(&ret->Name[1], Name);
	
	if( gpCurUnit )
		def = gpCurUnit->Groups;
	else
		def = gpGroups;
	
	//printf("ret->Name = '%s', def = %p\n", ret->Name, def);
	for( ; def; prev = def, def = def->Next )
	{
		if(strcmp(ret->Name, def->Name) == 0) {
			fprintf(stderr, "ERROR: Redefinition of %s", ret->Name);
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
		if( gpCurUnit ) {
			ret->Next = gpCurUnit->Groups;
			gpCurUnit->Groups = ret;
		}
		else {
			ret->Next = gpGroups;
			gpGroups = ret;
		}
	}
}

/**
 * \param Name	Name including leading '@'
 */
tGroupDef *GetGroup(const char *Name)
{
	tGroupDef	*def, *prev;
	
	if( gpCurUnit )
		def = gpCurUnit->Groups;
	else
		def = gpGroups;
	
	for( ; def; prev = def, def = def->Next )
	{
		//printf("strcmp(%s, %s)\n", Name, def->Name);
		if(strcmp(Name, def->Name) == 0)	return def;
		//if(strcmp(Name, def->Name) < 0)	return NULL;
	}
	return NULL;
}

/**
 * \brief Create an unnamed link
 */
tLink *CreateAnonLink(void)
{
	tLink	*ret = malloc(sizeof(tLink)+1);
	ret->Value = 0;
	ret->Link = NULL;
	ret->Name[0] = '\0';
	
	if( gpCurUnit ) {
		ret->Next = gpCurUnit->Links;
		gpCurUnit->Links = ret;
	}
	else {
		ret->Next = gpLinks;
		gpLinks = ret;
	}
	
	return ret;
}

/**
 * \brief Create a named link (line)
 */
tLink *CreateNamedLink(const char *Name)
{
	tLink	*ret, *def, *prev = NULL;
	
	if( gpCurUnit )
		def = gpCurUnit->Links;
	else
		def = gpLinks;
	
	for( ; def; prev = def, def = def->Next )
	{
		if(strcmp(Name, def->Name) == 0)	return def;
		if(strcmp(Name, def->Name) < 0)	break;
	}
	
	ret = malloc(sizeof(tLink) + strlen(Name) + 1);
	ret->Value = 0;
	strcpy(ret->Name, Name);
	ret->Link = NULL;
	
	if(prev) {
		ret->Next = prev->Next;
		prev->Next = ret;
	}
	else {
		if( gpCurUnit ) {
			ret->Next = gpCurUnit->Links;
			gpCurUnit->Links = ret;
		}
		else {
			ret->Next = gpLinks;
			gpLinks = ret;
		}
	}
	
	return ret;
}

/**
 * \brief Append a #defunit unit to the state
 */
tList *AppendUnit(tUnitTemplate *Unit, tList *Inputs)
{
	tLink	*link, *newLink;
	tElement	*ele;
	tList	*ret;
	 int	prefixLen = snprintf(NULL, 0, "%s#%i#", Unit->Name, Unit->InstanceCount);
	char	namePrefix[prefixLen+1];
	 int	i;
	
	if( Inputs->NItems != Unit->Inputs.NItems ) {
		// TODO: Error message?
		fprintf(stderr, "ERROR: %s takes %i lines, %i given\n",
			Unit->Name, Unit->Inputs.NItems, Inputs->NItems);
		return NULL;
	}
	
	snprintf(namePrefix, prefixLen+1, "%s#%i", Unit->Name, Unit->InstanceCount);
	Unit->InstanceCount ++;
	
	//printf("namePrefix = '%s'\n", namePrefix);
	
	// Create duplicates of all links (renamed)
	for( link = Unit->Links; link; link = link->Next )
	{
		tLink	*def, *prev = NULL;
		 int	len = 0;
		
		if( link->Name[0] )	len = prefixLen + strlen(link->Name);
		
		newLink = malloc( sizeof(tLink) + len + 1 );
		if( link->Name[0] ) {
			strcpy(newLink->Name, namePrefix);
			strcat(newLink->Name, link->Name);
			//printf("newLink->Name = '%s' ('%s' + '%s')\n",
			//	newLink->Name, namePrefix, link->Name);
		}
		else
			newLink->Name[0] = '\0';
		newLink->Link = NULL;
		newLink->Value = 0;
		
		link->Backlink = newLink;	// Set a back link to speed up later stuff
		
		// Append
		def = gpCurUnit ? gpCurUnit->Links : gpLinks;
		for( ; def; prev = def, def = def->Next )
		{
			if(strcmp(newLink->Name, def->Name) < 0)	break;
		}
		if(prev) {
			newLink->Next = prev->Next;
			prev->Next = newLink;
		}
		else {
			if( gpCurUnit ) {
				newLink->Next = gpCurUnit->Links;
				gpCurUnit->Links = newLink;
			}
			else {
				newLink->Next = gpLinks;
				gpLinks = newLink;
			}
		}
	}
	
	// Replace input lines
	for( i = 0; i < Unit->Inputs.NItems; i ++ )
	{
		link = Unit->Inputs.Items[i]->Backlink;
		if( link->Link )
			link->Link->Link = Inputs->Items[i];
		link->Link = Inputs->Items[i];
	}
	
	// Duplicate elements and replace links
	for( ele = Unit->Elements; ele; ele = ele->Next )
	{
		tElement	*newele;
		//printf("%p %p %i inputs\n", ele, ele->Type, ele->NInputs);
		newele = ele->Type->Create( ele->Param, ele->NInputs, ele->Inputs );
		newele->Type = ele->Type;
		newele->Param = ele->Param;
		newele->NInputs = ele->NInputs;
		for( i = 0; i < ele->NInputs; i ++ ) {
			//printf("%i: %p\n", i, ele->Inputs[i]);
			newele->Inputs[i] = ele->Inputs[i]->Backlink;
		}
		for( i = 0; i < ele->NOutputs; i ++ )
			newele->Outputs[i] = ele->Outputs[i]->Backlink;
		
		if( gpCurUnit ) {
			newele->Next = gpCurUnit->Elements;
			gpCurUnit->Elements = newele;
		}
		else {
			gpLastElement->Next = newele;
			gpLastElement = newele;
		}
	}
	
	// Create output return
	ret = malloc( sizeof(tList) + Unit->Outputs.NItems * sizeof(tLink*) );
	ret->NItems = Unit->Outputs.NItems;
	ret->Items = (void *)( (intptr_t)ret + sizeof(tList) );
	for( i = 0; i < Unit->Outputs.NItems; i ++ )
		ret->Items[i] = Unit->Outputs.Items[i]->Backlink;
	
	return ret;	
}

/**
 * \brief Create a new unit instance
 */
tList *CreateUnit(const char *Name, int Param, tList *Inputs)
{
	tElement	*ele;
	tList	*ret = NULL;
	 int	i;
	tElementDef	*def;
	tUnitTemplate	*tpl;
	
	// First, check for template units (#defunit)
	for( tpl = gpUnits; tpl; tpl = tpl->Next )
	{
		// We can't recurse, so also check that
		//printf("%p == %p && !strcmp('%s', '%s')\n", tpl, gpCurUnit,Name, tpl->Name);
		if( tpl != gpCurUnit && strcmp(Name, tpl->Name) == 0 ) {
			return AppendUnit(tpl, Inputs);
		}
	}
	
	// Next, check for builtins
	for( def = gpElementDefs; def; def = def->Next )
	{
		if( strcmp(Name, def->Name) == 0 )
			break;
	}
	if(!def)	return NULL;
	
	if( Inputs->NItems < def->MinInput ) {
		printf("%s requires at least %i input lines\n", def->Name, def->MinInput);
		return NULL;
	}
	if( def->MaxInput != -1 && Inputs->NItems > def->MaxInput ) {
		printf("%s takes at most %i input lines\n", def->Name, def->MaxInput);
		return NULL;
	}
	
	// Create an instance
	ele = def->Create(Param, Inputs->NItems, Inputs->Items);
	if(!ele)	return NULL;
	ele->Type = def;	// Force type to be set
	ele->Param = Param;
	for( i = 0; i < Inputs->NItems; i ++ ) {
		//printf("%i = %p\n", i, Inputs->Items[i]);
		ele->Inputs[i] = Inputs->Items[i];
	}
	
	// Append to element list
	if( gpCurUnit ) {
		ele->Next = gpCurUnit->Elements;
		gpCurUnit->Elements = ele;
	}
	else {
		gpLastElement->Next = ele;
		gpLastElement = ele;
	}
	
	// Create return list
	ret = malloc( sizeof(tList) + ele->NOutputs*sizeof(tLink*) );
	ret->NItems = ele->NOutputs;
	ret->Items = (void *)( (intptr_t)ret + sizeof(tList) );
	for( i = 0; i < ele->NOutputs; i ++ ) {
		ele->Outputs[i] = CreateAnonLink();
		ret->Items[i] = ele->Outputs[i];
		//printf(" ret->Items[%i] = %p\n", i, ret->Items[i]);
	}
	
	return ret;
}

/**
 */
void List_Free(tList *List)
{
	if( List->Items != (void *)( (intptr_t)List + sizeof(tList) ) )
		free(List->Items);
	else
		free(List);
}

/**
 * \brief Append one list to another
 */
void AppendList(tList *Dest, tList *Src)
{
	 int	oldOfs = Dest->NItems;
	Dest->NItems += Src->NItems;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	memcpy( &Dest->Items[oldOfs], Src->Items, Src->NItems*sizeof(tLink*) );
}

/**
 * \brief Append a named line to a list
 */
void AppendLine(tList *Dest, const char *Name)
{
	Dest->NItems ++;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	Dest->Items[Dest->NItems-1] = CreateNamedLink(Name);
}

/**
 * \brief Append a single line of a group to a list
 */
int AppendGroupItem(tList *Dest, const char *Name, int Index)
{
	 int	len;
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
	
	// Get string length
	len = snprintf(NULL, 0, "%s[%i]", Name, Index);
	
	{
		char	newname[len + 1];
		snprintf(newname, len+1, "%s[%i]", Name, Index);
		
		Dest->NItems ++;
		Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
		Dest->Items[Dest->NItems-1] = CreateNamedLink(newname);
	}
	return 0;
}

/**
 * \brief Append every line in a group to a list
 */
int AppendGroup(tList *Dest, const char *Name)
{
	tGroupDef	*grp = GetGroup(Name);
	 int	i, ofs, len;
	
	//printf("AppendGroup: (%p, %s)\n", Dest, Name);
	if(!grp) {
		// ERROR:
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	len = snprintf(NULL, 0, "%s[%i]", Name, grp->Size-1);
	
	ofs = Dest->NItems;
	Dest->NItems += grp->Size;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	
	for( i = 0; i < grp->Size; i ++ )
	{
		char	newname[len + 1];
		snprintf(newname, 100, "%s[%i]", Name, i);
		
		Dest->Items[ofs+i] = CreateNamedLink(newname);
	}
	
	return 0;
}

/**
 * \brief Link two sets of links (Sets the aliases for \a Src to \a Dest)
 * \return Boolean Failure
 */
int MergeLinks(tList *Dest, tList *Src)
{
	 int	i;
	if( Dest->NItems != Src->NItems ) {
		fprintf(stderr, "ERROR: MergeLinks - Mismatch of source and destination counts\n");
		return -1;
	}
	
	for( i = 0; i < Dest->NItems; i ++ )
	{
		if( Src->Items[i]->Link )
			Src->Items[i]->Link->Link = Dest->Items[i];
		Src->Items[i]->Link = Dest->Items[i];
	}
	return 0;
}
