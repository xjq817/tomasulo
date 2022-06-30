#include <cstdio>
#include <iostream>
#include <cstring>

#include "reservationStation.hpp"
#include "reorderBuffer.hpp"
#include "alu.hpp"
#include "loadStoreBuffer.hpp"

using namespace std;

const int memSize=500000;

unsigned regVal[32],mem[memSize];
int regSta[32],waitJump=-1,predictor[memSize];

int trans(char c){
	if (isdigit(c)) return c^48;
	return c-'A'+10;
}

unsigned Lb(unsigned fr){
	unsigned memVal=fr&255;
	if (memVal&127u) memVal|=(-1u)^(255u);
	return memVal;
}

unsigned Lh(unsigned fr){
	unsigned memVal=25&65535;
	if (memVal&32767u) memVal|=(-1u)^(65535u);
	return memVal;
}

unsigned Lw(unsigned fr){
	return fr;
}

unsigned Lbu(unsigned fr){
	return fr&255;
}

unsigned Lhu(unsigned fr){
	return fr&65535;
}

void store(unsigned order,unsigned dest,unsigned val){
	if (((order>>12)&7)==0b000) mem[dest]=val&255;
	if (((order>>12)&7)==0b001)
		mem[dest]=val&255,mem[dest+1]=(val>>8)&255;
	if (((order>>12)&7)==0b010)
		mem[dest]=val&255,mem[dest+1]=(val>>8)&255,
		mem[dest+2]=(val>>16)&255,mem[dest+3]=val>>24;
}

unsigned load(unsigned order,unsigned fr,bool u){
	if (u) fr=mem[fr]|(mem[fr+1]<<8)
		|(mem[fr+2]<<16)|(mem[fr+3]<<24);
	if (((order>>12)&7)==0b000) return Lb(fr);
	if (((order>>12)&7)==0b001) return Lh(fr);
	if (((order>>12)&7)==0b010) return Lw(fr);
	if (((order>>12)&7)==0b100) return Lbu(fr);
	return Lhu(fr);
}

void broadcast(int dst,unsigned val){
	for (int j=0;j<32;j++){
		RsNode x=reservationStation.get(j);
		if (x.qj==dst) x.vj=val,x.qj=-1;
		if (x.qk==dst) x.vk=val,x.qk=-1;
		reservationStation.put(j,x);
	}
	for (int j=0;j<32;j++){
		LSNode x=loadStoreBuffer.get(j);
		if (x.qj==dst) x.vj=val,x.qj=-1;
		if (x.qk==dst) x.vk=val,x.qk=-1;
		loadStoreBuffer.put(j,x);
	}
}

bool commit(unsigned& pc){
	RobNode u;
	if (reorderBuffer.getClock()){
		reorderBuffer.subClock();
		if (!reorderBuffer.getClock()){
			u=reorderBuffer.front();
			store(u.order,u.dest,u.value);
			reorderBuffer.pop();
		}
		return 0;
	}
	if (reorderBuffer.empty()) return 0;
	u=reorderBuffer.front();
	if (u.order==0x0ff00513) return 1;
	if (!u.ready) return 0;
	unsigned op=u.order&127;
	int b=reorderBuffer.getHead();
	if (op==0b0100011){
		if (!reorderBuffer.getClock())
			reorderBuffer.setClock();
		return 0;
	}
	if (op==0b1100011){
		if (u.value==1){
			if (predictor[u.pc]&2)
				reorderBuffer.pop();
			else{
				reorderBuffer.clear();
				reservationStation.clear();
				loadStoreBuffer.clear();
				waitJump=-1;
				pc=u.dest;
				for (int i=0;i<32;i++) regSta[i]=-1;
			}
			if (predictor[u.pc]<3) predictor[u.pc]++; 
		}
		else{
			if (predictor[u.pc]&2){
				reorderBuffer.clear();
				reservationStation.clear();
				loadStoreBuffer.clear();
				waitJump=-1;
				pc=u.pc+4;
				for (int i=0;i<32;i++) regSta[i]=-1;
			}
			else reorderBuffer.pop();
			if (predictor[u.pc]) predictor[u.pc]--;
		}
	}
	else if (op==0b1101111 || op==0b1100111){
		reorderBuffer.pop();
		if (u.dest) regVal[u.dest]=u.value;
		if (regSta[u.dest]==b) regSta[u.dest]=-1;
	}
	else{
		reorderBuffer.pop();
		if (u.dest) regVal[u.dest]=u.value;
		if (regSta[u.dest]==b) regSta[u.dest]=-1;
	}
	return 0;
}

