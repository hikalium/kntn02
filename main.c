#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LINE_SIZE (500 * 1024)
#define MAX_SEGMENTS	(50 * 1024)

int seglen_cmp(const void *p, const void *q)
{
	return (strlen(*(const char **)q) - strlen(*(const char **)p));
}

int check_match(const char *longstr, const char *segment)
{
	int i, lslen, seglen, same = 0;
	lslen = strlen(longstr);
	seglen = strlen(segment);
	if(seglen > lslen) return 0;
	for(i = 0; i < seglen; i++){
		if(longstr[i] == 'x' || longstr[i] == segment[i]) same++;
	}
	return same;
}

char tbuf[INPUT_LINE_SIZE];
int tlen;
char *segs[MAX_SEGMENTS];


int find_seg_ofs(const char *seg)
{
	int maxSameCount = 0, maxSameOfs = -1, sc, i;
	fprintf(stderr, "S[%lu] = %s\n", strlen(seg), seg);
	for(i = 0; i < tlen; i++){
		sc = check_match(&tbuf[i], seg);
		if(sc > maxSameCount){
			maxSameCount = sc;
			maxSameOfs = i;
			//fprintf(stderr, "Update: ofs = %d (cnt = %d / %lu)\n", maxSameOfs, maxSameCount, strlen(seg));
		}
	}
	return maxSameOfs;
}

int main_prg(int argc, char** argv)
{
	int i, stlen, nseg = 0;
	for(i = 0; i < MAX_SEGMENTS; i++){
		segs[i] = malloc(INPUT_LINE_SIZE);
		if(!segs[i]) return 1;
	}
	fgets(tbuf, INPUT_LINE_SIZE, stdin);
	tlen = strlen(tbuf);
	tlen--;
	tbuf[tlen] = 0;
	//fprintf(stderr, "T'[%d] = %s\n", tlen, tbuf);
	
	for(i = 0; i < MAX_SEGMENTS; i++){
		if(!fgets(segs[i], INPUT_LINE_SIZE, stdin)){
			break;
		}
		stlen = strlen(segs[i]);
		stlen--;
		segs[i][stlen] = 0;
		nseg++;
	}

	qsort(segs, nseg, sizeof(char *), seglen_cmp);

	int ofs;
	for(i = 0; i < nseg; i++){
		//fprintf(stderr, "S%d[%lu] = %s\n", i, strlen(segs[i]), segs[i]);
		ofs = find_seg_ofs(segs[i]);
		strncpy(&tbuf[ofs], segs[i], strlen(segs[i]));
	}	

	printf("%s\n", tbuf);

	return 0;
}

