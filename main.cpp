#include <cstdio>
#include <iostream>
#include <cstring>

#include "reservationStation.hpp"
#include "reorderBuffer.hpp"
#include "alu.hpp"
#include "loadStoreBuffer.hpp"

using namespace std;

unsigned regVal[32],mem[500000];
int regSta[32],Clock;

void printReg(){
	for (int i=0;i<32;i++) printf("i=%d %08x\n",i,regVal[i]);
}

int trans(char c){
	if (isdigit(c)) return c^48;
	return c-'A'+10;
}

unsigned Lb(unsigned fr){
	unsigned memVal=mem[fr];
	if (memVal&127u) memVal|=(-1u)^(255u);
	return memVal;
}

unsigned Lh(unsigned fr){
	unsigned memVal=mem[fr]|(mem[fr+1]<<8);
	if (memVal&32767u) memVal|=(-1u)^(65535u);
	return memVal;
}

unsigned Lw(unsigned fr){
	return mem[fr]|(mem[fr+1]<<8)
		|(mem[fr+2]<<16)|(mem[fr+3]<<24);
}

unsigned Lbu(unsigned fr){
	return mem[fr];
}

unsigned Lhu(unsigned fr){
	return mem[fr]|(mem[fr+1]<<8);
}

void store(unsigned order,unsigned dest,unsigned val){
	if (((order>>12)&7)==0b000) mem[dest]=val&255;
	if (((order>>12)&7)==0b001)
		mem[dest]=val&255,
		mem[dest+1]=(val>>8)&255;
	if (((order>>12)&7)==0b010)
		mem[dest]=val&255,
		mem[dest+1]=(val>>8)&255,
		mem[dest+2]=(val>>16)&255,
		mem[dest+3]=val>>24;
}

unsigned load(unsigned order,unsigned fr){
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
//		if (x.pc==0x146c) printf("#####%08x %d %08x %d %d %d\n",x.vj,x.qj,x.vk,x.qk,x.busy,x.offset);
		loadStoreBuffer.put(j,x);
	}
}

bool commit(unsigned& pc){
	RobNode u;
	if (reorderBuffer.getClock()){
		if (Clock==reorderBuffer.getClock()+2){
			u=reorderBuffer.front();
//			printf("dest=%08x value=%08x\n",u.dest,u.value);
			store(u.order,u.dest,u.value);
			reorderBuffer.pop();
			reorderBuffer.delClock();
		}
		return 0;
	}
	if (reorderBuffer.empty()) return 0;
	u=reorderBuffer.front();
	if (u.order==0x0ff00513) return 1;
//	printf("Clock=%d order=%08x nowpc=%08x prepc=%08x ready=%d head=%d tail=%d\n",
//		Clock,u.order,pc,u.pc,u.ready,reorderBuffer.getHead(),reorderBuffer.getTail());
//	for (int i=15;i<16;i++) printf("i=%d val=%08x sta=%d\n",i,regVal[i],regSta[i]);
//	printf("clock=%d pc=%08x order=%08x\n",Clock,u.pc,u.order);
	if (!u.ready) return 0;
//    if (Clock==133665)
//        cerr<<u.pc<<" "<<u.order<<endl;
//	printReg();
	unsigned op=u.order&127;
	int b=reorderBuffer.getHead();
	if (op==0b0100011){
//		puts("store");
		reorderBuffer.updClock(Clock);
		return 0;
	}
	if (op==0b1100011){
		if (u.value==1){
			reorderBuffer.clear();
			reservationStation.clear();
			loadStoreBuffer.clear();
			pc=u.pc+immB(u.order);
			for (int i=0;i<32;i++) regSta[i]=-1;
		}
		else reorderBuffer.pop();
	}
	else if (op==0b1101111 || op==0b1100111){
		reorderBuffer.clear();
		reservationStation.clear();
		loadStoreBuffer.clear();
		regVal[u.dest]=u.pc+4;
		pc=u.value;
//		printf("%04x %d %08x %d\n",pc,getRs1(u.order),getImm(u.order),u.dest);
		for (int i=0;i<32;i++) regSta[i]=-1;
	}
	else{
		reorderBuffer.pop();
//		if (op==0b0000011) printf("rd=%d val=%08x\n",u.dest,u.value);
		regVal[u.dest]=u.value;
//		if (u.pc==0x1008) printf("!!!!!!%d %08x\n",u.dest,u.value);
//		broadcast(u.dest,u.value);
		if (regSta[u.dest]==b) regSta[u.dest]=-1;
	}
	return 0;
}

