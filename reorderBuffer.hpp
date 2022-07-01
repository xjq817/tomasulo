#ifndef REORDERBUFFER_HPP
#define REORDERBUFFER_HPP

struct RobNode{
	bool ready,isJump;
	unsigned dest;
	unsigned value;
	unsigned order;
	unsigned pc;
	
	RobNode():ready{0} {}
};

class ReorderBuffer{
private:
	RobNode a[32];
	int head,tail,clock;
	
public:
	ReorderBuffer():head{0},tail{0}{}
	
	bool empty() const {return head==tail;}
	
	bool full() const {return ((tail+1)&31)==head;}
	
	RobNode front() const {return a[head];}
	
	void pop(){head=((head+1)&31);}
	
	void push(RobNode x){
		a[tail]=x;
		tail=((tail+1)&31);
	}
	
	int getTail() const {return tail;}
	
	int getHead() const {return head;}
	
	void put(int x,RobNode u){a[x]=u;}
	
	RobNode get(int x) const {return a[x];}
	
	bool isReady(int x) const {return a[x].ready;}
	
	unsigned getVal(int x) const {return a[x].value;}
	
	void clear(){head=tail=clock=0;}
	
	int getClock() const {return clock;}
	
	void subClock(){clock--;}
	
	void setClock(){clock=3;}
	
}reorderBuffer;

#endif
