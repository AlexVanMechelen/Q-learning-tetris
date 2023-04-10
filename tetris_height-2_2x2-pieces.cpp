// Author: S Melax (https://melax.github.io/tetris/tetris.html)
// Code documented by: Alex Van Mechelen

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <iostream>
#include <cstdlib> // for rand() and srand()


#define WIDTH (6)     // game width (height = 2)
float gamma = 0.80f;  // discount factor
float alpha = 0.02f;  // learning rate
int epsilon = 1;      // chance in % to explore using a random action (epsilon-greedy)

float Q[1<<(WIDTH+WIDTH)];  // utility value -> array of size 2^(2*WIDTH) -> One utility value per possible board state
int   P[1<<(WIDTH+WIDTH)];  // counter (not really needed)
int height =0;
int explore=0;  // flag I used to experiment with exploration


unsigned result(unsigned state, unsigned action, unsigned piece) 
{
	piece <<= action; // Move the piece horizontally to the position indicated by 'action'
	while((piece & state) || (piece <<WIDTH & state)) // While there is a part of the piece touching one or more blocks in the state
		piece <<=WIDTH; // Move the piece one layer up

	unsigned t = piece|state; // t represents the new board, with the piece placed as low as possible, give the previous state (board) and action (horizontal position)

	// Say this is the state:  010110  -> Row one
	//                         110110  -> Row zero
	if( (t&(((1<<WIDTH)-1)<<WIDTH)) == (((1<<WIDTH)-1)<<WIDTH)) // Checks if row one is completely filled
		t= (t&((1<<WIDTH)-1)) | ((t>>(2*WIDTH))<<WIDTH); // New t is row zero and rows two and three get shifted down to form row one and two (= delete row one)

	if( (t&((1<<WIDTH)-1)) == ((1<<WIDTH)-1)) // Checks if row zero is completely filled
		t>>=WIDTH; // Move all rows above zero down by one (= delete row zero)

	return t; // Return new state (can have up to height 4)
}

void init(){ // Initialize all utility values to zero
	for(int i=0;i<(1<< 2*WIDTH);i++) 
	{
		Q[i] = 0.0f; // Initialize all utility values to zero
		P[i] = 0; // counter (not used)
	}
}
unsigned rotate(unsigned p,int r) // Takes in a piece [(WIDTH+2)-bit number] and a rotation (0-3) -> Rotates the piece r number of times and returns the new piece
{
	while(r--) // Rotate r number of times (decrease r until it's zero; when zero the while loop won't execute anymore)
	{
		unsigned q = p>>WIDTH; // q stores the bits of the top 2 blocks of the piece
		p = ((p&1)?2:0) + ((p&2)?2<<WIDTH:0) +
		    ((q&2)?1<<WIDTH:0) + ((q&1)?1:0); // This is bitwise code to rotate the piece (if we want larger pieces we'll have to implement our own)
	}
	if(! (p%(1<<WIDTH))) // This is bitwise code to rotate the piece (if we want larger pieces we'll have to implement our own)
		p >>=WIDTH;

	return p;
}


