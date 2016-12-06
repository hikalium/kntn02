#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INPUT_LINE_SIZE 		(500 * 1024)
#define MAX_SEGMENTS			(32 * 1024)
#define MAX_CANDIDATE_OFFSETS	(128 * 1024 * 1024)
#define MIN_SEG_LEN		8

#define INDEX_BUF_SIZE	(20 * INPUT_LINE_SIZE)


typedef struct {
	int segID;
	int candidates;
	int *candidateOfsList;	// -1: 終端
} SegTag;

int seglen_cmp(const void *p, const void *q)
{
	return (strlen(*(const char **)q) - strlen(*(const char **)p));
}

char tbuf[INPUT_LINE_SIZE];
int tlen;
char segbuf[2 * INPUT_LINE_SIZE];	// セグメントの実体をここに詰め込む。
char *segList[MAX_SEGMENTS];		// ここはsegbuf中へのポインタしかもたない。この添え字をsegIDとする。
int segLenList[MAX_SEGMENTS];
int segCount = 0;

char fixedStr[INPUT_LINE_SIZE];		// 修正後の文字列が入る。初めは0初期化されている。

int segFixedOfs[MAX_SEGMENTS];		// 該当するセグメントが配置されたオフセットを保存。-1に初期化される。

SegTag segDecisionListBuf[MAX_SEGMENTS];	// この配列の添え字はsegIDではない！！！
SegTag* segDecisionList[MAX_SEGMENTS];		// この配列の添え字はsegIDではない！！！
SegTag* segTagMap[MAX_SEGMENTS];			// この配列の添え字はsegIDです。

int candidateOfsBuf[MAX_CANDIDATE_OFFSETS];
int candidateOfsBufCount = 0;

int *indexTable[27];
int indexBuf[INDEX_BUF_SIZE];
int indexBufCount = 0;

int segtag_cmp(const void *p, const void *q)
{
	const SegTag *sp = *(const SegTag **)p, *sq = *(const SegTag **)q;
	int d = sp->candidates - sq->candidates;
	if(d) return d;
	return (segLenList[sq->segID] - segLenList[sp->segID]);
}

int is_matched(int ofs, int segID)
{
	// segIDがofsに配置できるなら1を返す。矛盾する場合は0を返す。
	int i;
	const char *longstr = &tbuf[ofs];
	for(i = 0; i < segLenList[segID]; i++){
		if(longstr[i] != segList[segID][i] && longstr[i] != 'x') return 0;
	}
	return 1;
}

void putSegAtOfs(int segID, int ofs)
{
	segFixedOfs[segID] = ofs;
	strncpy(&fixedStr[ofs], segList[segID], segLenList[segID]);
	//fprintf(stderr, "S%04d[%02d] -> Fixed %d\n", segID, segLenList[segID], ofs);
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

#define GET_KEY(s)	((s[0] - 'a') + (s[1] - 'a') * 3 + (s[2] - 'a') * 9)
#define IS_KEY_OFS(s, i, k) ( \
	(s[i] == 'x' || s[i] - 'a' == k % 3) && \
	(s[i + 1] == 'x' || s[i + 1] - 'a' == (k / 3)%3) && \
	(s[i + 2] == 'x' || s[i + 2] - 'a' == k/9) \
)
#define INDEX_CHARS	3
void generateIndex()
{
	int i, k;
	for(k = 0; k < 27; k++){
		indexTable[k] = &indexBuf[indexBufCount];
		for(i = 0; i < tlen - INDEX_CHARS + 1; i++){
			if(IS_KEY_OFS(tbuf, i, k)){
				indexBuf[indexBufCount++] = i;
			}
		}
		indexBuf[indexBufCount++] = -1;
	}
	// 以下はデバッグ用
/*
	for(k = 0; k < 9; k++){
		fprintf(stderr, ">>> %c%c:\n", k%3 + 'a', k/3 + 'a');
		for(i = 0; ~indexTable[k][i]; i++){
			fprintf(stderr, "%s\n", &tbuf[indexTable[k][i]]);
		}
	}
*/
	fprintf(stderr, "IndexBuf used: %d / %d (%.6f)\n", indexBufCount, INDEX_BUF_SIZE, (double)indexBufCount / INDEX_BUF_SIZE);

}

