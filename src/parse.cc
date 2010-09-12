/*
 *
 */
#incluce <cstdio>
#incluce <cstdlib>
#include <Class.Link.hpp>

/**
 */
Link **ParseAction(const char *Data, int *Length)
{
	const char	*str = Data;
	const char	*name;
	
	while( isspace(*str) )	str ++;
	
	
	
	// Plain link
	if( *str == '$' ) {
		name = str;
		while( isalpha(*str) || isdigit(*str) )	str ++;
		
		ret = malloc( sizeof(Link*)*2 );
		if(!ret) {
			perror("ParseAction, malloc(sizeof(Link*)*2) ");
			return NULL;
		}
		
		{
			 int	nameLen = (intptr_t)str - (intptr_t)name;
			char	nameStr[str-name + 1];
			memcpy(nameStr, name, nameLen);
			nameStr[nameLen] = 0;
			
			ret[0] = new Link(nameStr);
			ret[1] = NULL;
		}
		
		*Length = str-Data;
		return ret;
	}
	
	// Array
	if(*str == '@') {
		*Length = str-Data;
	}
	
	// Another gate
	if(*str == '(') {
		*Length = str-Data;
	}
	
	// error
	
	return NULL;
}