unsigned crank(unsigned state,unsigned piece)
{
	// find the best next state
	float best =-999999999.9f; // Initialize best next state to -Inf (any state is better)
	unsigned t = 0;
	int loss=9999; // Number of rows added to height when they're 'pushed down' (2xWIDTH board 'moves up')

	int randomNumber = std::rand() % 100 + 1;

	if (randomNumber < epsilon) //random action
	{
		//chose a random action
		int a = std::rand() % WIDTH-1; //random horizontal position
		int r = std::rand() % 3; //random rotation

		unsigned t = result(state,a,rotate(piece,r)); // Return the game board that would result from placing 'piece' at horizontal position 'a' with rotation 'r'. Can have up to height 4 
		int loss=0; // Variable to keep track of how many rows were shifted up (number of extra rows above the normal 2)
		while(t>>(2*WIDTH)) // While there is part of a piece above row one
		{
			t>>=WIDTH; // Throw the bottom row away (shift all rows down by one)
			loss++; // Increase the counter that keeps track of how many rows were shifted up (number of extra rows above the normal 2)
		}
		assert(t<(1<< 2*WIDTH)); // Throws an error if there's still part of a piece above row one, which should normally not be the case anymore
		best = loss*-100 + gamma * Q[t]; // Update the current score for a next state
	}
	else 	// take action maximising the q value
	{
		for(int a=0;a<WIDTH-1;a++) // Loop over all horizontal positions a
		{
			for(int r=0;r<3;r++) // Loop over all possible rotations r of the piece
			{
				unsigned n = result(state,a,rotate(piece,r)); // Return the game board that would result from placing 'piece' at horizontal position 'a' with rotation 'r'. Can have up to height 4 
				int l=0; // Variable to keep track of how many rows were shifted up (number of extra rows above the normal 2)
				while(n>>(2*WIDTH)) // While there is part of a piece above row one
				{
					n>>=WIDTH; // Throw the bottom row away (shift all rows down by one)
					l++; // Increase the counter that keeps track of how many rows were shifted up (number of extra rows above the normal 2)
				}
				assert(n<(1<< 2*WIDTH)); // Throws an error if there's still part of a piece above row one, which should normally not be the case anymore
				// if((l<loss)||(l==loss && loss*-100 + gamma * Q[n] > best) ){
				if(l*-100 + gamma * Q[n] > best) // If the discount factor times the Q value of this new position (score for how good this new position is) MINOUS a punishment for the number of rows lost (l*-100) is bigger than the current best score for a next state
				{
					t=n; // Save the current best next state
					loss = l; // Save the loss of the current best next state
					best = loss*-100 + gamma * Q[t]; // Update the current best score for a next state
				}
			}
		} // At the end of this loop, the state with the highest 'best' score will still be saved in 't' and its loss in 'loss'
	}
	//if(loss)printf("Lost %d rows\n",loss);
	height += loss; 
	Q[state] = (1-alpha) * Q[state] + alpha * best; // update Q[state] based on result from state s to t 
	
	// average learning rule:  (it was useless)
	//Q[state] = (P[state] * Q[state] + alpha * best)/(1+P[state]);
	P[state] ++; // counter (not used)

	if(explore) // Simulates a random action being taken and a random rotation. This is useless since we already iterated over all possibilies in the code here above. This is why eventually the author realized this and set explore=0
	{
		loss=0;
		t = result(state,rand()%(WIDTH-1),rand()%4);
		while(t>>(2*WIDTH)) 
		{
			t>>=WIDTH;
		}
		//state=(state+1)%(1<<(2*WIDTH));
	}

	state = t;  // move to new state;
	return state;
}

int main(int,char**)
{
	init(); // Initialize all utility values to zero
	int game=0;
	while(game<1<<13) // Play 2^13 games, each consists of 10000 pieces
	{
		//explore = (game%2);  // doing exploration didn't help learning
		srand(0);
		height=0; // This variable keeps track of how heigh the pieces stack up (The game only considers a 2xWIDTH playable game space. If a new piece gets placed in a way that the game can't fit in this 2xWIDTH space and (an) incompleted row(s) get(s) pushed downward, height increases)
		unsigned state =0; // Keeps track of the board state (can take on values from 0 to 2^(2*WIDTH))
		for(int i=0;i<10000;i++) // 10000 pieces get added before the game is over
		{
			unsigned piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
			state = crank(state,piece); // Do a full Q-learning iteration for the current state and piece. Both the state and Q-table get updated
		}
		game++;
		if(0==(game & (game-1)))  // if a power of 2 -> Print the game number + the height of that game (= performance measure)
			printf("%4d %4d %s\n",game,height,(explore)?"learning":"");

	}
	return 0;
}
