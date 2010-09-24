/*
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include <common.h>
#include <unistd.h>

/**
 */
typedef struct
{
	const char	*DataStart;

	jmp_buf	jmpbuf;

	char	*File;
	 int	Line;
	
	const char	*CurPos;
	
	 int	Token;
	char	*TokenStr;
	
	const char	*TokenStart;
	 int	TokenLength;
	
	struct {
		const char	*Pos;
		 int	Line;
		 int	Token;
		const char	*TokenStart;
		 int	TokenLength;
	}	Saved;
	
	char	_static[15+1];
}	tParser;

enum eTokens
{
	TOK_NULL,
	TOK_EOF,
	
	TOK_META_STATEMENT,
	TOK_NUMBER,
	TOK_IDENT,
	TOK_STRING,
	
	TOK_LINE,	// $aaaa
	TOK_GROUP,	// @aaaa
	
	TOK_NEWLINE,
	TOK_COMMA,
	TOK_ASSIGN,
	
	TOK_PAREN_OPEN, TOK_PAREN_CLOSE,
	TOK_BRACE_OPEN, TOK_BRACE_CLOSE,
	TOK_SQUARE_OPEN,TOK_SQUARE_CLOSE,
	
	TOK_T_VALUE
};

const char * const casTOKEN_NAMES[] = {
	"TOK_NULL", "TOK_EOF",
	"TOK_META_STATEMENT", "TOK_NUMBER", "TOK_IDENT", "TOK_STRING",
	"TOK_LINE", "TOK_GROUP", "TOK_NEWLINE", "TOK_COMMA", "TOK_ASSIGN",
	"TOK_PAREN_OPEN", "TOK_PAREN_CLOSE",
	"TOK_BRACE_OPEN", "TOK_BRACE_CLOSE",
	"TOK_SQUARE_OPEN","TOK_SQUARE_CLOSE",
	
	"TOK_LINE, TOK_GROUP, TOK_NUMBER or TOK_PAREN_OPEN"
};

// === PROTOTYPES ===
 int	ParseValue(tParser *Parser, tList *destList);
void	*ParseOperation(tParser *Parser);
 int	ParseLine(tParser *Parser);
 int	ParseFile(const char *Filename);
 int	is_ident(char ch);
 int	GetToken(tParser *Parser);
void	PutBack(tParser *Parser);
void	SyntaxError(tParser *Parser, const char *Fmt, ...);
void	SyntaxWarning(tParser *Parser, const char *Fmt, ...);
void	SyntaxAssert(tParser *Parser, int Got, int Expected);

// === CODE ===
/**
 * \brief Parse a "value" (Constant, Line or Group)
 * \return Zero on success, 1 if no value is found
 */
int ParseValue(tParser *Parser, tList *destList)
{
	char	*tmpName;
	
	GetToken(Parser);
	switch( Parser->Token )
	{
	case TOK_LINE:
		AppendLine(destList, Parser->TokenStr);
		break;
	
	// Group (@name)
	case TOK_GROUP:
		tmpName = strdup(Parser->TokenStr);
		// Single line? (@group[i])
		if( GetToken(Parser) == TOK_SQUARE_OPEN ) {
			SyntaxAssert(Parser, GetToken(Parser), TOK_NUMBER);
			if( AppendGroupItem(destList, tmpName, atoi(Parser->TokenStr)) )
				SyntaxError(Parser, "Error referencing group item");
			SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_CLOSE);
		}
		// Entire group.
		else {
			PutBack(Parser);
			if( AppendGroup(destList, tmpName) )
				SyntaxError(Parser, "Error referencing group");
		}
		free(tmpName);
		break;
	
	// Constant
	// TODO: Implement
	case TOK_NUMBER:
		{
			 int	num = atoi(Parser->TokenStr);
			if( num < 0 || num > 1 ) {
				SyntaxWarning(Parser, "Non-boolean constant value (%i) used\n", num);
			}
			if( num == 0 )
				AppendLine( destList, "0" );
			else
				AppendLine( destList, "1" );
		}
		break;
	
	case TOK_PAREN_OPEN:
		{
		tList	*tmplist;
		tmplist = ParseOperation(Parser);
		AppendList(destList, tmplist);
		List_Free(tmplist);
		SyntaxAssert(Parser, GetToken(Parser), TOK_PAREN_CLOSE);
		}
		break;
	
	default:
		return 1;
	}
	
	return 0;
}