void readT()
{
	// 読み込み
	fgets(tbuf, INPUT_LINE_SIZE, stdin);
	tlen = strlen(tbuf);
	tlen--;
	tbuf[tlen] = 0;

	// 索引を作成
	generateIndex();

	// 以下はデバッグ用
	fprintf(stderr, "T'[%d]=%s\n", tlen, tbuf);
/*
	int k;
	for(i = 0; i < 8; i++){
		for(k = 0; k < i * 2; k++){
			fputc(' ', stderr);
		}
		for(k = 0; k < 8; k++){
			fputc('0' + ((tHeaderList[i] >> (7 - k)) & 1), stderr);
		}
		fputc('\n', stderr);
	}
*/
}

void readSegList()
{
	int i, len;
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
	}
	fprintf(stderr, "Given segs: %d\n", segCount);
	segCount = i;	// MIN_SEG_LEN文字以下は検査しても精度があがらないのでさようなら
	fprintf(stderr, "Segs longer than %d: %d\n", MIN_SEG_LEN, segCount);
}

int get_num_of_seg_ofs_puttable(SegTag *st)
{
	// すでに配置されているなら-1を返す
	int k, count = 0;
	if(segFixedOfs[st->segID] != -1) return -1;	// すでにこのsegmentは配置されているのでチェックしない
	for(k = 0; st->candidateOfsList[k] != -1; k++){
		if(is_matched(st->candidateOfsList[k], st->segID)){
			count++;
		}
	}
	return count;
}

void show_analytics()
{
	int i;
	fprintf(stderr, "T'      = |");
	for(i = 0; i < tlen; i++){
		if(tbuf[i] == 'x') fputc(' ', stderr);
		else fputc(tbuf[i], stderr);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "Fixed T = |%s\n", fixedStr);	

	int k, decided = 0;
	for(i = 0; i < segCount; i++){
		if(segFixedOfs[i] < 0){
			continue;
		}
		decided++;
		if(tlen <= 150){
			fprintf(stderr, "S%04d   = |", i);
			for(k = 0; k < segFixedOfs[i]; k++){
				fputc(' ', stderr);
			}
			fprintf(stderr, "%s\n", segList[i]);
		}
	}
	fprintf(stderr, "fixed segs: %d / %d (%.6lf)\n", decided, segCount, (double)decided / segCount);
	fprintf(stderr, "IndexBuf used: %d / %d (%.6f)\n", indexBufCount, INDEX_BUF_SIZE, (double)indexBufCount / INDEX_BUF_SIZE);
}

void updateDecisionList()
{
	int i, p;
	for(i = 0; i < segCount; i++){	// 配置済みのセグメントはソートしなくていいよね？飛ばすよ。
		if(segDecisionList[i]->candidates != 0) break;
	}
	p = i;
	for(; i < segCount; i++){
		// 未配置のセグメントについて、配置可能な位置の個数を更新する
		segDecisionList[i]->candidates = get_num_of_seg_ofs_puttable(segDecisionList[i]);
	}
	// 更新した候補数にしたがってソートする
	qsort(&segDecisionList[p], segCount - p, sizeof(SegTag *), segtag_cmp);
}

void initDecisionList()
{
	int i, ofs, ci, k;
	const int *indexPage;
	fprintf(stderr, "Generating DecisionList ...\n");
	for(i = 0; i < segCount; i++){
		segDecisionList[i] = &segDecisionListBuf[i];
		segTagMap[i] = segDecisionList[i];
		segDecisionList[i]->segID = i;
		// 初期時点における配置可能ofsのリストを作成
		segDecisionList[i]->candidateOfsList = &candidateOfsBuf[candidateOfsBufCount];
		ci = 0;
		indexPage = indexTable[GET_KEY(segList[i])];
		for(k = 0; ~indexPage[k]; k++){
			ofs = indexPage[k];
			// if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない（というチェックは最初なので不要）
			if(is_matched(ofs, i)){
				segDecisionList[i]->candidateOfsList[ci++] = ofs;
			}
		}
		segDecisionList[i]->candidateOfsList[ci++] = -1;
		candidateOfsBufCount += ci;
	}
	updateDecisionList();
}

void showDecisionList()
{
	int i;	
	fprintf(stderr, "DecisionList: \n");
	for(i = 0; i < segCount; i++){
		fprintf(stderr, "SDL[%04d]: c:%3d %s\n",
			i,
			segDecisionList[i]->candidates,
			segList[segDecisionList[i]->segID]
		);
		/*
		int k;
		for(k = 0; segDecisionList[i]->candidateOfsList[k] != -1; k++){
			fprintf(stderr, "%d ", segDecisionList[i]->candidateOfsList[k]);
		}
		fprintf(stderr, "\n");
		*/
	}
	
}

