#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LINE_SIZE (500 * 1024)
#define MAX_SEGMENTS	(50 * 1024)

#define MIN_SEG_LEN	1

int seglen_cmp(const void *p, const void *q)
{
    return (strlen(*(const char **)q) - strlen(*(const char **)p));
}

char tbuf[INPUT_LINE_SIZE];
char tbufinput[INPUT_LINE_SIZE];
int tlen;
char segbuf[2 * INPUT_LINE_SIZE];	// セグメントの実体をここに詰め込む。
char *segList[MAX_SEGMENTS];		// ここはsegbuf中へのポインタしかもたない。
int segLenList[MAX_SEGMENTS];
int segFitList[MAX_SEGMENTS];
int segCount = 0;
int changed=1;
void find_seg_ofs(int segID)
{
    int check, i,lslen, seglen,j;
    fprintf(stderr, "S[%d] = %s\n", segLenList[segID], segList[segID]);
    for(i = 0; i < tlen; i++)
    {
    	i+=segFitList[segID];
        check=1;
        const char *longstr = &tbufinput[i];
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
            strncpy(&tbuf[i], segList[segID], segLenList[segID]);//overwrite on returning answer
            if(seglen>40)
            {
                fprintf(stderr, "%s\n","0ver 40");
                return;
            }

        }
    }
}
void fillTrueAns(int segID)
{
    int check, i,lslen, seglen,j,count=0,position;
    fprintf(stderr, "S[%d] = %s\n", segLenList[segID], segList[segID]);
    for(i = 0; i < tlen; i++)
    {
    	i+=segFitList[segID];
        check=1;
        const char *longstr = &tbufinput[i];
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
        	count++;
        	if(count>1)break;
        	position=i;
         
        }

    }
    if(count==1)
    {
    	strncpy(&tbufinput[position], segList[segID], segLenList[segID]);//overwrite on input
    	fprintf(stderr, "%s\n","FOUND!" );
    	changed=1;
    	segFitList[segID]=segLenList[segID];
    }
}

void fillRestOfX()
{
    int i;
    for(i = 0; i < tlen; i++) {
        if(tbuf[i] == 'x') tbuf[i] = 'c';
    }
}

void readSegList()
{
    int i, len;
    char *p = segbuf;
    for(i = 0; i < MAX_SEGMENTS; i++)
    {
        segList[i] = p;
        if(!fgets(segList[i], INPUT_LINE_SIZE, stdin)) break;
        len = strlen(segList[i]) - 1;
        segList[i][len] = 0;
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


    readSegList();


    strcpy(tbufinput,tbuf);
    while(changed)//  quit filling if there were no changes in the loop
    {
    	changed=0;

    	for(i = segCount-1; i > 0; i--)
    	{
        if(0==segFitList[i])fillTrueAns(i);//if segFit[segID] is 1, you do not need to care this segment any more
   		}
   	}
   	i=0;
   	while(segLenList[i]>4){//begin from 5
   		i++;
   	}
    for(; i > 0; i--)
    {
        if(0==segFitList[i])find_seg_ofs(i);
    }


    fillRestOfX();

    printf("%s\n", tbuf);

    return 0;
}