void writeResult(unsigned& pc){
	for (int i=0;i<32;i++){
		RsNode u=reservationStation.get(i);
		if (u.busy!=2) continue;
		RobNode v=reorderBuffer.get(u.dest);
		v.value=u.value,v.ready=1,u.busy=0;
		broadcast(u.dest,u.value);
		if ((u.order&127)==0b1101111 || 
			(u.order&127)==0b1100111)
			pc=u.value,waitJump=-1,v.value=u.pc+4;
		if ((u.order&127)==0b1100011){
			v.dest=u.pc+u.imm;
			if (waitJump==i) pc=v.dest,waitJump=-1;
		}
		reorderBuffer.put(u.dest,v);
		reservationStation.put(i,u);
		return;
	}
	
	for (int i=0;i<32;i++){
		LSNode u=loadStoreBuffer.get(i);
		if (u.busy!=2) continue;
		if ((u.order&127)==0b0000011){
			RobNode v=reorderBuffer.get(u.dest);
			v.value=u.value,v.ready=1,u.busy=0;
			broadcast(u.dest,u.value);
			reorderBuffer.put(u.dest,v);
			loadStoreBuffer.put(i,u);
			return;
		}
		if (~u.qk) continue;
		RobNode v=reorderBuffer.get(u.dest);
		v.dest=u.value,v.value=u.vk,v.ready=1,u.busy=0;
		reorderBuffer.put(u.dest,v);
		loadStoreBuffer.put(i,u);
		return;
	}
}

void execution(){
	for (int i=0;i<32;i++){
		RsNode u=reservationStation.get(i);
		if (u.busy!=1) continue;
		if (~u.qj || ~u.qk) continue;
		u.value=simulate(u.order,u.vj,u.vk,u.imm,u.pc);
		u.busy=2,reservationStation.put(i,u);
		break;
	}
	
	if (loadStoreBuffer.getClock()){
		loadStoreBuffer.subClock();
		if (!loadStoreBuffer.getClock()){
			int i=loadStoreBuffer.getOpPos();
			LSNode u=loadStoreBuffer.get(i);
			u.value=load(u.order,u.value,1);
			u.busy=2;
			loadStoreBuffer.put(i,u);
		}
	}
	for (int i=0;i<32;i++){
		LSNode u=loadStoreBuffer.get(i);
		if (u.busy!=1) continue;
		if (u.offset){
			int j=reorderBuffer.getHead(),fl=1,pos=0;
			for (;j!=u.dest;j=(j+1)&31){
				RobNode v=reorderBuffer.get(j);
				if ((v.order&127)!=0b0100011) continue;
				if (!v.ready){fl=0;break;}
				if (v.dest==u.value) fl=2,pos=j;
			}
			if (fl==0) continue;
			else if (fl==2){
				RobNode v=reorderBuffer.get(pos);
				u.value=load(u.order,v.value,0);
				u.busy=2,loadStoreBuffer.put(i,u);
			}
			else if (!loadStoreBuffer.getClock()
				 && u.value<memSize)
				loadStoreBuffer.setClock(i);
		}
		if (~u.qj || u.offset) continue;
		u.value=u.imm+u.vj;
		if ((u.order&127)==0b0000011) u.offset=1;
		else u.busy=2;
		loadStoreBuffer.put(i,u);
		break;
	}
}