/**
 * \brief Parse a gate definition
 */
void *ParseOperation(tParser *Parser)
{
	char	*name;
	tList	inputs = {0};
	 int	param = -1;
	
	// Get Name
	if( ParseValue(Parser, &inputs) == 0 )
	{
		tList *ret = calloc(1, sizeof(tList));
		AppendList(ret, &inputs);
		return ret;
	}
	
	if( Parser->Token != TOK_IDENT )
	{
		SyntaxError(Parser,
			"Unexpected %s, expected TOK_IDENT/TOK_NUMBER/TOK_LINE/TOK_GROUP",
			casTOKEN_NAMES[ Parser->Token ]
			);
	}
	
	name = strdup( Parser->TokenStr );
	
	// Static Paramater (e.g. DELAY{4})
	if( GetToken(Parser) == TOK_BRACE_OPEN )
	{
		SyntaxAssert( Parser, GetToken(Parser), TOK_NUMBER );
		param = atoi( Parser->TokenStr );
		SyntaxAssert( Parser, GetToken(Parser), TOK_BRACE_CLOSE );
	}
	else
		PutBack(Parser);
	
	// Input lines
	if( GetToken(Parser) != TOK_NEWLINE )
	{
		PutBack(Parser);
		do {
			if( ParseValue(Parser, &inputs) != 0) {
				SyntaxError(Parser,
					"Unexpected %s, expected TOK_NUMBER, TOK_LINE, TOK_GROUP or TOK_PAREN_OPEN",
					casTOKEN_NAMES[ Parser->Token ]);
			}
			
			GetToken(Parser);
		} while(Parser->Token == TOK_COMMA);
		PutBack(Parser);
	}
	else {
		PutBack(Parser);
	}
	
	// Create output
	{
		tList	*ret = CreateUnit( name, param, &inputs );
		if(!ret) {
			free(inputs.Items);
			SyntaxError(Parser, "Unknown unit %s", name);
		}
		free(name);
		free(inputs.Items);
		return ret;
	}
}

/**
 * \brief Parse a line
 */
