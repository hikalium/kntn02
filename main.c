#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define INPUT_LINE_SIZE 		(500 * 1024)
#define MAX_SEGMENTS			(32 * 1024)
#define MAX_CANDIDATE_OFFSETS	(256 * 1024 * 1024)

typedef struct SEGMENT Segment;
struct SEGMENT {
	char *str;			// segbuf中へのポインタ
	int len;
	char isFixed;
	int duplicateCount;
	int candidates;
	int *baseCandidateList;	// -1: 終端, -2: すでに埋められた
	Segment *prefixSeg;
};

typedef struct {
	// T'
	int tLen;
	char tStr[INPUT_LINE_SIZE];
	// S
	int segCount;
	Segment *segList[MAX_SEGMENTS];			// セグメントの長さの降順にソート。重複は除去されている。
	Segment segListBase[MAX_SEGMENTS];
	char segBuf[2 * INPUT_LINE_SIZE]; 		// セグメントの実体をここに詰め込む。
} InputData;

InputData givenData;

int candidateOfsBuf[MAX_CANDIDATE_OFFSETS];
int candidateOfsBufCount = 0;
int *candidateOfsList1[3];	// a(0), b(1), c(2)それぞれからはじまるindexを保持

Segment *segListSortedByCC[MAX_SEGMENTS];	// CandidateCountの昇順にソート。
Segment *segListSortedByAlphabetical[MAX_SEGMENTS];	// 辞書順にソート。
int fixedIndexInSegListCC;

char fixedStr[INPUT_LINE_SIZE];		// 修正後の文字列が入る。初めは0初期化されている。
void putSegAtOfs(Segment *s, int ofs)
{
	strncpy(&fixedStr[ofs], s->str, s->len);
	//fprintf(stderr, "S%04d[%02d] -> Fixed %d\n", segID, segLenList[segID], ofs);
}

//
//	データ読み込み
//
int seglen_cmp(const void *p, const void *q)
{
	return (strlen((*(const Segment **)q)->str) - strlen((*(const Segment **)p)->str));
}


