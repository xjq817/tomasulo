#ifndef LOADSTOREBUFFER_HPP
#define LOADSTOREBUFFER_HPP

struct LSNode{
	int busy;
	bool offset;
	unsigned vj,vk;
	int qj,qk;
	unsigned order;
	unsigned imm,pc,value;
	int dest;
	
	LSNode(){busy=offset=0,qj=qk=-1;}
};

class LoadStoreBuffer{
private:
	LSNode a[32];
	int clock,opPos;
	
public:
	LoadStoreBuffer(){}
	
	int getPos() const {
		for (int i=0;i<32;i++)
			if (!a[i].busy) return i;
		return -1;
	}
	
	void put(const int& x,const LSNode& u){a[x]=u;}
	
	int getClock() const {return clock;}
	
	int getOpPos() const {return opPos;}
	
	void subClock() {clock--;}
	
	void setClock(const int& pos){
		clock=3,opPos=pos;
	}
	
	void clear(){
		for (int i=0;i<32;i++) a[i].busy=0;clock=0;
	}
	
	LSNode get(const int& x) const {return a[x];}
	
}loadStoreBuffer;

#endif