int ParseLine(tParser *Parser)
{
	tList	destArray = {0};
	tList	*outputs;
	
	switch(GetToken(Parser))
	{
	// Empty Line
	case TOK_NEWLINE:	return 0;
	// End of file
	case TOK_EOF:	return 1;
	
	// Meta statement
	case TOK_META_STATEMENT:
		// #array <name> <size>
		// - Define a group of lines with <size> lines in them
		// (Defines @<name>[0] to @<name>[<size>-1])
		if( strcmp(Parser->TokenStr, "#array") == 0 ) {
			char	*name;
			 int	len;
			SyntaxAssert(Parser, GetToken(Parser), TOK_IDENT );
			name = strndup( Parser->TokenStart, Parser->TokenLength );
			SyntaxAssert(Parser, GetToken(Parser), TOK_NUMBER );
			len = atoi( Parser->TokenStr );
			
			printf("Create Group @%s[%i]\n", name, len);
			CreateGroup( name, len );
			free( name );
		}
		// Define a unit
		else if( strcmp(Parser->TokenStr, "#defunit") == 0 ) {
			SyntaxAssert(Parser, GetToken(Parser), TOK_IDENT );
			Unit_DefineUnit( Parser->TokenStr );
		}
		// Close a unit definition
		else if( strcmp(Parser->TokenStr, "#endunit") == 0 ) {
			if( !Unit_IsInUnit() )
				SyntaxError(Parser, "#endunit used not in a unit");
			Unit_CloseUnit();
		}
		// Set input lines
		else if( strcmp(Parser->TokenStr, "#input") == 0 ) {
			if( !Unit_IsInUnit() )
				SyntaxError(Parser, "#input used not in a unit");
			do {
				GetToken(Parser);
				switch( Parser->Token )
				{
				case TOK_LINE:
					Unit_AddSingleInput( Parser->TokenStr );
					break;
				case TOK_GROUP:
					{
					char	name[Parser->TokenLength+1];
					strcpy(name, Parser->TokenStr);
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_OPEN);
					SyntaxAssert(Parser, GetToken(Parser), TOK_NUMBER);
					Unit_AddGroupInput( name, atoi(Parser->TokenStr) );
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_CLOSE);
					}
					break;
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);
		}
		// Set output lines
		else if( strcmp(Parser->TokenStr, "#output") == 0 ) {
			// Check for unit
			if( !Unit_IsInUnit() )
				SyntaxError(Parser, "#output used not in a unit");
			// Comma separated list of lines
			do {
				GetToken(Parser);
				switch( Parser->Token )
				{
				// Single
				// "$line"
				case TOK_LINE:
					Unit_AddSingleOutput( Parser->TokenStr );
					break;
				// Group (with size)
				// "@group[size]"
				case TOK_GROUP:
					{
					char	name[Parser->TokenLength+1];
					strcpy(name, Parser->TokenStr);
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_OPEN);
					SyntaxAssert(Parser, GetToken(Parser), TOK_NUMBER);
					Unit_AddGroupOutput( name, atoi(Parser->TokenStr) );
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_CLOSE);
					}
					break;
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			
			PutBack(Parser);	// Put back non-comma character
		}
		// Set display values
		else if( strcmp(Parser->TokenStr, "#display") == 0 ) {
			tList	cond = {0}, values = {0};
			char	*title;
			// Condition - Single value (well, should be :)
			ParseValue(Parser, &cond);
			// Display Title - String
			SyntaxAssert(Parser, GetToken(Parser), TOK_STRING);
			title = strdup(Parser->TokenStr);
			// Comma separated list of values to print
			do {
				if( ParseValue(Parser, &values) ) {
					SyntaxError(Parser,
						"Unexpected %s, expected %s",
						casTOKEN_NAMES[TOK_T_VALUE], casTOKEN_NAMES[ Parser->Token ]);
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);	// Put back non-comma character
			
			// Remove quotes
			title[strlen(title)-1] = '\0';
			title ++;
			
			AddDisplayItem(title, &cond, &values);
		
			title --;
		
			free(title);
			List_Free(&cond);
			List_Free(&values);
		}
		else {
			SyntaxError(Parser, "Unknown meta-statement '%s'",
				Parser->TokenStr);
			return -1;
		}
		SyntaxAssert(Parser, GetToken(Parser), TOK_NEWLINE );
		return 0;
	}
	
	// If there are parameters
	if( Parser->Token != TOK_IDENT )
	{
		// Standard Assignment Line
		PutBack(Parser);
		while( ParseValue(Parser, &destArray) == 0 )
		{
			if( GetToken(Parser) != TOK_COMMA )
				break;
		}
		
		// Check for assignmenr
		if( Parser->Token != TOK_ASSIGN ) {
			SyntaxError(Parser, "Expected '=', found %s '%s'",
				casTOKEN_NAMES[Parser->Token], Parser->TokenStr);
			return -1;
		}
	}
	
	// Get gate (or gate chain)
	outputs = ParseOperation( Parser );
	
	// Assign
	if( MergeLinks( &destArray, outputs ) )
		SyntaxError(Parser,
			"Mismatch of left and right counts (%i != %i)",
			destArray.NItems, outputs->NItems
			);
	
	// Ensure a newline
	SyntaxAssert( Parser, GetToken(Parser), TOK_NEWLINE );
	
	// Clean up
	List_Free(outputs);
	free(destArray.Items);
	
	return 0;
}

/**
 * \brief Create a heap string using \a fmt
 */
char *MakeFmtStr( const char *fmt, ... )
{
	 int	len;
	va_list	args, args2;
	char	*ret;
	
	va_start(args, fmt);
	
	va_copy(args2, args);
	len = vsnprintf(NULL, 0, fmt, args2);
	
	ret = malloc( len + 1 );
	vsnprintf(ret, len+1, fmt, args);
	
	va_end(args);
	return ret;
}

/**
 * \brief Parse a file
 */