int putDecidedSeg()
{
	// 配置しうるofsが一つしかないsegを配置してしまう。
	// もし矛盾するものがあった場合は-1を返す
	int i, segID, fixCount = 0, ofs, k;
	SegTag *st;
	for(i = 0; i < segCount; i++){
		if(segDecisionList[i]->candidates != 0) break;	// すでに配置済みのsegを飛ばす
	}
	for(; i < segCount; i++){
		if(segDecisionList[i]->candidates != 1) break;	// 候補数順にソートされているので、1でなくなったら終了
		st = segDecisionList[i];
		st->candidates = 0;	// これから配置するので候補を0にする
		segID = st->segID;
		fprintf(stderr, "S%04d[%d] = %s\n", segID, segLenList[segID], segList[segID]);
		for(k = 0; st->candidateOfsList[k] != -1; k++){
			ofs = st->candidateOfsList[k];
			if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
			if(is_matched(ofs, segID)){
				// 次における場所はここみたいだ。ここに配置しよう。
				putSegAtOfs(segID, ofs);
				fixCount++;
				break;
			}
		}
		if(st->candidateOfsList[k] == -1) return -1;	// 次の候補が見つからなかった。矛盾だ。
	}
	fprintf(stderr, "%d segs fixed.\n", fixCount);
	return fixCount;
}

void putAllDecidedSeg()
{
	int fixCount;
	for(;;){
		fixCount = putDecidedSeg();
		if(fixCount == -1){
			fprintf(stderr, "Conflict detected\n");
			exit(EXIT_FAILURE);
		}
		updateDecisionList();
		showDecisionList();
		if(fixCount == 0) break;
	}
}
/*
void fill_nkgwer_sub(int segID)
{
	// nkgwer's algorithm
	int i, seglen;
	fprintf(stderr, "nkgwer:S[%d] = %s\n", segLenList[segID], segList[segID]);
	for(i = 0; i < tlen; i++){
		seglen = segLenList[segID];
		if(seglen > (tlen - i)) continue;
		if(is_matched(i, segID)){
			putSegAtOfs(segID, i);
			if(seglen > 40){
				fprintf(stderr, "%s\n","0ver 40");
				return;
			}
		}
	}
}
*/
void fill_nkgwer_sub(int segID)
{
	// nkgwer's algorithm
	int k, ofs;
	SegTag *st = segTagMap[segID];
	fprintf(stderr, "nkgwer:S[%d] = %s\n", segLenList[segID], segList[segID]);

	for(k = 0; st->candidateOfsList[k] != -1; k++){
		ofs = st->candidateOfsList[k];
		putSegAtOfs(segID, ofs);
		if(segLenList[segID] > 40){
			fprintf(stderr, "%s\n","0ver 40");
			return;
		}
	}
/*

	for(ofs = 0; ofs < tlen; ofs++){
		if(seglen > (tlen - ofs)) continue;
		if(is_matched(ofs, segID)){
			putSegAtOfs(segID, ofs);
			if(seglen > 40){
				fprintf(stderr, "%s\n","0ver 40");
				return;
			}
		}
	}
*/	
}

void fill_nkgwer()
{
	// nkgwer's algorithm
	int i;
	for(i = segCount - 1; i >=0; i--){
		fill_nkgwer_sub(i);
	}
}

char fixedStrStack[INPUT_LINE_SIZE];
void pushFixedStr()
{
	// 現在のfixedStrを保存しておく
	memcpy(fixedStrStack, fixedStr, tlen);
}

void popFixedStr()
{
	// 以前のfixedStrで現在のfixedStrを上書きする。前回未決定の部分はそのまま残す。
	int i;
	for(i = 0; i < tlen; i++){
		if(fixedStrStack[i]) fixedStr[i] = fixedStrStack[i];
	}
}

int main_prg(int argc, char** argv)
{
	// 読み込み
	readT();
	readSegList();

	//
	initDecisionList();
	showDecisionList();

	// 処理
	putAllDecidedSeg();	// 確定しているセグメントを埋める

	pushFixedStr();	
	fill_nkgwer();
	pushFixedStr();

	// 後処理
	fillRestX();	// 埋められなかった部分をなんとかする
	// 結果出力
	printf("%s\n", fixedStr);

	// デバッグ出力
	show_analytics();

	return 0;
}

