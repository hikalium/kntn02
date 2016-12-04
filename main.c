#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INPUT_LINE_SIZE (500 * 1024)
#define MAX_SEGMENTS	(50 * 1024)

#define MIN_SEG_LEN		8

#define HEADER_CHARS	8
typedef uint16_t Header;

typedef struct {
	int ofs;
	char *str;
} Chunk;

int seglen_cmp(const void *p, const void *q)
{
	return (strlen(*(const char **)q) - strlen(*(const char **)p));
}

int chunklen_cmp(const void *p, const void *q)
{
	int plen, qlen;
	plen = strlen(((Chunk *)p)->str);
	qlen = strlen(((Chunk *)q)->str);
	if(plen == qlen) return strcmp(((Chunk *)p)->str, ((Chunk *)q)->str);
	return (strlen(((Chunk *)q)->str) - strlen(((Chunk *)p)->str));
	//return strcmp(((Chunk *)q)->str, ((Chunk *)p)->str);
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

char tChunkBuffer[INPUT_LINE_SIZE];	// segbuf的なもの。xを含まない連続部分を格納する。
Chunk tChunkList[MAX_SEGMENTS];		// ここはtChunkBuffer中へのポインタしかもたない。
int tChunkCount = 0;
/*
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
*/
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
/*
int find_seg_ofs(int segID)
{
	int maxSameCount = 0, maxSameOfs = -1, sc, i;
	int filledCountAtOfs = 0;
	const int seglen = segLenList[segID];
	//
	fprintf(stderr, "S%04d[%d] (%04X) = %s\n", segID, segLenList[segID], segHeaderList[segID], segList[segID]);
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
			//fprintf(stderr, "Update: %d / %d\n", sc, segLenList[segID]);
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
*/
void putSegAtOfs(int segID, int ofs)
{
	segFixedOfs[segID] = ofs;
	strncpy(&fixedStr[ofs], segList[segID], segLenList[segID]);
	fprintf(stderr, "S%04d[%02d] -> Fixed %d\n", segID, segLenList[segID], ofs);
}
/*
void fill00()
{
	int i, ofs;
	for(i = 0; i < segCount; i++){
		ofs = find_seg_ofs(i);
		segFixedOfs[i] = ofs;
		putSegAtOfs(i, ofs);
	}
}

void fill01()
{
	int i, k;
	int cl, ofs;
	char *p;
	for(i = 0; i < tChunkCount; i++){
		cl = strlen(tChunkList[i].str);
		if(cl < 12) break;
		fprintf(stderr, "Chunk%6d @ %6d %s\n", i, tChunkList[i].ofs, tChunkList[i].str);
		for(k = 0; k < segCount; k++){
			if(segLenList[k] < cl) break;
			p = strstr(segList[k], tChunkList[i].str);
			if(!p) continue;
			ofs = p - segList[k];
			ofs = tChunkList[i].ofs - ofs;
			if(!is_matched(ofs, k)) continue;	// 置けないと確実にわかるなら弾く
			fprintf(stderr, "\tS%04dd[%02d]+%02ld %2d = %s\n", i, segLenList[k], p - segList[k], is_matched(ofs, k), segList[k]);
			
			putSegAtOfs(k, ofs);
			break;
		}
	}
}
*/

int fill03()
{
	// 配置しうるofsが一つしかないsegを配置してしまう。
	int segID, ofs, i, fixCount = 0;
	for(segID = 0; segID < segCount; segID++){
		//if(segLenList[segID] < 10) break;
		if(segFixedOfs[segID] != -1) continue;	// すでにこのsegmentは配置されているのでチェックしない
		fprintf(stderr, "S%04d[%d] = %s\n", segID, segLenList[segID], segList[segID]);
		for(ofs = 0; ofs < tlen; ofs++){
			if(fixedStr[ofs]) continue;	// すでに配置されているのでここにはおけない
			if(is_matched(ofs, segID)){
				fprintf(stderr, "f1@%d\n", ofs);
				for(i = ofs + 1; i < tlen; i++){
					if(fixedStr[i]) continue;	// すでに配置されているのでここにはおけない
					if(is_matched(i, segID)){
						fprintf(stderr, "f2@%d\n", i);
						break;
					}
				}
				if(i >= tlen){
					// 1箇所しかない！絶対にここだ！
					putSegAtOfs(segID, ofs);
					fixCount++;
				} else{
					break;
				}
			}
		}
	}
	return fixCount;
}

void fill03All()
{
	int fillCount = 1, count = 0;
	while(fillCount){
		count++;
		fillCount = fill03();
		fprintf(stderr, "Fixed segs(%d): %d\n", count, fillCount);
	}
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
	// チャンク生成
	strncpy(tChunkBuffer, tbuf, tlen);
	int flg = 0;
	for(i = 0; i < tlen; i++){
		if(tChunkBuffer[i] == 'x'){
			tChunkBuffer[i] = 0;
			flg = 0;
		} else{
			if(!flg){
				tChunkList[tChunkCount].ofs = i;
				tChunkList[tChunkCount].str = &tChunkBuffer[i];
				tChunkCount++;
			}
			flg = 1;
		}
	}
	qsort(tChunkList, tChunkCount, sizeof(Chunk), chunklen_cmp);

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
	fprintf(stderr, "Segs longer than 10: %d\n", segCount);
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
	int segID, ofs;
	fprintf(stderr, "Puttable ofs:\n");
	for(segID = 0; segID < segCount; segID++){
		if(segFixedOfs[segID] != -1) continue;	// すでにこのsegmentは配置されているのでチェックしない
		fprintf(stderr, "S%04d[%d] (%04X): %d = %s\n", segID, segLenList[segID], segHeaderList[segID], get_num_of_seg_ofs_puttable(segID), segList[segID]);
/*
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

int main_prg(int argc, char** argv)
{
	// 読み込み
	readT();
	readSegList();

	// 処理
	fill03All();	// 確実に位置がわかるものをすべて配置する
	show_seg_ofs_puttable();

	// 後処理
	fillRestX();	// 埋められなかった部分をなんとかする
	// 結果出力
	printf("%s\n", fixedStr);

	// デバッグ出力
	show_analytics();

	return 0;
}

