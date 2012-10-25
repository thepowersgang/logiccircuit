/*
 * LogicCircuit
 * By John Hodge (thePowersGang)
 *
 * compiled.c
 * - Reads/writes compiled versions of nets
 */
#include <common.h>
#include <stdio.h>
#include <stdlib.h>	// malloc
#include <stdint.h>
#include <string.h>
#include "include/compiled.h"

extern tElementDef	*gpElementDefs;

void WriteCompiledVersion(const char *Path, int bBinary)
{
	 int	n_links = 0;
	 int	n_eletypes = 0;
	 int	n_elements = 0;
	FILE	*fp;
	tLink	*link;
	tElement	*ele;
	 int	i;
	
	for( link = gpLinks; link; link = link->Next ) {
		link->Backlink = (void*)(intptr_t)n_links;	// Save
		n_links ++;
	}
	for( ele = gpElements; ele; ele = ele->Next )
		n_elements ++;
	for( tElementDef *ed = gpElementDefs; ed; ed = ed->Next )
		n_eletypes ++;

	void _Write16(FILE *fp, uint16_t val) {
		fputc(val & 0xFF, fp);
		fputc(val >> 8, fp);
	}
	void _Write32(FILE *fp, uint32_t val) {
		_Write16(fp, val & 0xFFFF);
		_Write16(fp, val >> 16);
	}
	
	fp = fopen(Path, "wb");
	
	if( bBinary ) {
		_Write16(fp, n_links);
		_Write16(fp, n_eletypes);
		_Write16(fp, n_elements);

		// Element type names
		for( tElementDef *ed = gpElementDefs; ed; ed = ed->Next ) {
			fputc(strlen(ed->Name), fp);
			fwrite(ed->Name, 1, strlen(ed->Name), fp);
		}
	}
	else {
		fprintf(fp, "%04x %04x\n", n_links, n_elements);
	}
	
	// Element info
	for( ele = gpElements; ele; ele = ele->Next )
	{
		if( bBinary ) {
			 int	typeid = 0;
			for( tElementDef *ed = gpElementDefs; ed && ed != ele->Type; ed = ed->Next )
				typeid ++;
			_Write16(fp, typeid);
			_Write16(fp, ele->NInputs);
			_Write16(fp, ele->NOutputs);
			_Write16(fp, ele->NParams);
			for( i = 0; i < ele->NInputs; i ++ )
				_Write16(fp, (intptr_t)ele->Inputs[i]->Backlink);
			for( i = 0; i < ele->NOutputs; i ++ )
				_Write16(fp, (intptr_t)ele->Outputs[i]->Backlink);
			for( i = 0; i < ele->NParams; i ++ )
				_Write32(fp, ele->Params[i]);
		}
		else {
			fprintf(fp,
				"%s %02x %02x %02x",
				ele->Type->Name, ele->NOutputs, ele->NParams, ele->NInputs
				);
			fprintf(fp, " |");
			for( i = 0; i < ele->NParams; i ++ )
				fprintf(fp, " %x", ele->Params[i]);
			fprintf(fp, " |");
			for( i = 0; i < ele->NInputs; i ++ )
				fprintf(fp, " %04x", (int)(intptr_t)ele->Inputs[i]->Backlink);
			fprintf(fp, " |");
			for( i = 0; i < ele->NOutputs; i ++ )
				fprintf(fp, " %04x", (int)(intptr_t)ele->Outputs[i]->Backlink);
			fprintf(fp, "\n");
		}
	}
	
	fclose(fp);
}

