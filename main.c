#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LINE_SIZE (500 * 1024)
#define MAX_SEGMENTS	(50 * 1024)

#define MIN_SEG_LEN		10

int seglen_cmp(const void *p, const void *q)
{
	return (strlen(*(const char **)q) - strlen(*(const char **)p));
}

char tbuf[INPUT_LINE_SIZE];
int tlen;
char segbuf[2 * INPUT_LINE_SIZE];	// セグメントの実体をここに詰め込む。
char *segList[MAX_SEGMENTS];		// ここはsegbuf中へのポインタしかもたない。
int segLenList[MAX_SEGMENTS];
int segCount = 0;

int check_match(int ofs, int segID)
{
	int i, lslen, seglen, same = 0;
	const char *longstr = &tbuf[ofs];
	lslen = tlen - ofs;
	seglen = segLenList[segID];
	if(seglen > lslen) return 0;
	for(i = 0; i < seglen; i++){
		if(longstr[i] == 'x' || longstr[i] == segList[segID][i]) same++;
	}
	return same;
}

int find_seg_ofs(int segID)
{
	int maxSameCount = 0, maxSameOfs = -1, sc, i;
	fprintf(stderr, "S[%d] = %s\n", segLenList[segID], segList[segID]);
	for(i = 0; i < tlen; i++){
		sc = check_match(i, segID);
		if(sc > maxSameCount){
			maxSameCount = sc;
			maxSameOfs = i;
			//fprintf(stderr, "Update: ofs = %d (cnt = %d / %lu)\n", maxSameOfs, maxSameCount, strlen(seg));
		}
	}
	return maxSameOfs;
}

void fillRestX()
{
	int i;
	for(i = 0; i < tlen; i++){
		if(tbuf[i] == 'x') tbuf[i] = 'c';
	}
}

void readSegList()
{
	int i, len;
	char *p = segbuf;
	for(i = 0; i < MAX_SEGMENTS; i++){
		segList[i] = p;
		if(!fgets(segList[i], INPUT_LINE_SIZE, stdin)) break;
		len = strlen(segList[i]) - 1;
		segList[i][len] = 0;
		//
		p += len + 1;
		segCount++;
	}

	qsort(segList, segCount, sizeof(char *), seglen_cmp);

	for(i = 0; i < segCount; i++){
		segLenList[i] = strlen(segList[i]);
		if(segLenList[i] < MIN_SEG_LEN) break;
	}
	segCount = i;	// 10文字以下は検査しても精度があがらないのでさようなら
}

int main_prg(int argc, char** argv)
{
	int i;
	fgets(tbuf, INPUT_LINE_SIZE, stdin);
	tlen = strlen(tbuf);
	tlen--;
	tbuf[tlen] = 0;
	//fprintf(stderr, "T'[%d] = %s\n", tlen, tbuf);
	
	readSegList();

	int ofs;
	for(i = 0; i < segCount; i++){
		//fprintf(stderr, "S%d[%lu] = %s\n", i, strlen(segs[i]), segs[i]);
		ofs = find_seg_ofs(i);
		strncpy(&tbuf[ofs], segList[i], segLenList[i]);
	}

	fillRestX();

	printf("%s\n", tbuf);

	return 0;
}

