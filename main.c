#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INPUT_LINE_SIZE (500 * 1024)
#define MAX_SEGMENTS	(50 * 1024)

#define MIN_SEG_LEN		8	// >= HEADER_CHARS

#define HEADER_CHARS	8
typedef uint16_t Header;

typedef struct {
	int ofs;
	char *str;
} Chunk;

typedef struct {
	int segID;
	int candidates;
	int level;
	int triedOfs;
} SegTag;

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

Header segHeaderList[MAX_SEGMENTS];		// すべてのビットがabcのいずれかに応じてセットされている
Header tHeaderList[INPUT_LINE_SIZE];		// xのところは0になっているが、それ以外はabcのいずれかに応じてセットされている
Header tHeaderMaskList[INPUT_LINE_SIZE];	// xのところのみ0で、それ以外は1になっている

int segFixedOfs[MAX_SEGMENTS];		// 該当するセグメントが配置されたオフセットを保存。-1に初期化される。

SegTag segDecisionList[MAX_SEGMENTS];

int segtag_cmp(const void *p, const void *q)
{
	const SegTag *sp = p, *sq = q;
	int d = sp->candidates - sq->candidates;
	if(d) return d;
	return (segLenList[sq->segID] - segLenList[sp->segID]);
}

int is_matched(int ofs, int segID)
{
	// segIDがofsに配置できるなら1を返す。矛盾する場合は0を返す。
	int i;
	const char *longstr = &tbuf[ofs];
	if((segHeaderList[segID] & tHeaderMaskList[ofs]) != tHeaderList[ofs]) return 0;	// 最初の数文字（ヘッダ）を見て判断
	for(i = HEADER_CHARS; i < segLenList[segID]; i++){	// ヘッダ分はすでに一致するとわかっているのでとばす
		if(longstr[i] != segList[segID][i] && longstr[i] != 'x') return 0;
	}
	return 1;
}

void putSegAtOfs(int segID, int ofs)
{
	segFixedOfs[segID] = ofs;
	strncpy(&fixedStr[ofs], segList[segID], segLenList[segID]);
	fprintf(stderr, "S%04d[%02d] -> Fixed %d\n", segID, segLenList[segID], ofs);
}

void fill04sub(int segID)
{
	int i;
	fprintf(stderr, "S[%d] = %s\n", segLenList[segID], segList[segID]);
	for(i = 0; i < tlen; i++){
		if(is_matched( i, segID)){
			putSegAtOfs(segID, i);
			//i=i+segLenList[segID];
			if(segLenList[segID] > 40) return;
		}
	}
}

void fill04()
{
	// from nkgw, https://kntn.slack.com/files/nkgwer/F3ACSU8BX/main_c.c
	int i;
	for(i = segCount; i > 0; i--)
	{
		fill04sub(i);
	}
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
	Header head, mask;
	// 読み込み
	fgets(tbuf, INPUT_LINE_SIZE, stdin);
	tlen = strlen(tbuf);
	tlen--;
	tbuf[tlen] = 0;
	// ヘッダ・マスク生成
	head = 0;
	mask = 0;
	for(i = 0; i < HEADER_CHARS; i++){
		head <<= 2;
		mask <<= 2;
		if((tbuf[i] != 'x')){
			head |= tbuf[i] - 'a' + 1;
			mask |= 3;
		}
	}
	for(; i < tlen; i++){
		tHeaderMaskList[i - HEADER_CHARS] = mask;
		tHeaderList[i - HEADER_CHARS] = head & mask;
		head <<= 2;
		mask <<= 2;
		if((tbuf[i] != 'x')){
			head |= tbuf[i] - 'a' + 1;
			mask |= 3;
		}
	}
	for(; i < tlen + HEADER_CHARS; i++){
		tHeaderMaskList[i - HEADER_CHARS] = mask;
		tHeaderList[i - HEADER_CHARS] = head & mask;
		head <<= 2;
		mask <<= 2;
		head |= 0;
		mask |= 3;
	}

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
	int i, len, k;
	Header head;
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
		for(k = 0; k < HEADER_CHARS; k++){
			head <<= 2;
			head |= segList[i][k] - 'a' + 1;	// a: 01, b: 10, c: 11, x: 00
		}
		segHeaderList[i] = head;
	}
	fprintf(stderr, "Given segs: %d\n", segCount);
	segCount = i;	// 10文字以下は検査しても精度があがらないのでさようなら
	fprintf(stderr, "Segs longer than %d: %d\n", MIN_SEG_LEN, segCount);
}