#define PPM_XSIZE	1000
void printAsImg(const char *str, const char *filename)
{
	FILE *fp = fopen(filename, "wb");
	if(!fp) return;
	int i;
	uint32_t col;
	fprintf(fp, "P3 %d %d 255 ", PPM_XSIZE, (givenData.tLen + PPM_XSIZE - 1) / PPM_XSIZE);
	for(i = 0; i < givenData.tLen; i++){
		if(str[i] < 'a' || 'c' < str[i]){
			if(givenData.tStr[i]) col = 0xff << (givenData.tStr[i] - 'a') * 8;
			else col = 0;
		} else{
			col = 0xff << (str[i] - 'a') * 8;
		}
		//col = givenData.tStr[i] == 'x' ? 0xffffff : 0x000000;
		fprintf(fp, "%d %d %d ", (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
	}
	fclose(fp);
}

void readT()
{
	// 読み込み
	fgets(givenData.tStr, INPUT_LINE_SIZE, stdin);
	givenData.tLen = strlen(givenData.tStr);
	givenData.tLen--;
	givenData.tStr[givenData.tLen] = 0;	// 末尾の改行文字を除去

	printAsImg(givenData.tStr, "T.ppm");

	// 以下はデバッグ用
	//fprintf(stderr, "T'[%d]=%s\n", givenData.tLen, givenData.tStr);
}

int seg_cmp_alpha(const void *p, const void *q)
{
	const Segment *sp = *(const Segment **)p, *sq = *(const Segment **)q;
	return strcmp(sp->str, sq->str);
}

void readSegList()
{
	int i, k;
	Segment *s;
	char *p = givenData.segBuf;
	for(i = 0; i < MAX_SEGMENTS; i++){
		s = &givenData.segListBase[i];
		if(!fgets(p, INPUT_LINE_SIZE, stdin)) break;
		s->len = strlen(p) - 1;
		p[s->len] = 0;
		//
		for(k = 0; k < i; k++){
			if(s->len != givenData.segListBase[k].len) continue;	// この行は高速化に寄与している
			if(strcmp(p, givenData.segListBase[k].str) == 0) break;
		}
		if(k < i){
			// 重複を発見。
			givenData.segListBase[k].duplicateCount++;
			i--;
			continue;
		}
		// 新規タグなので追加
		s->str = p;
		s->isFixed = 0;
		//
		givenData.segList[i] = s;
		segListSortedByCC[i] = s;
		segListSortedByAlphabetical[i] = s;
		//
		s->duplicateCount = 1;
		p += s->len + 1;
		givenData.segCount++;
	}

	qsort(givenData.segList, givenData.segCount, sizeof(Segment *), seglen_cmp);
	qsort(segListSortedByAlphabetical, givenData.segCount, sizeof(Segment *), seg_cmp_alpha);

	fprintf(stderr, "Building Prefix Tree...\n");
	int L;
	for(i = givenData.segCount - 1; i >= 0; i--){
		// セグメントiのプレフィックスとなっているセグメントkを探す
		s = segListSortedByAlphabetical[i];
		L = s->len - 1;
		for(k = i - 1; k >= 0; k--){
			if(segListSortedByAlphabetical[k]->len > L) continue;
			if(strcmp(s->str, segListSortedByAlphabetical[k]->str) >= 'a') break;
			L = segListSortedByAlphabetical[k]->len;
		}
		if(k >= 0){
			// kがプレフィックスだ！
			s->prefixSeg = segListSortedByAlphabetical[k];
		} else{
			// みつからなかった…
			s->prefixSeg = NULL;	// 不要（NULL初期化されているはずだから）だけど一応書いておく
		}
	}
	fprintf(stderr, "Building Prefix Tree Done.\n");
}

int seg_cmp_cc(const void *p, const void *q)
{
	const Segment *sp = *(const Segment **)p, *sq = *(const Segment **)q;
	return sp->candidates - sq->candidates;
}


int is_matched(int ofs, Segment *s)
{
	// segIDがofsに配置できるなら1を返す。tStrのみを参照し、現在の配置状況には依存しない。
	int i;
	const char *longstr = &givenData.tStr[ofs];
	for(i = 0; i < s->len; i++){
		if(longstr[i] != s->str[i] && longstr[i] != 'x') return 0;
	}
	return 1;
}

int is_empty(int ofs, Segment *s)
{
	// ofsにsを配置するだけの空きがあるか調べる。
	int i;
	for(i = 0; i < s->len; i++){
		if(fixedStr[ofs + i]) return 0;
	}
	return 1;
}

void fillGivenAnswer(char *fixedStr)
{
	int i;
	// まず、そもそも与えられている正解部分を再度上書きする（正解をわざわざ間違える必要なんてない）
	for(i = 0; i < givenData.tLen; i++){
		if(givenData.tStr[i] != 'x') fixedStr[i] = givenData.tStr[i];
	}
}

void fillMidX(char *fixedStr)
{
	int i;
	int c;
	for(i = 1; i < givenData.tLen - 1; i++){
		if(!fixedStr[i]/* && fixedStr[i - 1] && fixedStr[i + 1]*/){
			// まんなかだけ空いている！
			c = fixedStr[i - 1] + fixedStr[i + 1] - 'a' * 2;
			fixedStr[i] = ((3 - c) % 3) + 'a';
			// aとbに挟まれていたらc
			// bとcに挟まれていたらa
			// cとaに挟まれていたらb
		}
	}
}

void fillRestX(char *fixedStr)
{
	int i;

	for(i = 0; i < givenData.tLen; i++){
		if(!fixedStr[i]) fixedStr[i] = 'c';
	}
}

void updateDecisionList()
{
	int i, k, count, *clist;
	Segment *s;
	fprintf(stderr, "Updating ...\n");
	for(i = fixedIndexInSegListCC; i < givenData.segCount; i++){
		// 未配置のセグメントについて、配置可能な位置の個数を更新する
		s = segListSortedByCC[i];
		if(s->candidates == -1){
			fixedIndexInSegListCC++;
			continue;	// すでにこのsegmentは配置されているのでチェックしない
		}
		clist = s->baseCandidateList;
		count = 0;
		for(k = 0; clist[k] != -1; k++){
			if(clist[k] == -2) continue;	// 配置不能とすでにわかっている位置は飛ばす
			if(is_empty(clist[k], s)){	// すでにT'パターンにあうことはわかっているので、重ならないかだけチェックすればよい（高速化）
				count++;
			} else{
				clist[k] = -2;	// 配置不能とマークする
			}
		}
		s->candidates = count;
	}
	// 更新した候補数にしたがってソートする
	fprintf(stderr, "Sorting...\n");
	qsort(segListSortedByCC, givenData.segCount - fixedIndexInSegListCC, sizeof(Segment *), seg_cmp_cc);
}

void initCandidateList()
{
	int i, ofs, ci, k;
	const int *indexPage;
	Segment *s;
	fprintf(stderr, "Generating candidateList...\n");
	for(i = 0; i < 3; i++){
		// candidateOfsList1を生成（最初の1文字の挿入可能性）
		ci = 0;
		candidateOfsList1[i] = &candidateOfsBuf[candidateOfsBufCount];
		for(ofs = 0; ofs < givenData.tLen; ofs++){
			if(givenData.tStr[ofs] == 'x' || givenData.tStr[ofs] == 'a' + i){
				candidateOfsList1[i][ci++] = ofs;
			}
		}
		candidateOfsList1[i][ci++] = -1;
		candidateOfsBufCount += ci;
	}
	// 各セグメントの挿入可能性
	for(i = givenData.segCount - 1; i >= 0; i--){
		// 初期時点における配置可能ofsのリストを作成
		s = givenData.segList[i];
		s->baseCandidateList = &candidateOfsBuf[candidateOfsBufCount];
		ci = 0;
		if(s->prefixSeg){
			// プレフィックスが存在するなら、そのcandidateの部分集合になるはず
			indexPage = s->prefixSeg->baseCandidateList;
		} else{
			// 最初の1文字が存在する位置を対象に検索させる
			indexPage = candidateOfsList1[s->str[0] - 'a'];
		}
		for(k = 0; ~indexPage[k]; k++){
			ofs = indexPage[k];
			if(is_matched(ofs, s)){
				s->baseCandidateList[ci++] = ofs;
			}
		}
		s->baseCandidateList[ci++] = -1;
		candidateOfsBufCount += ci;
	}
	updateDecisionList();
}


int putDecidedSeg()
{
	// 配置しうるofsが一つしかないsegを配置してしまう。
	// もし矛盾するものがあった場合は-1を返す
	int i, fixCount = 0, ofs, k;
	Segment *s;
	for(i = 0; i < givenData.segCount; i++){
		if(segListSortedByCC[i]->candidates != -1) break;	// すでに配置済みのsegを飛ばす
	}
	for(; i < givenData.segCount; i++){
		//if(segListSortedByCC[i]->candidates != 1) break;	// 候補数順にソートされているので、1でなくなったら終了
		if(segListSortedByCC[i]->candidates != segListSortedByCC[i]->duplicateCount) continue;
		s = segListSortedByCC[i];
		s->candidates = -1;	// 配置済みなら-1
		fprintf(stderr, "S[%d](%d) = %s\n", s->len, s->duplicateCount, s->str);
		for(k = 0; s->baseCandidateList[k] != -1; k++){
			ofs = s->baseCandidateList[k];
			if(ofs == -2 || !is_empty(ofs, s)) continue;
			// おける場所はここみたいだ。ここに配置しよう。
			putSegAtOfs(s, ofs);
			fixCount++;
			break;
		}
		if(s->baseCandidateList[k] == -1) return -1;	// 次の候補が見つからなかった。矛盾だ。
	}
	fprintf(stderr, "%d segs fixed.\n", fixCount);
	return fixCount;
}

void printSegListSortedByCC()
{
	int i;
	Segment *s;
	for(i = 0; i < givenData.segCount; i++){
		s = segListSortedByCC[i];
		fprintf(stderr, "S%04d[%2d]x%3d : %3d = %s\n", i, s->len, s->duplicateCount, s->candidates, s->str);
	}
}

void putAllDecidedSeg()
{
	int fixCount;
	for(;;){
		//printSegListSortedByCC();
		fixCount = putDecidedSeg();
		if(fixCount == -1){
			fprintf(stderr, "Conflict detected\n");
			exit(EXIT_FAILURE);
		}
		updateDecisionList();
		if(fixCount == 0) break;
	}
}

void fillFuzzy()
{
	int i, k, ofs;
	Segment *s;
	fprintf(stderr, "Filling fuzzy...\n");
	for(i = givenData.segCount - 1; i >= 0; i--){
		s = givenData.segList[i];
		//s = segListSortedByCC[i];
		if(s->candidates == -1) continue;	// すでに配置されているセグメントに関しては検討しない
		// if(s->len < 2) continue;	// 短すぎるものに関しては埋めない．
		//fprintf(stderr, "FUZZY: S%04d[%2d]x%3d : %3d = %s\n", i, s->len, s->duplicateCount, s->candidates, s->str);
		for(k = 0; s->baseCandidateList[k] != -1; k++){
			ofs = s->baseCandidateList[k];
			if(ofs == -2) continue;	// おけないとすでに判明している（確実に配置するフェーズの段階で）
			// とにかくおける場所を埋めてゆく
			//fprintf(stderr, "%d\n", ofs);
			putSegAtOfs(s, ofs);
		}
	}
}
void printSegList()
{
	int i;
	Segment *s;
	for(i = 0; i < givenData.segCount; i++){
		s = givenData.segList[i];
		fprintf(stderr, "S%04d[%2d]x%3d : %3d = %s\n", i, s->len, s->duplicateCount, s->candidates, s->str);
	}
}

int main_prg(int argc, char** argv)
{
	// T'読み込み
	readT();
	// S読み込み
	readSegList();
	//
	initCandidateList();
	//
	//printSegListSortedByCC();
	putAllDecidedSeg();
	printAsImg(fixedStr, "Tdecided.ppm");
	//
	fillFuzzy();
	fillRestX(fixedStr);	//埋められなかった部分をなんとかする
	printAsImg(fixedStr, "Tfixed.ppm");
	// 結果出力
	printf("%s\n", fixedStr);
	return 0;
}

