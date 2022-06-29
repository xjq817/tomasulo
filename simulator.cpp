#include <bits/stdc++.h>
using namespace std;

unsigned reg[32],mem[500000],pc;

int trans(char c){
	if (isdigit(c)) return c^48;
	return c-'A'+10;
}

unsigned getRd(unsigned order){
	return ((order)>>7)&31;
}

unsigned getRs1(unsigned order){
	return ((order>>15)&31);
}

unsigned getRs2(unsigned order){
	return ((order>>20)&31);
}

unsigned immI(unsigned order){
	if (order&(1u<<31))
		return (-1u)^4095^(order>>20);
	return order>>20;
}

unsigned immS(unsigned order){
	unsigned imm=((order>>7)&31);
	imm|=((order>>25)<<5);
	if (order&(1u<<31))
		return (-1u)^4095^imm;
	return imm;
}

unsigned immB(unsigned order){
	unsigned imm=((order>>7)&31);
	if (imm&1) imm^=1,imm|=(1<<11);
	imm|=(((order>>25)&63)<<5);
	if (order&(1u<<31))
		return (-1u)^4095^imm;
	return imm;
}

unsigned immU(unsigned order){
	return (order>>12)<<12;
}

unsigned immJ(unsigned order){
	unsigned imm=((order>>20)&2047);
	if (imm&1) imm^=1,imm|=(1<<11);
	imm|=(((order>>12)&255)<<12);
	if (order&(1u<<31))
		return (-1u)^1048575^imm;
	return imm;
}

void Lui(unsigned order){
//	puts("Lui");
	reg[getRd(order)]=immU(order);
//	cout<<immU(order)<<endl;
	pc+=4;
}

void Auipc(unsigned order){
//	puts("Auipc");
	reg[getRd(order)]=immU(order)+pc;
	pc+=4;
}

void Jal(unsigned order){
//	puts("Jal");
	reg[getRd(order)]=pc+4;
	pc+=immJ(order);
}

void Jalr(unsigned order){
//	puts("Jalr");
	reg[getRd(order)]=pc+4;
	pc=(reg[getRs1(order)]+immI(order))&(-2u);
//	printf("%d %08x\n",getRs1(order),immI(order));
}

void Beq(unsigned order){
//	puts("Beq");
	if (reg[getRs1(order)]==reg[getRs2(order)])
		pc+=immB(order);
	else pc+=4;
}

void Bne(unsigned order){
//	puts("Bne");
	if (reg[getRs1(order)]!=reg[getRs2(order)])
		pc+=immB(order);
	else pc+=4;
}

void Blt(unsigned order){
//	puts("Blt");
	int rs1Val=reg[getRs1(order)];
	int rs2Val=reg[getRs2(order)];
	if (rs1Val<rs2Val) pc+=immB(order);
	else pc+=4;
}

void Bge(unsigned order){
//	puts("Bge");
	int rs1Val=reg[getRs1(order)];
	int rs2Val=reg[getRs2(order)];
	if (rs1Val>=rs2Val) pc+=immB(order);
	else pc+=4;
}

void Bltu(unsigned order){
//	puts("Bltu");
	if (reg[getRs1(order)]<reg[getRs2(order)])
		pc+=immB(order);
	else pc+=4;
}

void Bgeu(unsigned order){
//	puts("Bgeu");
	if (reg[getRs1(order)]>=reg[getRs2(order)])
		pc+=immB(order);
	else pc+=4;
}

void Lb(unsigned order){
//	puts("Lb");
	unsigned memVal=mem[reg[getRs1(order)]+immI(order)];
	if (memVal&127u) memVal|=(-1u)^(255u);
	reg[getRd(order)]=memVal;
	pc+=4;
}

void Lh(unsigned order){
//	puts("Lh");
	unsigned addr=immI(order)+reg[getRs1(order)];
	unsigned memVal=mem[addr]|(mem[addr+1]<<8);
	if (memVal&32767u) memVal|=(-1u)^(65535u);
	reg[getRd(order)]=memVal;
	pc+=4;
}

void Lw(unsigned order){
//	puts("Lw");
	unsigned addr=immI(order)+reg[getRs1(order)];
	reg[getRd(order)]=mem[addr]|(mem[addr+1]<<8)|(mem[addr+2]<<16)|(mem[addr+3]<<24);
//	printf("%08x %d %08x\n",addr,getRd(order),mem[addr]|(mem[addr+1]<<8)|(mem[addr+2]<<16)|(mem[addr+3]<<24));
	pc+=4;
}

