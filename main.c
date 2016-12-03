#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

char fixedStr[INPUT_LINE_SIZE];		// 修正後の文字列が入る。初めは0初期化されている。

uint16_t segHeaderList[MAX_SEGMENTS];		// すべてのビットがabcのいずれかに応じてセットされている
uint16_t tHeaderList[INPUT_LINE_SIZE];		// xのところは0になっているが、それ以外はabcのいずれかに応じてセットされている
uint16_t tHeaderMaskList[INPUT_LINE_SIZE];	// xのところのみ0で、それ以外は1になっている

int segFixedOfs[MAX_SEGMENTS];		// 該当するセグメントが配置されたオフセットを保存。-1に初期化される。

int check_match(int ofs, int segID)
{
	int i, lslen, seglen, same = 0;
	const char *longstr = &tbuf[ofs];
	lslen = tlen - ofs;
	seglen = segLenList[segID];
	if(seglen > lslen) return 0;
	for(i = 0; i < seglen; i++){
		if(longstr[i] == 'x' || longstr[i] == segList[segID][i]) same++;
		else break;
	}
	return same;
}

int find_seg_ofs(int segID)
{
	int maxSameCount = 0, maxSameOfs = -1, sc, i;
	int filledCountAtOfs = 0;
	const int seglen = segLenList[segID];
	//
	fprintf(stderr, "S[%d] (%04X) = %s\n", segLenList[segID], segHeaderList[segID], segList[segID]);
	//
	for(i = 0; i < seglen; i++){
		if(fixedStr[i]) filledCountAtOfs++;
	}
	for(i = 0; i < tlen; i++){
		// すでに埋まっているところはスキップしよう!
		if(filledCountAtOfs) continue;
		//
		sc = check_match(i, segID);
		if(sc > maxSameCount){
			fprintf(stderr, "Update: %d / %d\n", sc, segLenList[segID]);
			maxSameCount = sc;
			maxSameOfs = i;
			//if(maxSameCount == segLenList[segID]) break;	// 完全一致しそうだしこれでしょ！
			//fprintf(stderr, "Update: ofs = %d (cnt = %d / %lu)\n", maxSameOfs, maxSameCount, strlen(seg));
		}
		//
		if(fixedStr[i]) filledCountAtOfs--;	// 次のセグメント範囲内にある、埋まっている文字は1個減る
		if(fixedStr[i + seglen]) filledCountAtOfs++; // 次のセグメント範囲内にある、埋まっている文字は1個減る
	}
	return maxSameOfs;
}

void fillRestX()
{
	int i;
	// まず、そもそも与えられている正解部分を再度上書きする（正解をわざわざ間違える必要なんてない）
	for(i = 0; i < tlen; i++){
		if(tbuf[i] != 'x') fixedStr[i] = tbuf[i];
	}

	for(i = 0; i < tlen; i++){
		if(!fixedStr[i]) fixedStr[i] = 'c';
	}
}

void readT()
{
	int i;
	uint16_t head, mask;
	fgets(tbuf, INPUT_LINE_SIZE, stdin);
	tlen = strlen(tbuf);
	tlen--;
	tbuf[tlen] = 0;
	//
	head = 0;
	mask = 0;
	for(i = 0; i < 8; i++){
		head <<= 2;
		mask <<= 2;
		head |= tbuf[i] - 'a' + 1;	// a: 01, b: 10, c: 11
		mask |= tbuf[i] == 'x' ? 3 : 0;
	}
	for(; i < tlen; i++){
		tHeaderMaskList[i - 8] = ~mask;
		tHeaderList[i - 8] = head & (~mask);
		head <<= 2;
		mask <<= 2;
		head |= tbuf[i] - 'a' + 1;
		mask |= tbuf[i] == 'x' ? 3 : 0;
	}
	for(; i < tlen + 8; i++){
		tHeaderMaskList[i - 8] = ~mask;
		tHeaderList[i - 8] = head & (~mask);
		head <<= 2;
		mask <<= 2;
		head |= 0;
		mask |= tbuf[i] == 3;
	}

	fprintf(stderr, "T'[%d]=%s\n", tlen, tbuf);
	//fprintf(stderr, "%04X\n", tHeaderMaskList[0]);
	//fprintf(stderr, "%04X\n", tHeaderMaskList[1]);
}

void readSegList()
{
	int i, len, k;
	uint16_t head;
	char *p = segbuf;
	for(i = 0; i < MAX_SEGMENTS; i++){
		segList[i] = p;
		segFixedOfs[i] = -1;
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
		// ヘッダを計算
		head = 0;
		for(k = 0; k < 8; k++){
			head <<= 2;
			head |= segList[i][k] - 'a' + 1;	// a: 01, b: 10, c: 11, x: 00
		}
		segHeaderList[i] = head;
	}
	segCount = i;	// 10文字以下は検査しても精度があがらないのでさようなら
}

int main_prg(int argc, char** argv)
{
	int i;
	
	readT();
	readSegList();

	int ofs;
	for(i = 0; i < segCount; i++){
		//fprintf(stderr, "S%d[%lu] = %s\n", i, strlen(segs[i]), segs[i]);
		ofs = find_seg_ofs(i);
		segFixedOfs[i] = ofs;
		strncpy(&fixedStr[ofs], segList[i], segLenList[i]);
	}
	fillRestX();

	printf("%s\n", fixedStr);

	// 以下はデバッグ用。各セグメントがどう配置されたかを表示する。
	int k;
	for(i = 0; i < tlen; i++){
		if(tbuf[i] == 'x') tbuf[i] = ' ';
	}
	fprintf(stderr, "T'      = |%s\n", tbuf);
	fprintf(stderr, "Fixed T = |%s\n", fixedStr);
	for(i = 0; i < segCount; i++){
		fprintf(stderr, "S[%04d] = |", i);
		for(k = 0; k < segFixedOfs[i]; k++){
			fputc(' ', stderr);
		}
		fprintf(stderr, "%s\n", segList[i]);
	}
	
	

	return 0;
}