void issueLoadStore(unsigned order,unsigned& pc){
	int r=loadStoreBuffer.getPos();
	if (r==-1) return;
	RobNode u;LSNode v;
	int b=reorderBuffer.getTail();
	unsigned rs1=getRs1(order),rs2=getRs2(order),
		op=(order&127),imm=getImm(order),rd=getRd(order);
	v.busy=1,v.dest=b;
	u.order=v.order=order,u.pc=v.pc=pc;
	
	if (~regSta[rs1] && rs1){
		int x=regSta[rs1];
		if (reorderBuffer.isReady(x))
			v.vj=reorderBuffer.getVal(x),v.qj=-1;
		else v.qj=x;
	}
	else v.vj=regVal[rs1],v.qj=-1;
	
	if (op==0b0100011){
		if (~regSta[rs2] && rs2){
			int x=regSta[rs2];
			if (reorderBuffer.isReady(x))
				v.vk=reorderBuffer.getVal(x),v.qk=-1;
			else v.qk=x;
		}
		else v.vk=regVal[rs2],v.qk=-1;
	}
	else regSta[u.dest=rd]=rd?b:-1;
	v.imm=imm;	
	reorderBuffer.push(u);
	loadStoreBuffer.put(r,v);pc+=4;
}

void issueOthers(unsigned order,unsigned& pc){
	int r=reservationStation.getPos();
	if (r==-1) return;
	RobNode u;RsNode v;
	u.order=v.order=order,u.pc=v.pc=pc;
	int b=reorderBuffer.getTail();
	unsigned rs1=getRs1(order),rs2=getRs2(order),
		op=(order&127),imm=getImm(order),rd=getRd(order);
	v.busy=1,v.dest=b,u.dest=rd;
	if (op!=0b0110111 && op!=0b0010111 && op!=0b1101111){
		if (~regSta[rs1] && rs1){
			int x=regSta[rs1];
			if (reorderBuffer.isReady(x))
				v.vj=reorderBuffer.getVal(x),v.qj=-1;
			else v.qj=x;
		}
		else v.vj=regVal[rs1],v.qj=-1;
		
		if (op!=0b0010011 && op!=0b1100111){
			if (~regSta[rs2] && rs2){
				int x=regSta[rs2];
				if (reorderBuffer.isReady(x))
					v.vk=reorderBuffer.getVal(x),v.qk=-1;
				else v.qk=x;
			}
			else v.vk=regVal[rs2],v.qk=-1;
		}
	}
	v.imm=imm;
	
	if (op!=0b1100011) regSta[rd]=(rd?b:-1);
	reorderBuffer.push(u);
	reservationStation.put(r,v);
	if (op==0b1101111 || op==0b1100111)
		waitJump=r;
	else if (op==0b1100011){
		if (predictor[pc]&2) waitJump=r;
		else pc+=4;
	}
	else pc+=4;
}

void issue(unsigned order,unsigned& pc){
	if (reorderBuffer.full()) return;
	unsigned op=(order&127);
	if (op==0b0000011 || op==0b0100011)
		return issueLoadStore(order,pc);
	issueOthers(order,pc);
}

bool solve(unsigned& pc){
	if (commit(pc)) return 1;
	writeResult(pc);
	execution();
	if (!~waitJump)
		issue(mem[pc]|(mem[pc+1]<<8)
			|(mem[pc+2]<<16)|(mem[pc+3]<<24),pc);
	return 0;
}

int main(){
//	freopen("bulgarian.data","r",stdin);
//	freopen("new.txt","w",stdout);
	char c[15];
	unsigned addr=0;
	while(~scanf("%s",c)){
		if (c[0]=='@'){
			for (int i=1;i<=8;i++)
				addr=(addr<<4)|trans(c[i]);
		}
		else mem[addr++]=(trans(c[0])<<4)|trans(c[1]);
	}
	for (int i=0;i<32;i++) regSta[i]=-1;
	unsigned pc=0;
	while(1) if (solve(pc)) break;
	printf("%d\n",regVal[10]&255u);
	return 0;
}
