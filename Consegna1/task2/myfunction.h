#include <ctype.h>
#include <stddef.h>

size_t countStrLen(char *str){
	size_t c = 0;
	while(*str != '\0'){
		c += 1;
		str++;
	}
	return c;
}

void printData(char *str, size_t numBytes){
	int i = 0;
	for(i = 0; i < numBytes; i++){
		printf("%c", str[i]);
	}
	printf("\n");
}

void convertToUpperCase(char *str, size_t numBytes){
	int i = 0;
	for(i = 0; i < numBytes; i++){
		str[i] = toupper(str[i]);
	}
}