int get_num_of_seg_ofs_puttable(int segID)
{
	// すでに配置されているなら-1を返す
	int ofs, count = 0;
	if(segFixedOfs[segID] != -1) return -1;	// すでにこのsegmentは配置されているのでチェックしない
	for(ofs = 0; ofs < tlen; ofs++){
		if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
		if(is_matched(ofs, segID)){
			count++;
		}
	}
	return count;
}
void show_seg_ofs_puttable()
{
	int segID;
	fprintf(stderr, "Puttable ofs:\n");
	for(segID = 0; segID < segCount; segID++){
		if(segFixedOfs[segID] != -1) continue;	// すでにこのsegmentは配置されているのでチェックしない
		fprintf(stderr, "S%04d[%d] (%04X): %d = %s\n", segID, segLenList[segID], segHeaderList[segID], get_num_of_seg_ofs_puttable(segID), segList[segID]);
/*
		int ofs;
		for(ofs = 0; ofs < tlen; ofs++){
			if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
			if(is_matched(ofs, segID)){
				fprintf(stderr, "%d\n", ofs);
			}
		}
*/
	}
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
		fprintf(stderr, "S%04d   = |", i);
		if(tlen <= 150){
			for(k = 0; k < segFixedOfs[i]; k++){
				fputc(' ', stderr);
			}
			fprintf(stderr, "%s\n", segList[i]);
		} else{
			fprintf(stderr, "%d\n", segFixedOfs[i]);
		}
	}
	fprintf(stderr, "fixed segs: %d / %d (%.6lf)\n", decided, segCount, (double)decided / segCount);
}

void updateDecisionList()
{
	int i, p;
	for(i = 0; i < segCount; i++){
		if(segDecisionList[i].level == -1) break;
	}
	p = i;
	for(; i < segCount; i++){
		// 未配置のセグメントについて、配置可能な位置の個数を更新する
		segDecisionList[i].candidates = get_num_of_seg_ofs_puttable(segDecisionList[i].segID);
	}
	// 更新した候補数にしたがってソートする
	qsort(&segDecisionList[p], segCount - p, sizeof(SegTag), segtag_cmp);
}

void initDecisionList()
{
	int i;
	for(i = 0; i < segCount; i++){
		segDecisionList[i].segID = i;
		segDecisionList[i].level = -1;
		segDecisionList[i].triedOfs = 0;
	}
	updateDecisionList();
}

void showDecisionList()
{
	int i;	
	fprintf(stderr, "DecisionList: \n");
	for(i = 0; i < segCount; i++){
		fprintf(stderr, "SDL[%04d]: c:%3d lv:(%3d) %s\n",
			i,
			segDecisionList[i].candidates,
			segDecisionList[i].level,
			segList[segDecisionList[i].segID]
		);
	}
	
}

int putDecidedSeg(int currentLevel)
{
	// 配置しうるofsが一つしかないsegを配置してしまう。
	// もし矛盾するものがあった場合は-1を返す
	int i, segID, fixCount = 0, ofs;
	for(i = 0; i < segCount; i++){
		if(segDecisionList[i].candidates != 0) break;	// すでに配置済みのsegを飛ばす
	}
	for(; i < segCount; i++){
		if(segDecisionList[i].candidates != 1) break;	// 候補数順にソートされているので、1でなくなったら終了
		segDecisionList[i].candidates = 0;	// これから配置するので候補を0にする
		segDecisionList[i].level = currentLevel;
		segID = segDecisionList[i].segID;
		fprintf(stderr, "S%04d[%d] = %s\n", segID, segLenList[segID], segList[segID]);
		for(ofs = 0; ofs < tlen; ofs++){
			if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
			if(is_matched(ofs, segID)){
				if(fixedStr[ofs]) return -1;	// すでに配置されている場所に配置しようとした。矛盾だ。
				// 次における場所はここみたいだ。ここに配置しよう。
				putSegAtOfs(segID, ofs);
				fixCount++;
				break;
			}
		}
		if(ofs >= tlen) return -1;	// 次の候補が見つからなかった。矛盾だ。
	}
	fprintf(stderr, "%d segs fixed.\n", fixCount);
	return fixCount;
}
/*
void fill03All()
{
	int fillCount = 1, count = 0;
	while(fillCount){
		count++;
		fillCount = fill03();
		fprintf(stderr, "Fixed segs(%d): %d\n", count, fillCount);
	}
}
*/
int main_prg(int argc, char** argv)
{
	// 読み込み
	readT();
	readSegList();

	//
	initDecisionList();
	showDecisionList();

	// 処理
	int fixCount, lv = 0;
	for(;;){
		for(;;){
			fixCount = putDecidedSeg(lv);
			if(fixCount == -1){
				fprintf(stderr, "Conflict detected\n");
				break;
			}
			updateDecisionList();
			showDecisionList();
			if(fixCount == 0) break;
		}
		lv++;
		break;
	}


	// 後処理
	fillRestX();	// 埋められなかった部分をなんとかする
	// 結果出力
	printf("%s\n", fixedStr);

	// デバッグ出力
	show_analytics();

	return 0;
}

