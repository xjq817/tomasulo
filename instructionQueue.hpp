#ifndef INSTRUCTIONQUEUE_HPP
#define INSTRUCTIONQUEUE_HPP

#include <queue>

using std::queue;

struct InsNode{
	unsigned order,pc;
};

queue<InsNode>instructionQueue;

#endif
