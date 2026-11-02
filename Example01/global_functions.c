#include "globals.h"

char *look_for_str(char *source_str, char *match_str) {
	char *ret_val;
	int j, k;
	int length_str1;
	int length_str2;
	int found;
	int pos_start;
	length_str1 = strlen(source_str);
	length_str2 = strlen(match_str);

	found = 2;

	for (j = 0; j < length_str1 && found != 0; j++) {
		pos_start = j;
		for (k = 0, found = 0; k < length_str2; k++) {
			if (*(source_str + j + k) != *(match_str + k)) {
				found = 1;
				break;
			}
			//else
			//	j++;

		}

	}
	ret_val = (source_str + pos_start);
	if (found == 0) {

		return (ret_val);
	} else
		return 0;
}


int find_matching_str( char *given_str, char *string_set[], char **matching_str,
		int max_str, int opt)
{
	int i;
	*matching_str = 0;
	for(i=max_str-1; i>=0; i--){
		if((*matching_str = look_for_str(given_str, string_set[i])) != 0)
			return(i);
	}
	return(-1);
}