void writeResult(){
	for (int i=0;i<32;i++){
		RsNode u=reservationStation.get(i);
		if (u.busy!=2) continue;
		RobNode v=reorderBuffer.get(u.dest);
		v.value=u.value,v.ready=1,u.busy=0;
		if ((u.order&127)!=0b1101111 || 
			(u.order&127)!=0b1100111)
				broadcast(u.dest,u.value);
		else broadcast(u.dest,u.pc+4);
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

bool isPreSameStore(const LSNode& u){
	int j=reorderBuffer.getHead();
//	if (u.pc==0x146c) printf("$$$$$head=%d dest=%d busy=%d offset=%d\n",j,u.dest,u.busy,u.offset);
	while(j!=u.dest){
		RobNode v=reorderBuffer.get(j);
		if ((v.order&127)==0b0100011) return 1;
//		if ((v.order&127)==0b0100011){
//			if (!v.ready) return 1;
//			unsigned delta=v.dest-u.value;
//			printf("%08x\n",delta);
//			if (delta==0u || delta==2u || delta==(unsigned)-2u) return 1;
//		}
		j=(j+1)&31;
	}
	return 0;
}

void execution(){
//	if (Clock>30) puts("fxxk1");
	for (int i=0;i<32;i++){
		RsNode u=reservationStation.get(i);
		if (u.busy!=1) continue;
		if (~u.qj || ~u.qk) continue;
		u.value=simulate(u.order,u.vj,u.vk,u.imm,u.pc);
		u.busy=2,reservationStation.put(i,u);
//		if (Clock==14 && i==0) printf("%d %d %d %08x\n",u.busy,u.qj,u.qk,u.value);
		break;
	}
	
//	if (Clock>30) puts("fxxk2");
	if (loadStoreBuffer.getClock()){
		if (Clock==loadStoreBuffer.getClock()+2){
			int i=loadStoreBuffer.getOpPos();
			LSNode u=loadStoreBuffer.get(i);
			u.value=load(u.order,u.value);
			u.busy=2;
			loadStoreBuffer.delClock();
			loadStoreBuffer.put(i,u);
		}
		return;
	}
//	if (Clock>30) puts("fxxk3");
	for (int i=0;i<32;i++){
		LSNode u=loadStoreBuffer.get(i);
		if (u.busy!=1) continue;
//		printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^%08x\n",u.pc);
		if (u.offset && !isPreSameStore(u)){
//			if (Clock==16) printf("%d %08x %d %d\n",i,u.order,u.offset,u.busy);
			loadStoreBuffer.updClock(Clock,i);
			return;
		}
		if (~u.qj || u.offset) continue;
		u.value=u.imm+u.vj;
		if ((u.order&127)==0b0000011) u.offset=1;
		else u.busy=2;//,printf("????????%08x %08x\n",u.imm,u.vj);
//		if (Clock==15) printf("%d %08x %d %d\n",i,u.order,u.offset,u.busy);
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
		op=(order&127),imm=getImm(order);
	v.busy=1,v.dest=b;
	u.order=v.order=order,u.pc=v.pc=pc;
	
	if (~regSta[rs1]){
		int x=regSta[rs1];
		if (reorderBuffer.isReady(x))
			v.vj=reorderBuffer.getVal(x),v.qj=-1;
		else v.qj=x;
//		printf("#####%08x %d %08x\n",v.vj,v.qj,v.order);
	}
	else v.vj=regVal[rs1],v.qj=-1;
	
	if (op==0b0100011){
		if (~regSta[rs2]){
			int x=regSta[rs2];
			if (reorderBuffer.isReady(x))
				v.vk=reorderBuffer.getVal(x),v.qk=-1;
			else v.qk=x;
		}
		else v.vk=regVal[rs2],v.qk=-1;
	}
	else regSta[u.dest=getRd(order)]=b;//,printf("#######%04x %d %d %d\n",pc,u.dest,regSta[u.dest],Clock);
	v.imm=imm;
	
//	if (pc==0x146c) printf("r=%d b=%d imm=%08x vj=%08x qj=%d rd=%d\n",r,b,imm,v.vj,v.qj,u.dest);
	
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
		op=(order&127),imm=getImm(order);
	v.busy=1,v.dest=b,u.dest=getRd(order);
	
//	printf("%04x\n",pc);
	
//	if (pc==0x1000) printf("Clock=%d rs=%d rob=%d order=%08x pc=%04x\n",Clock,r,b,order,pc);
	
	if (op!=0b0110111 && op!=0b0010111 && op!=0b1101111){
		if (~regSta[rs1]){
			int x=regSta[rs1];
			if (reorderBuffer.isReady(x))
				v.vj=reorderBuffer.getVal(x),v.qj=-1;
			else v.qj=x;
		}
		else v.vj=regVal[rs1],v.qj=-1;
		
		if (op!=0b0010011 && op!=0b1100111){
			if (~regSta[rs2]){
//				if (pc==0x1008) puts("fuck!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
				int x=regSta[rs2];
				if (reorderBuffer.isReady(x))
					v.vk=reorderBuffer.getVal(x),v.qk=-1;
				else v.qk=x;
			}
			else v.vk=regVal[rs2],v.qk=-1;
		}
//		if (pc==0x1008) printf("!!!!!!!%08x %d %08x %d %d %d\n",v.vj,v.qj,v.vk,v.qk,rs2,Clock),printReg();
	}
	v.imm=imm;
	
	if (op!=0b1100011) regSta[getRd(order)]=b;
	reorderBuffer.push(u);
	reservationStation.put(r,v);pc+=4;
}

void issue(unsigned order,unsigned& pc){
	if (reorderBuffer.full()) return;
//	printf("%d %08x %04x\n",Clock,order,pc);
	unsigned op=(order&127);
	if (op==0b0000011 || op==0b0100011)
		return issueLoadStore(order,pc);
	issueOthers(order,pc);
}

bool solve(unsigned& pc){
//	if (Clock==50) exit(0);
	Clock++;
//    if (Clock==)
//        puts("???");
	if (commit(pc)) return 1;regVal[0]=0;
	writeResult(),execution();
	issue(mem[pc]|(mem[pc+1]<<8)|(mem[pc+2]<<16)
		|(mem[pc+3]<<24),pc);
//	printf("Clock=%d:\n",Clock);
//	for (int i=0;i<32;i++) printf("%08x\n",regVal[i]);
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
