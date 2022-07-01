#include <cstdio>
#include <iostream>
#include <cstring>

#include "reservationStation.hpp"
#include "reorderBuffer.hpp"
#include "alu.hpp"
#include "loadStoreBuffer.hpp"

using namespace std;

const int memSize=500000;

unsigned regVal_now[32],regVal_pre[32],mem[memSize];
int regSta_now[32],regSta_pre[32];
int waitJump_now=-1,waitJump_pre=-1;
int predictor_now[32],predictor_pre[32];

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
//	printf("#########dst=%d val=%08x\n",dst,val);
	for (int j=0;j<32;j++){
		RsNode x=reservationStation_pre.get(j);
		if (x.qj==dst) x.vj=val,x.qj=-1;
		if (x.qk==dst) x.vk=val,x.qk=-1;
		reservationStation_now.put(j,x);
	}
	for (int j=0;j<32;j++){
		LSNode x=loadStoreBuffer_pre.get(j);
		if (x.qj==dst) x.vj=val,x.qj=-1;
		if (x.qk==dst) x.vk=val,x.qk=-1;
		loadStoreBuffer_now.put(j,x);
	}
}

int commit(unsigned& pc){
//	printf("%d\n",reorderBuffer_pre.isReady(30));
	RobNode u;
	if (reorderBuffer_pre.getClock()){
		reorderBuffer_now.subClock();
		if (!reorderBuffer_now.getClock()){
			u=reorderBuffer_now.front();
//			if (u.pc==0x1010) printf("?????????%032x %d\n",u.value,reorderBuffer_now.getHead());
			store(u.order,u.dest,u.value);
			reorderBuffer_now.pop();
		}
		return 0;
	}
	if (reorderBuffer_pre.empty()) return 0;
	u=reorderBuffer_pre.front();
	if (u.order==0x0ff00513) return 1;
//	printf("pc=%04x order=%08x\n",u.pc,u.order);
	if (!u.ready) return 0;
//	printf("pc=%04x order=%08x memval=%032x\n",
//	u.pc,u.order,mem[0x11b0]|(mem[0x11b1]<<8)|(mem[0x11b2]<<16)|(mem[0x11b3]<<24));
//	for (int i=0;i<32;i++) printf("i=%d %d\n",i,regVal_pre[i]);
	unsigned op=u.order&127;
	int b=reorderBuffer_pre.getHead();
	if (op==0b0100011){
		if (!reorderBuffer_pre.getClock())
			reorderBuffer_now.setClock();
		return 0;
	}
	if (op==0b1100011){
		unsigned pdt=(u.order>>7)&31;
		if (u.value==1){
			if (u.isJump)
				reorderBuffer_now.pop();
			else{
				reorderBuffer_now.clear();
				reservationStation_now.clear();
				loadStoreBuffer_now.clear();
				waitJump_now=-1;
				pc=u.dest;
				for (int i=0;i<32;i++) regSta_now[i]=-1;
//				puts("fuck1");
				return 2;
			}
			if (predictor_pre[pdt]<3) predictor_now[pdt]++; 
		}
		else{
			if (u.isJump){
				reorderBuffer_now.clear();
				reservationStation_now.clear();
				loadStoreBuffer_now.clear();
				waitJump_now=-1;
				pc=u.dest;
				for (int i=0;i<32;i++) regSta_now[i]=-1;
//				puts("fuck2");
				return 2;
			}
			else reorderBuffer_now.pop();
			if (predictor_pre[pdt]) predictor_now[pdt]--;
		}
	}
	else{
		reorderBuffer_now.pop();
		if (u.dest) regVal_now[u.dest]=u.value;
//		broadcast(b,u.value);
		if (regSta_pre[u.dest]==b) regSta_now[u.dest]=-1;
//		if (u.dest==8) printf("!!!!!!!!!!!!!pc=%04x b=%d sta=%d\n",u.pc,b,regSta_now[8]);
	}
	return 0;
}

