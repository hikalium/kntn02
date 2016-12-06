import random
def splitter():
	return 1+int(random.expovariate(1/10.0))
def main():
	xrate=0.44
	x=input("Enter # of characters :")
	test1=""
	for i in range(x):
		test1=test1+chr(97+random.randint(0,2))
	f = open(str(x) + '_ref', 'w') 
	f.write(test1+"\n") 
	f.close()
	test1x=[]
	for i,v in enumerate(test1):
		if(random.random()<xrate):
			test1x.append("x")
		else:
			test1x.append(v)
	f = open(str(x) + '_in', 'w') 
	f.write("".join(test1x)+"\n") 
	start=0
	place=splitter()
	words=[]
	while(place<x):
		words.append(test1[start:place])
		start=place
		place=min(x,start+splitter())
	words.append(test1[start:place])
	wordlist=list(words)
	random.shuffle(wordlist)
	for word in wordlist:
		f.write(word+"\n") 
	f.close()

if __name__ == '__main__':
	main()