int ParseFile(const char *Filename)
{
	tParser	parser;
	 int	flen;
	FILE	*fp;
	char	*buf;
	
	#if 1
	{
	char	tmpFileName[] = "/tmp/logic_cct.cct.XXXXXX";
	char	*cmdString;
	
	char	*fname = strrchr(Filename, '/');
	 int	pathLen = (intptr_t)fname - (intptr_t)Filename + 1;
	char	*path = malloc( pathLen + 1 );
	memcpy(path, Filename, pathLen);
	path[pathLen] = '\0';
	
	close( mkstemp(tmpFileName) );
	
	//cmdString = MakeFmtStr( "nasm -e \"%s\" -i \"%s\" | grep -v \"^%\" > \"%s\"", Filename, path, tmpFileName );
	//cmdString = MakeFmtStr( "yasm -e \"%s\" | grep -v \"^%%\" > \"%s\"", Filename, tmpFileName );
	cmdString = MakeFmtStr( "yasm -e \"%s\" > \"%s\"", Filename, tmpFileName );
	printf("%s\n", cmdString);
	fflush(stdout);
	
	system(cmdString);
	free(cmdString);
	free(path);
	fp = fopen(tmpFileName, "r");
	unlink( tmpFileName );
	}
	#else
	fp = fopen(Filename, "r");
	#endif
	
	if(!fp) {
		perror("Unable to open file");
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	buf = malloc( flen + 1 );
	fread(buf, 1, flen, fp);
	buf[flen] = '\0';
	
	fclose(fp);
	
	//printf("\n----\n%s\n----\n", buf);
	
	parser.File = strdup(Filename);
	parser.Line = 1;
	parser.TokenStr = parser._static;
	parser.DataStart = buf;
	parser.CurPos = parser.DataStart;
	
	if( setjmp( parser.jmpbuf ) != 0 ) {
		free( buf );
		return -1;
	}
	
	while( ParseLine( &parser ) == 0 );
	
	free( (char*)parser.File );
	free( buf );
	return 0;
}

/**
 * \brief Read a token from a parser state
 */
int GetToken(tParser *Parser)
{
	char	ch;
	 int	ret;
	
	// Wait until all "whitespace" is read
	for( ;; )
	{		
		// Whitespace
		while( isspace(*Parser->CurPos) && *Parser->CurPos != '\n' )
			Parser->CurPos ++;
		// Allow escaping of newlines
		if( *Parser->CurPos == '\\' && Parser->CurPos[1] == '\n' ) {
			Parser->CurPos += 2;
			continue;
		}
		
		#if 1
		// ASM Comments
		if( *Parser->CurPos == ';' ) {
			while( *Parser->CurPos != '\n' )
				Parser->CurPos ++;
			continue ;
		}
		#endif
		
		#if 1
		// Preprocessor Comments
		if( *Parser->CurPos == '%' ) {
			Parser->CurPos ++;
			if( strncmp("line ", Parser->CurPos, 5) == 0 ) {
				char	newFile[128];
				 int	line, unk;
				sscanf(Parser->CurPos, "line %i+%i %s\n", &line, &unk, newFile);
				//printf("line = %i, unk = %i, newFile='%s'\n", line, unk, newFile);
				free( Parser->File );
				Parser->File = strdup(newFile);
				Parser->Line = line;
			}
			
			while( *Parser->CurPos != '\n' )
				Parser->CurPos ++;
			Parser->CurPos ++;
			continue ;
		}
		#endif
		
		// C++ Comments
		if( *Parser->CurPos == '/' && Parser->CurPos[1] == '/' ) {
			while( *Parser->CurPos != '\n' )
				Parser->CurPos ++;
			continue ;
		}
		
		break;
	}
	
	// Save
	Parser->Saved.Line = Parser->Line;
	Parser->Saved.Pos = Parser->CurPos;
	Parser->Saved.Token = Parser->Token;
	Parser->Saved.TokenStart = Parser->TokenStart;
	Parser->Saved.TokenLength = Parser->TokenLength;
	
	Parser->TokenStart = Parser->CurPos;
	ch = *Parser->CurPos;
	Parser->CurPos ++;
	
	switch( ch )
	{
	case '\0':
		ret = TOK_EOF;
		break;
		
	case '\n':
		Parser->Line ++;
		//printf("Line %i\n", Parser->Line);
		ret = TOK_NEWLINE;
		break;
	
	// Meta-Statement (Definition, Etc)
	case '#':
		// Read identifier
		while( isalpha( *Parser->CurPos ) )
			Parser->CurPos ++;
		ret = TOK_META_STATEMENT;
		break;
	
	// Single Line
	case '$':
		while( is_ident( *Parser->CurPos ) )
			Parser->CurPos ++;
		ret = TOK_LINE;
		break;
	
	// Line Group
	case '@':
		while( is_ident( *Parser->CurPos ) )
			Parser->CurPos ++;
		ret = TOK_GROUP;
		break;
	
	// Group Index
	case '[':	// Square Open
		ret = TOK_SQUARE_OPEN;
		break;
	case ']':	// Square Close
		ret = TOK_SQUARE_CLOSE;
		break;
	
	// Arguments for builtins
	case '{':	// Brace Open
		ret = TOK_BRACE_OPEN;
		break;
	case '}':	// Brace Close
		ret = TOK_BRACE_CLOSE;
		break;
	
	// Grouping
	case '(':	// Paren Open
		ret = TOK_PAREN_OPEN;
		break;
	case ')':	// Paren Close
		ret = TOK_PAREN_CLOSE;
		break;
	
	// Argument Separation
	case ',':	// Comma
		ret = TOK_COMMA;
		break;
		
	// Assignment
	case '=':	// Assign Equals
		ret = TOK_ASSIGN;
		break;
	
	// String
	case '"':
		while( *Parser->CurPos != '"' )	Parser->CurPos ++;
		Parser->CurPos ++;
		ret = TOK_STRING;
		break;
	
	// Default
	default:
		if( '0' <= ch && ch <= '9' ) {
			// Numeric (Bin, Oct, Dec, Hex)
			// 0b1111, 017, 15, 0xF
			while( isalnum( *Parser->CurPos ) )
				Parser->CurPos ++;
			ret = TOK_NUMBER;
			break;
		}
		if( ch == '_' || ('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') ) {
			while( is_ident( *Parser->CurPos ) )
				Parser->CurPos ++;
			ret = TOK_IDENT;
			break;
		}
		ret = TOK_NULL;
		break;
	}
	
	Parser->TokenLength = Parser->CurPos - Parser->TokenStart;
	Parser->Token = ret;
	
	if( Parser->TokenStr != Parser->_static )
		free( Parser->TokenStr );
	
	if(Parser->TokenLength <= 15) {
		memcpy(Parser->_static, Parser->TokenStart, Parser->TokenLength);
		Parser->_static[ Parser->TokenLength ] = '\0';
		Parser->TokenStr = Parser->_static;
	}
	else {
		Parser->TokenStr = strndup(Parser->TokenStart, Parser->TokenLength);
	}
	
	//printf("%s\n", casTOKEN_NAMES[ret]);
	
	return ret;
}

/**
 * \brief Put back a read token
 */
void PutBack(tParser *Parser)
{
	Parser->Line = Parser->Saved.Line;
	Parser->CurPos = Parser->Saved.Pos;
	Parser->Token = Parser->Saved.Token;
	Parser->TokenStart = Parser->Saved.TokenStart;
	Parser->TokenLength = Parser->Saved.TokenLength;
	
	if( Parser->TokenStr != Parser->_static )
		free( Parser->TokenStr );
	
	if(Parser->TokenLength <= 15) {
		memcpy(Parser->_static, Parser->TokenStart, Parser->TokenLength);
		Parser->_static[ Parser->TokenLength ] = '\0';
		Parser->TokenStr = Parser->_static;
	}
	else {
		Parser->TokenStr = strndup(Parser->TokenStart, Parser->TokenLength);
	}
}

/**
 */
int is_ident(char ch)
{
	if('0' <= ch && ch <= '9')	return 1;
	if('A' <= ch && ch <= 'Z')	return 1;
	if('a' <= ch && ch <= 'z')	return 1;
	if('_' == ch)	return 1;
	return 0;
}

/**
 * \brief Print a syntax error
 */
void SyntaxError(tParser *Parser, const char *Fmt, ...)
{
	va_list	args;
	va_start(args, Fmt);
	fprintf(stderr, "%s:%i: error: ", Parser->File, Parser->Line);
	vfprintf(stderr, Fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	longjmp(Parser->jmpbuf, 1);
}

/**
 * \brief Print a warning message
 */
void SyntaxWarning(tParser *Parser, const char *Fmt, ...)
{
	va_list	args;
	va_start(args, Fmt);
	fprintf(stderr, "%s:%i: warning: ", Parser->File, Parser->Line);
	vfprintf(stderr, Fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/**
 * \brief Assertion with a syntax error
 */
void SyntaxAssert(tParser *Parser, int Got, int Expected)
{
	if( Got != Expected ) {
		SyntaxError(Parser, "Expected %s, got %s '%s'",
			casTOKEN_NAMES[Expected], casTOKEN_NAMES[Got],
			Parser->TokenStr);
	}
}