void Lbu(unsigned order){
//	puts("Lbu");
	reg[getRd(order)]=mem[reg[getRs1(order)]+immI(order)];
	pc+=4;
}

void Lhu(unsigned order){
//	puts("Lhu");
	unsigned addr=immI(order)+reg[getRs1(order)];
	reg[getRd(order)]=mem[addr]|(mem[addr+1]<<8);
	pc+=4;
}

void Sb(unsigned order){
//	puts("Sb");
	mem[reg[getRs1(order)]+immS(order)]=reg[getRs2(order)]&255;
	pc+=4;
}

void Sh(unsigned order){
//	puts("Sh");
	unsigned addr=immS(order)+reg[getRs1(order)];
	mem[addr]=reg[getRs2(order)]&255;
	mem[addr+1]=(reg[getRs2(order)]>>8)&255;
	pc+=4;
}

void Sw(unsigned order){
//	puts("Sw");
	unsigned addr=immS(order)+reg[getRs1(order)];
//	printf("%08x %08x %08x\n",immS(order),reg[getRs1(order)],reg[getRs2(order)]);
	mem[addr]=reg[getRs2(order)]&255;
	mem[addr+1]=(reg[getRs2(order)]>>8)&255;
	mem[addr+2]=(reg[getRs2(order)]>>16)&255;
	mem[addr+3]=reg[getRs2(order)]>>24;
	pc+=4;
}

void Addi(unsigned order){
//	if (pc==0x108c) printf("%d %d %08x %08x\n",getRd(order),getRs1(order),reg[getRs1(order)],immI(order));
//	puts("Addi");
	reg[getRd(order)]=reg[getRs1(order)]+immI(order);
	pc+=4;
}

void Slti(unsigned order){
//	puts("Slti");
	int imm=immI(order);
	int rs1Val=reg[getRs1(order)];
	reg[getRd(order)]=(rs1Val<imm);
	pc+=4;
}

void Sltiu(unsigned order){
//	puts("Sltiu");
	reg[getRd(order)]=(reg[getRs1(order)]<immI(order));
	pc+=4;
}

void Xori(unsigned order){
//	puts("Xori");
	reg[getRd(order)]=(reg[getRs1(order)]^immI(order));
	pc+=4;
}

void Ori(unsigned order){
//	puts("Ori");
	reg[getRd(order)]=(reg[getRs1(order)]|immI(order));
	pc+=4;
}

void Andi(unsigned order){
//	puts("Andi");
	reg[getRd(order)]=(reg[getRs1(order)]&immI(order));
	pc+=4;
}

void Slli(unsigned order){
//	puts("Slli");
	reg[getRd(order)]=(reg[getRs1(order)]<<immI(order));
	pc+=4;
}

void Srli(unsigned order){
//	puts("Srli");
	reg[getRd(order)]=(reg[getRs1(order)]>>immI(order));
	pc+=4;
}

void Srai(unsigned order){
//	puts("Srai");
	order^=(1u<<30);
	reg[getRd(order)]=(reg[getRs1(order)]>>immI(order));
	if (reg[getRs1(order)]&(1u<<31))
		reg[getRd(order)]|=(-1u)^((1u<<(32-immI(order)))-1);
	pc+=4;
}

void Add(unsigned order){
//	puts("Add");
	reg[getRd(order)]=reg[getRs1(order)]+reg[getRs2(order)];
	pc+=4;
}

void Sub(unsigned order){
//	puts("Sub");
	reg[getRd(order)]=reg[getRs1(order)]-reg[getRs2(order)];
	pc+=4;
}

void Sll(unsigned order){
//	puts("Sll");
	reg[getRd(order)]=reg[getRs1(order)]<<(reg[getRs2(order)]&31);
	pc+=4;
}

void Slt(unsigned order){
//	puts("Slt");
	int rs1Val=reg[getRs1(order)];
	int rs2Val=reg[getRs2(order)];
	reg[getRd(order)]=(rs1Val<rs2Val);
	pc+=4;
}

void Sltu(unsigned order){
//	puts("Sltu");
	reg[getRd(order)]=(reg[getRs1(order)]<reg[getRs2(order)]);
	pc+=4;
}

void Xor(unsigned order){
//	puts("Xor");
//	if (pc==0x1008)
//		printf("%d %08x %08x %08x\n",getRs2(order),reg[getRs1(order)],reg[getRs2(order)],reg[getRs1(order)]^reg[getRs2(order)]);
	reg[getRd(order)]=(reg[getRs1(order)]^reg[getRs2(order)]);
	pc+=4;
}

