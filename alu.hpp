#ifndef ALU_HPP
#define ALU_HPP

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

unsigned getImm(unsigned order){
	unsigned op=(order&127);
	if (op==0b0110111 || op==0b0010111)
		return immU(order);
	if (op==0b1101111) return immJ(order);
	if (op==0b1100111 || op==0b0000011)
		return immI(order);
	if (op==0b0010011){
		if (((order>>12)&7)==0b101 && 
			((order>>25)&127)==0b0100000)
			order^=(1<<30);
		return immI(order);
	}
	if (op==0b1100011) return immB(order);
	return immS(order);
}

unsigned Sra(unsigned vj,unsigned vk){
	unsigned v=vj>>vk;
	if (vj&(1u<<31))
		v|=(-1u)^((1u<<(32-vk))-1);
	return v;
}

unsigned simulate(unsigned order,unsigned vj,unsigned vk,unsigned imm,unsigned pc){
	unsigned op=(order&127);
	if (op==0b0110111) return imm;
	if (op==0b0010111) return imm+pc;
	if (op==0b1101111) return pc+imm;
	if (op==0b1100111) return (vj+imm)&(-2u);
	if (op==0b1100011){
		if (((order>>12)&7)==0b000) return vj==vk;
		if (((order>>12)&7)==0b001) return vj!=vk;
		if (((order>>12)&7)==0b100) return (int)vj<(int)vk;
		if (((order>>12)&7)==0b101) return (int)vj>=(int)vk;
		if (((order>>12)&7)==0b110) return vj<vk;
		if (((order>>12)&7)==0b111) return vj>=vk;
	}
	if (op==0b0010011){
		if (((order>>12)&7)==0b000) return vj+imm;
		if (((order>>12)&7)==0b010) return (int)vj<(int)imm;
		if (((order>>12)&7)==0b011) return vj<imm;
		if (((order>>12)&7)==0b100) return vj^imm;
		if (((order>>12)&7)==0b110) return vj|imm;
		if (((order>>12)&7)==0b111) return vj&imm;
		if (((order>>12)&7)==0b001) return vj<<imm;
		if (((order>>12)&7)==0b101){
			if (((order>>25)&127)==0b0000000) return vj>>imm;
			if (((order>>25)&127)==0b0100000) return Sra(vj,imm);
		}
	}
	if (op==0b0110011){
		if (((order>>12)&7)==0b000){
			if (((order>>25)&127)==0b0000000) return vj+vk;
			if (((order>>25)&127)==0b0100000) return vj-vk;
		}
		if (((order>>12)&7)==0b001) return vj<<(vk&31);
		if (((order>>12)&7)==0b010) return (int)vj<(int)vk;
		if (((order>>12)&7)==0b011) return vj<vk;
		if (((order>>12)&7)==0b100) return vj^vk;
		if (((order>>12)&7)==0b101){
			if (((order>>25)&127)==0b0000000) return vj>>(vk&31);
			if (((order>>25)&127)==0b0100000) return Sra(vj,vk&31);
		}
		if (((order>>12)&7)==0b110) return vj|vk;
		if (((order>>12)&7)==0b111) return vj&vk;
	}
	return 0;
}

#endif