void writeResult(unsigned& pc){
	for (int i=0;i<32;i++){
		RsNode u=reservationStation_pre.get(i);
		if (u.busy!=2) continue;
		RobNode v=reorderBuffer_pre.get(u.dest);
		v.value=u.value,v.ready=1,u.busy=0;
		if ((u.order&127)==0b1101111) v.value=u.pc+4;
		if ((u.order&127)==0b1100111)
			pc=u.value,waitJump_now=-1,v.value=u.pc+4;//,printf("!!!%04x\n",pc);
		broadcast(u.dest,v.value);
		reorderBuffer_now.put(u.dest,v);
		reservationStation_now.put(i,u);
		return;
	}
	
	for (int i=0;i<32;i++){
		LSNode u=loadStoreBuffer_pre.get(i);
		if (u.busy!=2) continue;
		if ((u.order&127)==0b0000011){
			RobNode v=reorderBuffer_pre.get(u.dest);
//			if (u.dest==30) printf("i=%d u.pc=%04x %d v.pc=%04x",i,u.pc,u.value,v.pc);
			v.value=u.value,v.ready=1,u.busy=0;
//			if (u.pc==0x1094) printf("%d %d\n",u.dest,reorderBuffer_now.getHead());
			broadcast(u.dest,u.value);
			reorderBuffer_now.put(u.dest,v);
			loadStoreBuffer_now.put(i,u);
			return;
		}
		if (~u.qk){
			RobNode v=reorderBuffer_pre.get(u.qk);
			if (v.ready)
				u.vk=v.value,u.qk=-1,
				loadStoreBuffer_now.put(i,u);
		}
		if (~u.qk) continue;
		RobNode v=reorderBuffer_pre.get(u.dest);
		v.dest=u.value,v.value=u.vk,v.ready=1,u.busy=0;
		
//		if (u.pc==0x1010) printf("!!!!!!!!%x %d %d\n",u.vk,u.dest,v.ready);
		
		reorderBuffer_now.put(u.dest,v);
		loadStoreBuffer_now.put(i,u);
		return;
	}
}

void execution(){
	for (int i=0;i<32;i++){
		RsNode u=reservationStation_pre.get(i);
		if (u.busy!=1) continue;
		if (~u.qj){
			RobNode v=reorderBuffer_pre.get(u.qj);
			if (v.ready)
				u.vj=v.value,u.qj=-1,
				reservationStation_now.put(i,u);
		}
		if (~u.qk){
			RobNode v=reorderBuffer_pre.get(u.qk);
			if (v.ready)
				u.vk=v.value,u.qk=-1,
				reservationStation_now.put(i,u);
		}
		if (~u.qj || ~u.qk) continue;
		u.value=simulate(u.order,u.vj,u.vk,u.imm,u.pc);
		u.busy=2,reservationStation_now.put(i,u);
		break;
	}
	
	if (loadStoreBuffer_pre.getClock()){
		loadStoreBuffer_now.subClock();
		if (!loadStoreBuffer_now.getClock()){
			int i=loadStoreBuffer_pre.getOpPos();
			LSNode u=loadStoreBuffer_pre.get(i);
			u.value=load(u.order,u.value,1);
			u.busy=2;
			loadStoreBuffer_now.put(i,u);
		}
	}
	for (int i=0;i<32;i++){
		LSNode u=loadStoreBuffer_pre.get(i);
		if (u.busy!=1) continue;
//		if (i==1 && u.pc==0x1094)
//			printf("%d\n",u.offset);
		if (u.offset){
			int j=reorderBuffer_pre.getHead(),fl=1,pos=0;
			for (;j!=u.dest;j=(j+1)&31){
				RobNode v=reorderBuffer_pre.get(j);
				if ((v.order&127)!=0b0100011) continue;
				if (!v.ready){fl=0;break;}
				if (v.dest==u.value) fl=2,pos=j;
			}
//			if (i==1 && u.pc==0x1094) printf("%d\n",fl);
			if (fl==0) continue;
			else if (fl==2){
				RobNode v=reorderBuffer_pre.get(pos);
				u.value=load(u.order,v.value,0);
				u.busy=2,loadStoreBuffer_now.put(i,u);
			}
			else if (!loadStoreBuffer_pre.getClock()
				 && u.value<memSize)
				loadStoreBuffer_now.setClock(i);
		}
		if (~u.qj){
			RobNode v=reorderBuffer_pre.get(u.qj);
			if (v.ready)
				u.vj=v.value,u.qj=-1,
				loadStoreBuffer_now.put(i,u);
		}
		if (~u.qj || u.offset) continue;
		u.value=u.imm+u.vj;
		if ((u.order&127)==0b0000011) u.offset=1;
		else u.busy=2;
		
//		if (u.pc==0x1010) printf("!!!!!!!!%x %d %d\n",u.value,reorderBuffer_pre.isReady(u.dest),u.dest);
		
		loadStoreBuffer_now.put(i,u);
		break;
	}
}

