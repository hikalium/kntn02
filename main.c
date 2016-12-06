#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INPUT_LINE_SIZE 		(500 * 1024)
#define MAX_SEGMENTS			(32 * 1024)
#define MAX_CANDIDATE_OFFSETS	(128 * 1024 * 1024)
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
	int *candidateOfsList;	// -1: 終端
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

SegTag segDecisionListBuf[MAX_SEGMENTS];
SegTag* segDecisionList[MAX_SEGMENTS];

int candidateOfsBuf[MAX_CANDIDATE_OFFSETS];
int candidateOfsBufCount = 0;

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
	for(i = 0; i < segLenList[segID]; i++){	// ヘッダ分はすでに一致するとわかっているのでとばす
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
			head |= segList[i][k] - 'a' + 1;	// a: 01, b: 10, c: 11
		}
		segHeaderList[i] = head;
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
}

void updateDecisionList()
{
	int i, p;
	for(i = 0; i < segCount; i++){
		if(segDecisionList[i]->level == -1) break;
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
	int i, ofs, ci;
	fprintf(stderr, "Generating DecisionList ...\n");
	for(i = 0; i < segCount; i++){
		segDecisionList[i] = &segDecisionListBuf[i];
		segDecisionList[i]->segID = i;
		segDecisionList[i]->level = -1;
		segDecisionList[i]->triedOfs = 0;
		// 初期時点における配置可能ofsのリストを作成
		segDecisionList[i]->candidateOfsList = &candidateOfsBuf[candidateOfsBufCount];
		ci = 0;
		for(ofs = 0; ofs < tlen; ofs++){
			if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
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
		fprintf(stderr, "SDL[%04d]: c:%3d lv:(%3d) %s\n",
			i,
			segDecisionList[i]->candidates,
			segDecisionList[i]->level,
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

int putDecidedSeg(int currentLevel)
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
		st->level = currentLevel;
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

void putAllDecidedSeg(int currentLevel)
{
	int fixCount;
	for(;;){
		fixCount = putDecidedSeg(currentLevel);
		if(fixCount == -1){
			fprintf(stderr, "Conflict detected\n");
			break;
		}
		updateDecisionList();
		showDecisionList();
		if(fixCount == 0) break;
	}
}

int isConflicted()
{
	int i;
	for(i = 0; i < segCount; i++){
		if(segDecisionList[i]->candidates != 0) break;
		if(segDecisionList[i]->level == -1) return 1;	// 未配置なのに候補位置の個数が0になるのはおかしい。これ以前の配置が間違っている。
	}
	return 0;
}

int putNextCandidate(int currentLevel)
{
	// 配置しうるofsの個数が最も少ないsegを試しに配置してみる。
	// もし、もう未定のものがない場合は-1を返す
	int i, segID, ofs;
	for(i = 0; i < segCount; i++){
		if(segDecisionList[i]->candidates != 0) break;	// すでに配置済みのsegを飛ばす
	}
	if(i >= segCount) return -1;	// もうsegないよ！
	// segDecisionList[i]は未配置で、かつ複数の配置先候補があるはず。
	segDecisionList[i]->candidates = 0;	// これから配置するので候補を0にする
	segDecisionList[i]->level = currentLevel;
	segID = segDecisionList[i]->segID;
	fprintf(stderr, "TRY S%04d[%d] = %s\n", segID, segLenList[segID], segList[segID]);
	for(ofs = 0; ofs < tlen; ofs++){
		if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
		if(is_matched(ofs, segID)){
			if(fixedStr[ofs]) continue;	// すでに配置されている場所には置けないね。
			// 次における場所はここみたいだ。ここに配置しよう。
			putSegAtOfs(segID, ofs);
			break;
		}
	}
	fprintf(stderr, "fixed.\n");
	return 0;
}
/*
void fill_nkgwer_sub(int segID)
{
    int check, i,lslen, seglen,j;
    fprintf(stderr, "S[%d] = %s\n", segLenList[segID], segList[segID]);
    for(i = 0; i < tlen; i++)
    {
        check=1;
        const char *longstr = &tbuf[i];
        lslen = tlen - i;
        seglen = segLenList[segID];
        if(seglen > lslen) continue;
        for(j = 0; j < seglen; j++)
        {
            if(!(longstr[j] == 'x' || longstr[j] == segList[segID][j]))
            {
                check=0;
                break;

            }
        }
        if(check)
        {
			putSegAtOfs(segID, i);
            if(seglen>40)
            {
                fprintf(stderr, "%s\n","0ver 40");
                return;
            }

        }
    }
}

void fill_nkgwer()
{
	// nkgwer's algorithm
   	int i = 0;
   	while(segLenList[i]>4){//begin from 5
   		i++;
   	}
    for(; i > 0; i--)
    {
        if(segFixedOfs[i] != -1) fill_nkgwer(i);
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
	int lv = 0;
	putAllDecidedSeg(lv);
	lv++;
/*
	for(;;){
		if(putNextCandidate(lv) == -1){	// 次の候補位置に配置してみる
			// もう候補がないので終了
			break;
		}
		updateDecisionList();
		//showDecisionList();
		if(isConflicted()){		// その位置が矛盾しないかチェック
			fprintf(stderr, "Conflicted.\n");
			break;
		}
		putAllDecidedSeg(lv);
		lv++;
	}
*/
	//fill_nkgwer();
	// 後処理
	fillRestX();	// 埋められなかった部分をなんとかする
	// 結果出力
	printf("%s\n", fixedStr);

	// デバッグ出力
	show_analytics();

	return 0;
}