void ReadCompiledVersion(const char *Path, int bBinary)
{
	int n_links, n_elements;
	char	**elenames;
	 int	n_elenames;
	FILE	*fp;

	tElement	*first_ele = NULL;
	tElement	*ele, *last_ele = NULL;
	tLink	*links;

	uint16_t _Read16(FILE *FP) {
		uint16_t rv = fgetc(FP);
		rv |= (uint16_t)fgetc(FP) << 8;
		return rv;
	}
	uint32_t _Read32(FILE *FP) {
		uint32_t rv = _Read16(FP);
		rv |= (uint32_t)_Read32(FP) << 16;
		return rv;
	}

	if( bBinary )
		fp = fopen(Path, "rb");
	else
		fp = fopen(Path, "r");

	if( bBinary )
	{
		n_links    = _Read16(fp);
		n_elements = _Read16(fp);
		n_elenames = _Read16(fp);
		
		elenames = malloc( n_elenames * sizeof(elenames[0]));
		if( !elenames ) {
			// TODO: Sad
			goto _err;
		}
		for( int i = 0; i < n_elenames; i ++ )
		{
			int len = fgetc(fp);
			elenames[i] = malloc( len + 1 );
			fread(elenames[i], 1, len, fp);
			elenames[i][len] = '\0';
		}
	}
	else
	{
		fscanf(fp, "%04x %04x\n", &n_links, &n_elements);
	}
	
	// Create links
	links = calloc( n_links, sizeof(tLink) );
	for( int i = 0; i < n_links-1; i ++ )
		links[i].Next = &links[i+1];
	
	// Create elements
	for( int i = 0; i < n_elements; i ++ )
	{
		char	txtname[16];
		char	*name;
		int	params[MAX_PARAMS];
		 int	n_params, n_input, n_output;
		
		if( bBinary ) {
			int typeid;
			typeid   = _Read16(fp);
			n_params = _Read16(fp);
			n_input  = _Read16(fp);
			n_output = _Read16(fp);
			if( n_params > MAX_PARAMS )
				n_params = MAX_PARAMS;
			for( i = 0; i < n_params; i ++ )
				params[i] = _Read32(fp);
			if( typeid >= n_elenames )
				name = "";
			else
				name = elenames[typeid];
		}
		else {
			fscanf(fp, "%16s %x %x %x", txtname, &n_params, &n_input, &n_output);
			name = txtname;
			if( n_params > MAX_PARAMS )
				n_params = MAX_PARAMS;
			fscanf(fp, " |");
			for( i = 0; i < n_params; i ++ )
				fscanf(fp, " %x", &params[i]);
			fscanf(fp, "\n");
		}

		// Locate type
		tElementDef *def;
		for( def = gpElementDefs; def; def = def->Next )
		{
			if( strcmp(name, def->Name) == 0 )
				break;
		}
		if(!def) {
			fprintf(stderr, "Unknown unit '%s'\n", name);
			goto _err;
		}
	
		// Create unit
		ele = def->Create(n_params, params, n_input, NULL);
		if( !ele ) {
			fprintf(stderr, "Unit %s errored\n", name);
			goto _err;
		}
		if( ele->NOutputs != n_output ) {
			fprintf(stderr, "%s with %i inputs did not return %i outputs (%i instead)\n",
				name, n_input, n_output, ele->NOutputs
				);
			goto _err;
		}

		// Link up inputs/outputs
		if( bBinary ) {
			for( i = 0; i < ele->NInputs; i ++ ) {
				int idx = _Read16(fp);
				ele->Inputs[i] = &links[idx];
			}
			for( i = 0; i < ele->NOutputs; i ++ ) {
				int idx = _Read16(fp);
				ele->Outputs[i] = &links[idx];
			}
		}
		else {
			fscanf(fp, " |");
			for( i = 0; i < ele->NInputs; i ++ ) {
				int idx;
				fscanf(fp, " %x", &idx);
				ele->Inputs[i] = &links[idx];
			}
			fscanf(fp, " |");
			for( i = 0; i < ele->NOutputs; i ++ ) {
				int idx;
				fscanf(fp, " %x", &idx);
				ele->Outputs[i] = &links[idx];
			}
			fscanf(fp, "\n");
		}
		
		// Append to list
		if( last_ele )
			last_ele->Next = ele;
		else
			first_ele = ele;
		last_ele = ele;
	}
	
	links[n_links-1].Next = gpLinks;
	gpLinks = links;
	last_ele->Next = gpElements;
	gpElements = last_ele;
_err:
	if( bBinary )
		free(elenames);
	free(links);
	fclose(fp);
	return ;
}