void issueLoadStore(unsigned order,unsigned& pc){
	int r=loadStoreBuffer_pre.getPos();
	if (r==-1) return;
	RobNode u;LSNode v;
	int b=reorderBuffer_now.getTail();
	unsigned rs1=getRs1(order),rs2=getRs2(order),
		op=(order&127),imm=getImm(order),rd=getRd(order);
	v.busy=1,v.dest=b;
	u.order=v.order=order,u.pc=v.pc=pc;
	
	if (~regSta_pre[rs1] && rs1){
		int x=regSta_pre[rs1];
		if (reorderBuffer_pre.isReady(x))
			v.vj=reorderBuffer_pre.getVal(x),v.qj=-1;
		else v.qj=x;
	}
	else v.vj=regVal_pre[rs1],v.qj=-1;
	
	if (op==0b0100011){
		if (~regSta_pre[rs2] && rs2){
			int x=regSta_pre[rs2];
			if (reorderBuffer_pre.isReady(x))
				v.vk=reorderBuffer_pre.getVal(x),v.qk=-1;
			else v.qk=x;
		}
		else v.vk=regVal_pre[rs2],v.qk=-1;
	}
	else regSta_now[u.dest=rd]=rd?b:-1;
	v.imm=imm;
	reorderBuffer_now.push(u);
	loadStoreBuffer_now.put(r,v);
	
//	if (b==30) printf("#####################%04x r=%d\n",pc,r);
//	
//	if (pc==0x1010) puts("fuck");
	
//	if (pc==0x10cc) printf("%x\n",imm);
//	if (pc==0x1094) printf("######vj=%d qj=%d r=%d rd=%d rs1=%d\n",v.vj,v.qj,r,rd,rs1);
	
	pc+=4;
}

void issueOthers(unsigned order,unsigned& pc){
//	printf("issue!!!!!order=%08x pc=%04x\n",order,pc);
	int r=reservationStation_pre.getPos();
	if (r==-1) return;
	RobNode u;RsNode v;
	u.order=v.order=order,u.pc=v.pc=pc;
	int b=reorderBuffer_now.getTail();
	unsigned rs1=getRs1(order),rs2=getRs2(order),
		op=(order&127),imm=getImm(order),rd=getRd(order);
	v.busy=1,v.dest=b,u.dest=rd;
	if (op!=0b0110111 && op!=0b0010111 && op!=0b1101111){
		if (~regSta_pre[rs1] && rs1){
			int x=regSta_pre[rs1];
//			RobNode uu=reorderBuffer_pre.get(x);
//			printf("@@@@%d %d %d\n",rs1,x,uu.ready);
			if (reorderBuffer_pre.isReady(x))
				v.vj=reorderBuffer_pre.getVal(x),v.qj=-1;
			else v.qj=x;
		}
		else v.vj=regVal_pre[rs1],v.qj=-1;
		
		if (op!=0b0010011 && op!=0b1100111){
			if (~regSta_pre[rs2] && rs2){
				int x=regSta_pre[rs2];
				if (reorderBuffer_pre.isReady(x))
					v.vk=reorderBuffer_pre.getVal(x),v.qk=-1;
				else v.qk=x;
			}
			else v.vk=regVal_pre[rs2],v.qk=-1;
		}
	}
	v.imm=imm;
	
	if (op!=0b1100011) regSta_now[rd]=(rd?b:-1);//,printf("!!!!pc=%04x rd=%d\n",pc,rd);
//	if (pc==0x1044) printf("b=%d qj=%d\n",b,v.qj);
	if (op==0b1101111) pc+=imm;
	else if (op==0b1100111) waitJump_now=r;
	else if (op==0b1100011){
		unsigned pdt=(order>>7)&31;
		if (predictor_pre[pdt]&2)
			u.dest=pc+4,pc+=imm,u.isJump=1;
		else u.dest=pc+imm,pc+=4,u.isJump=0;
	}
	else pc+=4;
	
	
	reorderBuffer_now.push(u);
	reservationStation_now.put(r,v);
}

void issue(unsigned order,unsigned& pc){
	if (reorderBuffer_pre.full()) return;
//	printf("%04x\n",pc);
	unsigned op=(order&127);
	if (op==0b0000011 || op==0b0100011)
		return issueLoadStore(order,pc);
	issueOthers(order,pc);
}

void copy(){
	waitJump_pre=waitJump_now;
	reorderBuffer_pre=reorderBuffer_now;
	loadStoreBuffer_pre=loadStoreBuffer_now;
	reservationStation_pre=reservationStation_now;
	for (int i=0;i<32;i++)
		regVal_pre[i]=regVal_now[i],
		regSta_pre[i]=regSta_now[i],
		predictor_pre[i]=predictor_now[i];
}

bool solve(unsigned& pc){
//	puts("___________________________________________________________");
//	printf("%04x %d\n",pc,reorderBuffer_now.getHead());
	copy();
	int ret=commit(pc);
	if (ret==1) return 1;
	if (ret==2) return 0;
	writeResult(pc);
	execution();
	if (!~waitJump_pre)
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
	for (int i=0;i<32;i++) regSta_pre[i]=regSta_now[i]=-1;
	unsigned pc=0;
	while(1) if (solve(pc)) break;
	printf("%d\n",regVal_now[10]&255u);
	return 0;
}
