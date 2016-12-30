#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define INPUT_LINE_SIZE 		(500 * 1024)
#define MAX_SEGMENTS			(32 * 1024)
#define MAX_CANDIDATE_OFFSETS	(256 * 1024 * 1024)

#define MAX(a, b)	((a) > (b) ? (a) : (b))

typedef struct SEGMENT Segment;
struct SEGMENT {
	char *str;			// segbuf中へのポインタ
	int len;
	char isFixed;
	int duplicateCount;
	int candidates;
	int *baseCandidateList;	// -1: 終端, -2: すでに埋められた
	int *numOfXList;
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

int numOfXBuf[MAX_CANDIDATE_OFFSETS];
int numOfXBufCount = 0;

Segment *segListSortedByCC[MAX_SEGMENTS];	// CandidateCountの昇順にソート。
Segment *segListSortedByAlphabetical[MAX_SEGMENTS];	// 辞書順にソート。

//
// 配置
//

char fixedStr[INPUT_LINE_SIZE];		// 修正後の文字列が入る。初めは0初期化されている。
void putSegAtOfs(Segment *s, int ofs)
{
	strncpy(&fixedStr[ofs], s->str, s->len);
	//fprintf(stderr, "S%04d[%02d] -> Fixed %d\n", segID, segLenList[segID], ofs);
}

//
// デバッグ出力
//

void printSegList(FILE *fp)
{
	int i, k;
	Segment *s;
	for(i = 0; i < givenData.segCount; i++){
		s = givenData.segList[i];
		fprintf(fp, "S%04d[%2d]x%3d : %3d = %s\n", i, s->len, s->duplicateCount, s->candidates, s->str);
		for(k = 0; s->baseCandidateList[k] != -1; k++){
			fprintf(fp, "\t%5d %d\n", s->baseCandidateList[k], s->numOfXList[k]);
			if(k > 5){
				fprintf(fp, "\t...\n");
				break;
			}
		}
	}
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

void printErrorRate(const char *str, const char *ref)
{
	int diff = 0, mismatch = 0, i, notX = 0;
	if(!ref) return;
	for(i = 0; i < givenData.tLen; i++){
		if(str[i] != ref[i]){
			diff++;
			if(str[i] && str[i] != 'x'){
				// xでもNULでもないので，間違った予測をしている．
				mismatch++;
			}
		}
		if(str[i] && str[i] != 'x'){
			notX++;
		}
	}
	fprintf(stderr, "%d / %d differs (error rate: %.6f, mismatch: %d / %d)\n", diff, givenData.tLen, (double)diff / givenData.tLen, mismatch, notX);
}

//
// 画像出力
//

char b[3], a[54] = {
	// BITMAPFILEHEADER: +0
    0x42, 0x4d, 0x36, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
	// BITMAPINFOHEADER: +14
    0x28, 0x00, 0x00, 0x00, 
	0x00, 0x01, 0x00, 0x00, // XSIZE
	0x00, 0x01, 0x00, 0x00, // YSIZE
	0x01, 0x00, 0x18
    // 残りはすべて0
};

#define IMG_XSIZE	1000

void printAsImg(const char *str, const char *filename)
{
	// str中で'\0'または'x'の場所は，．
	FILE *fp = fopen(filename, "wb");
	int i;
	uint32_t col, xsize, ysize, pbytes;
	//
	if(!fp) return;
	xsize = IMG_XSIZE;
	ysize = (givenData.tLen + IMG_XSIZE - 1) / IMG_XSIZE;
	pbytes = 3 * xsize + ysize;
	*((int32_t *)&a[0x12]) = IMG_XSIZE;
	*((int32_t *)&a[0x16]) = -ysize;	// 上から下の順でピクセルを配置するため．
	*((int32_t *)&a[0x02]) = 54 + pbytes;
	fwrite(a, 1, 54, fp);
	//
	for(i = 0; i < givenData.tLen; i++){
		// bgrの順
/*
		if(str[i] < 'a' || 'c' < str[i]){
			// 未決定の部分はgivenData.tStrですでに与えられていればそちらを表示
			col = 0xff << (givenData.tStr[i] - 'a') * 8;
		} else{
*/
			col = 0xff << (str[i] - 'a') * 8;
//		}
		fputc((col >> 16)	& 0xff, fp);
		fputc((col >> 8)	& 0xff, fp);
		fputc(col			& 0xff, fp);
	}
	for(; i < pbytes; i++){
		fputc(0xc6, fp);
		fputc(0xc6, fp);
		fputc(0xc6, fp);
	}
	fclose(fp);
}

//
//	データ読み込み
//
int seglen_cmp(const void *p, const void *q)
{
	return (strlen((*(const Segment **)q)->str) - strlen((*(const Segment **)p)->str));
}

char *readRef(const char *fname)
{
	// 読み込み
	static char refbuf[INPUT_LINE_SIZE];
	FILE *fp = fopen(fname, "rb");
	//
	if(!fp) return NULL;
	fgets(refbuf, INPUT_LINE_SIZE, fp);
	refbuf[strlen(refbuf) - 1] = 0;	// 末尾の改行文字を除去
	//
	printAsImg(refbuf, "Tref.bmp");
	return refbuf;
}

void readT()
{
	// 読み込み
	fgets(givenData.tStr, INPUT_LINE_SIZE, stdin);
	givenData.tLen = strlen(givenData.tStr);
	givenData.tLen--;
	givenData.tStr[givenData.tLen] = 0;	// 末尾の改行文字を除去

	printAsImg(givenData.tStr, "T.bmp");

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
}

int seg_cmp_cc(const void *p, const void *q)
{
	const Segment *sp = *(const Segment **)p, *sq = *(const Segment **)q;
	return sp->candidates - sq->candidates;
}

int is_matched_with_skip(int ofs, Segment *s, int skip)
{
	// segIDがofsに配置できるなら1を返す。tStrのみを参照し、現在の配置状況には依存しない。
	int i;
	const char *longstr = &givenData.tStr[ofs];
	for(i = skip; i < s->len; i++){
		if(longstr[i] != s->str[i] && longstr[i] != 'x') return 0;
	}
	return 1;
}

int get_num_of_x_in_seg(int ofs, Segment *s)
{
	// 位置ofsにsを配置した時，そのセグメントがいくつのxをカバーするかを返す．それ以外は一切チェックしない．
	int i, xc = 0;
	const char *longstr = &givenData.tStr[ofs];
	for(i = 0; i < s->len; i++){
		if(longstr[i] == 'x') xc++;
	}
	return xc;
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

//
// データ生成
//
void updateCandidateList()
{
	static int fixedIndexInSegListCC = 0;
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
	int i, ofs, ci, k, prefixLen;
	const int *indexPage;
	Segment *s;
	fprintf(stderr, "Generating candidateList1...\n");
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
	fprintf(stderr, "Generating candidateList...\n");
	// 各セグメントの挿入可能性
	for(i = givenData.segCount - 1; i >= 0; i--){
		// 初期時点における配置可能ofsのリストを作成
		s = givenData.segList[i];
		s->baseCandidateList = &candidateOfsBuf[candidateOfsBufCount];
		s->numOfXList = &numOfXBuf[candidateOfsBufCount];
		ci = 0;
		if(s->prefixSeg){
			// プレフィックスが存在するなら、そのcandidateの部分集合になるはず
			indexPage = s->prefixSeg->baseCandidateList;
			prefixLen = s->prefixSeg->len;
		} else{
			// 最初の1文字が存在する位置を対象に検索させる
			indexPage = candidateOfsList1[s->str[0] - 'a'];
			prefixLen = 1;
		}
		for(k = 0; ~indexPage[k]; k++){
			ofs = indexPage[k];
			if(is_matched_with_skip(ofs, s, prefixLen)){
				s->baseCandidateList[ci] = ofs;
				s->numOfXList[ci] = get_num_of_x_in_seg(ofs, s);
				ci++;
			}
		}
		//
		s->baseCandidateList[ci] = -1;
		s->numOfXList[ci] = -1;
		ci++;
		//
		candidateOfsBufCount += ci;
	}
	updateCandidateList();
}

//
// 確定的fill
//

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

void putAllDecidedSeg()
{
	int fixCount;
	for(;;){
		//printSegListSortedByCC();
		fixCount = putDecidedSeg();
		if(fixCount == -1){
			fprintf(stderr, "Conflict detected\n");
			exit(EXIT_FAILURE);
			//return;
		}
		updateCandidateList();
		if(fixCount == 0) break;
	}
}

//
// 確率的fill
//

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

int probabilityList[INPUT_LINE_SIZE][3];

void putProbabilityList(Segment *s, int ofs, int p)
{
	int i;
	for(i = 0; i < s->len; i++){
		probabilityList[ofs + i][s->str[i] - 'a'] += p;
	}
}

void genProbabilityList()
{
	int i, k, ofs;
	Segment *s;
	int p;
	fprintf(stderr, "Filling fuzzy...\n");
	for(i = givenData.segCount - 1; i >= 0; i--){
		s = givenData.segList[i];
		//s = segListSortedByCC[i];
		if(s->candidates == -1) continue;	// すでに配置されているセグメントに関しては検討しない
		p = s->duplicateCount * givenData.tLen / s->candidates;
		fprintf(stderr, "P: S%04d[%2d]x%4d : %5d = %5d %s\n", i, s->len, s->duplicateCount, s->candidates, p, s->str);
		for(k = 0; s->baseCandidateList[k] != -1; k++){
			ofs = s->baseCandidateList[k];
			if(ofs == -2) continue;	// おけないとすでに判明している（確実に配置するフェーズの段階で）
			putProbabilityList(s, ofs, p);
		}
	}
}

void fillProbableChar()
{
	int i, max;
	double sum;
	genProbabilityList();
	for(i = 0; i < givenData.segCount; i++){
		sum = probabilityList[i][0] + probabilityList[i][1] + probabilityList[i][2];
		if(sum == 0) continue;
		max = MAX(probabilityList[i][0], MAX(probabilityList[i][1], probabilityList[i][2]));
		if((double)max / sum > 0.9){
			if(probabilityList[i][0] == max) fixedStr[i] = 'a';
			else if(probabilityList[i][1] == max) fixedStr[i] = 'b';
			else fixedStr[i] = 'c';
		}
		//fprintf(stderr, "%5d: %5.2f %5.2f %5.2f\n", i, probabilityList[i][0] / sum, probabilityList[i][1] / sum, probabilityList[i][2] / sum);
	}
}

void fillHikalium()
{
	// s->candidatesが4以下で，s->duplicateCountが1のセグメントを，numOfXが最小の位置に配置する．
	int i, k, minOfs, minNumOfX, minNumOfX2;
	Segment *s;
	for(i = 0; i < givenData.segCount; i++){
		s = givenData.segList[i];
		if(s->candidates == -1 || s->candidates > 6 || s->duplicateCount != 1) continue;
		//fprintf(stderr, "S%04d[%2d]x%3d : %3d = %s\n", i, s->len, s->duplicateCount, s->candidates, s->str);
		minNumOfX = INPUT_LINE_SIZE;	// ありえないほど大きい値
		for(k = 0; s->baseCandidateList[k] != -1; k++){
			if(s->baseCandidateList[k] == -2) continue;
			//fprintf(stderr, "\t%5d %d\n", s->baseCandidateList[k], s->numOfXList[k]);
			if(s->numOfXList[k] <= minNumOfX){
				minNumOfX2 = minNumOfX;
				minNumOfX = s->numOfXList[k];
				minOfs = s->baseCandidateList[k];
			}
		}
		//fprintf(stderr, "-> fix @%d\n", minOfs);
		if(minNumOfX2 - minNumOfX > 5){
			putSegAtOfs(s, minOfs);
			s->candidates = -1;
		}
	}
	updateCandidateList();
}

//
// 残り物fill系
//

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
		if(!fixedStr[i]) fixedStr[i] = 'x';
	}
}

//
// main
//

int main_prg(int argc, char** argv)
{
	const char *refstr = NULL;
	// T'読み込み
	readT();
	// S読み込み
	readSegList();
	// デバッグ用にrefが指定されていれば読み込む
	if(argc == 2){
		// 第1引数をrefファイル名とみなす
		refstr = readRef(argv[1]);
	}
	//
	initCandidateList();
	//
	//printSegListSortedByCC();
	putAllDecidedSeg();
	printAsImg(fixedStr, "Tdecided.bmp");
	printErrorRate(fixedStr, refstr);
	//
	//fillProbableChar();
	fillHikalium();
	printErrorRate(fixedStr, refstr);
	//
	fillFuzzy();
	fillGivenAnswer(fixedStr);
	fillRestX(fixedStr);	//埋められなかった部分をなんとかする
	printAsImg(fixedStr, "Tfixed.bmp");
	// 結果出力
	printf("%s\n", fixedStr);
	printErrorRate(fixedStr, refstr);
	if(refstr){
		FILE *fp = fopen("seglist.txt", "w");
		printSegList(fp);
		fclose(fp);
	}
	return 0;
}

