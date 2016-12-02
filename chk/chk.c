#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	if(argc < 3){
		return 1;
	}
	FILE *fp1 = fopen(argv[1], "rb");
	FILE *fp2 = fopen(argv[2], "rb");
	int c1, c2;
	int diff = 0;
	int len = 0;
	for(;;){
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if((c1 == '\n' || c1 == EOF) && (c2 == '\n' || c2 == EOF)) break;
		if(c1 != c2){
			diff++;
		}
		len++;
	}
	printf("%d / %d differs (error rate: %.6f)\n", diff, len, (double)diff / len);

	return 0;
}