void Srl(unsigned order){
//	puts("Srl");
	reg[getRd(order)]=reg[getRs1(order)]>>(reg[getRs2(order)]&31);
	pc+=4;
}

void Sra(unsigned order){
//	puts("Sra");
	reg[getRd(order)]=reg[getRs1(order)]>>(reg[getRs2(order)]&31);
	if (reg[getRs1(order)]&(1u<<31))
		reg[getRd(order)]|=(-1u)^((1u<<(32-(reg[getRs2(order)]&31)))-1);
	pc+=4;
}

void Or(unsigned order){
//	puts("Or");
	reg[getRd(order)]=(reg[getRs1(order)]|reg[getRs2(order)]);
	pc+=4;
}

void And(unsigned order){
//	puts("And");
	reg[getRd(order)]=(reg[getRs1(order)]&reg[getRs2(order)]);
	pc+=4;
}

void simulate(unsigned order){
	if ((order&127)==0b0110111) Lui(order);
	if ((order&127)==0b0010111) Auipc(order);
	if ((order&127)==0b1101111) Jal(order);
	if ((order&127)==0b1100111) Jalr(order);
	if ((order&127)==0b1100011){
		if (((order>>12)&7)==0b000) Beq(order);
		if (((order>>12)&7)==0b001) Bne(order);
		if (((order>>12)&7)==0b100) Blt(order);
		if (((order>>12)&7)==0b101) Bge(order);
		if (((order>>12)&7)==0b110) Bltu(order);
		if (((order>>12)&7)==0b111) Bgeu(order);
	}
	if ((order&127)==0b0000011){
		if (((order>>12)&7)==0b000) Lb(order);
		if (((order>>12)&7)==0b001) Lh(order);
		if (((order>>12)&7)==0b010) Lw(order);
		if (((order>>12)&7)==0b100) Lbu(order);
		if (((order>>12)&7)==0b101) Lhu(order);
	}
	if ((order&127)==0b0100011){
		if (((order>>12)&7)==0b000) Sb(order);
		if (((order>>12)&7)==0b001) Sh(order);
		if (((order>>12)&7)==0b010) Sw(order);
//		printf("dest=%08x value=%08x\n",immS(order)+reg[getRs1(order)],reg[getRs2(order)]);
	}
	if ((order&127)==0b0010011){
		if (((order>>12)&7)==0b000) Addi(order);
		if (((order>>12)&7)==0b010) Slti(order);
		if (((order>>12)&7)==0b011) Sltiu(order);
		if (((order>>12)&7)==0b100) Xori(order);
		if (((order>>12)&7)==0b110) Ori(order);
		if (((order>>12)&7)==0b111) Andi(order);
		if (((order>>12)&7)==0b001) Slli(order);
		if (((order>>12)&7)==0b101){
			if (((order>>25)&127)==0b0000000) Srli(order);
			if (((order>>25)&127)==0b0100000) Srai(order);
		}
	}
	if ((order&127)==0b0110011){
		if (((order>>12)&7)==0b000){
			if (((order>>25)&127)==0b0000000) Add(order);
			if (((order>>25)&127)==0b0100000) Sub(order);
		}
		if (((order>>12)&7)==0b001) Sll(order);
		if (((order>>12)&7)==0b010) Slt(order);
		if (((order>>12)&7)==0b011) Sltu(order);
		if (((order>>12)&7)==0b100) Xor(order);
		if (((order>>12)&7)==0b101){
			if (((order>>25)&127)==0b0000000) Srl(order);
			if (((order>>25)&127)==0b0100000) Sra(order);
		}
		if (((order>>12)&7)==0b110) Or(order);
		if (((order>>12)&7)==0b111) And(order);
	}
}

int main(){
	freopen("bulgarian.data","r",stdin);
	freopen("std.txt","w",stdout);
	char c[15];
	unsigned addr=0;
	while(~scanf("%s",c)){
		if (c[0]=='@'){
			for (int i=1;i<=8;i++)
				addr=(addr<<4)|trans(c[i]);
		}
		else mem[addr++]=(trans(c[0])<<4)|trans(c[1]);
	}
	while(1){
		unsigned order=mem[pc]|(mem[pc+1]<<8)|(mem[pc+2]<<16)|(mem[pc+3]<<24);
		if (order==0x0ff00513) break;
		printf("order=%08x pc=%08x\n",order,pc);
		for (int i=0;i<32;i++) printf("i=%d %08x\n",i,reg[i]);
		simulate(order);reg[0]=0;
//		system("pause");
	}
	printf("%d\n",reg[10]&255u);
	return 0;
}
