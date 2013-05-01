/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 * 
 * lex.c
 * - Lexer
 */
#include "parse.h"
#include <stdlib.h>
#include <stdio.h>	// sscanf
#include <string.h>
#include <ctype.h>

// === PROTOTYPES ===
 int	GetToken(tParser *Parser);
void	PutBack(tParser *Parser);
 int	is_ident(char ch);

// === CODE ===
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
		
		// ASM-style Comments
		// NOTE: `yasm -e` kills these befire we see them
		if( *Parser->CurPos == ';' ) {
			while( *Parser->CurPos != '\n' )
				Parser->CurPos ++;
			continue ;
		}
		
		#if 1
		// NASM/YASM Preprocessor Comments
		if( *Parser->CurPos == '%' ) {
			Parser->CurPos ++;
			if( strncmp("line ", Parser->CurPos, 5) == 0 ) {
				char	newFile[128];
				 int	line, unk;
				sscanf(Parser->CurPos, "line %i+%i %s\n", &line, &unk, newFile);
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
	
	case '\\':
		if( *Parser->CurPos == '\\' ) {
			Parser->CurPos ++;
			ret = TOK_NEWLINE;
			break;
		}
		ret = TOK_NULL;
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
	
	// Argument Separation
	case ':':	// Colon
		ret = TOK_COLON;
		break;
		
	// Assignment
	case '=':	// Assign Equals
		ret = TOK_ASSIGN;
		break;
	
	case '+':	ret = TOK_PLUS;	break;
	case '-':	ret = TOK_MINUS;	break;
	case '*':	ret = TOK_STAR;	break;
	case '/':	ret = TOK_SLASH;	break;
	
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
	
	return ret;
}


const char *GetTokenStr(enum eTokens Token)
{
	#define _ent(val)	case val: return #val;
	switch(Token)
	{
	_ent(TOK_NULL)
	_ent(TOK_EOF)
	_ent(TOK_META_COMMENT)
	_ent(TOK_META_STATEMENT)
	_ent(TOK_NUMBER)
	_ent(TOK_IDENT)
	_ent(TOK_STRING)
	
	_ent(TOK_LINE)	// $aaaa
	_ent(TOK_GROUP)	// @aaaa
	
	_ent(TOK_NEWLINE)
	_ent(TOK_COMMA)
	_ent(TOK_ASSIGN)
	_ent(TOK_COLON)
	
	_ent(TOK_PLUS)
	_ent(TOK_MINUS)
	_ent(TOK_STAR)
	_ent(TOK_SLASH)
	
	_ent(TOK_PAREN_OPEN)
	_ent(TOK_PAREN_CLOSE)
	_ent(TOK_BRACE_OPEN)
	_ent(TOK_BRACE_CLOSE)
	_ent(TOK_SQUARE_OPEN)
	_ent(TOK_SQUARE_CLOSE)
	case TOK_T_VALUE:	return "TOK_LINE, TOK_GROUP, TOK_NUMBER or TOK_PAREN_OPEN";
	case TOK_T_LINE:	return "TOK_LINE, TOK_GROUP";
	}
	return "-TOK_UNK-";
	#undef _ent
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

	if( !Parser->TokenLength )
		return ;
	
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


