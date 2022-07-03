#ifndef RESERVATIONSTATION_HPP
#define RESERVATIONSTATION_HPP

struct RsNode{
	int busy;
	unsigned vj,vk;
	int qj,qk;
	unsigned order;
	unsigned imm,pc,value;
	int dest;
	
	RsNode(){busy=0;qj=qk=-1;}
	
};

class ReservationStation{
private:
	RsNode a[32];
	
public:
	ReservationStation(){}
	
	int getPos(){
		for (int i=0;i<32;i++)
			if (!a[i].busy) return i;
		return -1;
	}
	
	RsNode get(const int& pos) const {return a[pos];}
	
	void put(const int& pos,const RsNode& u){a[pos]=u;}
	
	void clear(){
		for (int i=0;i<32;i++) a[i].busy=0;
	}
	
}reservationStation_now,reservationStation_pre;

#endif
