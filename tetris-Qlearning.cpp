// Based on: S Melax (https://melax.github.io/tetris/tetris.html)

#include <stack>
#include <queue>
#include <bitset>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define WIDTH (6)     // game width (height = 2)
float gamma = 0.80f;  // discount factor
float alpha = 0.02f;  // learning rate

float Q[1<<(WIDTH+WIDTH)];  // utility value -> array of size 2^(2*WIDTH) -> One utility value per possible board state
int   P[1<<(WIDTH+WIDTH)];  // counter (not really needed)
int height =0;
int explore=0;  // flag I used to experiment with exploration

using namespace std;
stack<unsigned> row_LIFO_stack_above; // LIFO stack that keeps track of the rows above the current inspected 2xWIDTH frame (gets filled when there are 'holes' present and we move downward to old layers)
stack<unsigned> row_LIFO_stack_below; // LIFO stack that keeps track of the rows below the current inspected 2xWIDTH frame (used to save old rows when the top pieces build outside the 2xWIDTH frame)
stack<unsigned> row_LIFO_stack_above_two; // LIFO stack that keeps track of the rows above the current inspected 2xWIDTH frame and gets used in the crank function

void empty_queue(queue<unsigned>& q) { // Function used to empty the 'row_FIFO_queue_below_tmp' at the beginning of each next state evaluation
    queue<unsigned> empty; // Initialize an empty queue
    q.swap(empty); // Swap the input queue with the empty one
}

void empty_stack(stack<unsigned>& q) {
    stack<unsigned> empty; // Initialize an empty stack
    q.swap(empty); // Swap the input queue with the empty one
}

unsigned result(unsigned state, unsigned action, unsigned piece) 
{
	piece <<= action; // Move the piece horizontally to the position indicated by 'action'
	while((piece & state) || ((piece <<WIDTH & state) && ((piece<<1 & state) || (action == WIDTH-1)) && ((piece>>1 & state) || (action == 0)))) // Check if the piece could have gotten into this position, if not -> go inside this loop
		piece <<=WIDTH; // Move the piece one layer up

	unsigned t = piece|state; // t represents the new board, with the piece placed as low as possible, give the previous state (board) and action (horizontal position)

	// Say this is the state:  010110  -> Row one
	//                         110110  -> Row zero
	if( (t&(((1<<WIDTH)-1)<<WIDTH)) == (((1<<WIDTH)-1)<<WIDTH)) // Checks if row one is completely filled
	{
		t= (t&((1<<WIDTH)-1)) | ((t>>(2*WIDTH))<<WIDTH); // New t is row zero and rows two and three get shifted down to form row one and two (= delete row one)
	}

	if( (t&((1<<WIDTH)-1)) == ((1<<WIDTH)-1)) // Checks if row zero is completely filled
	{
		t>>=WIDTH; // Move all rows above zero down by one (= delete row zero)
	}

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

template <typename T>
std::stack<T> copy_top_two_from_LIFO_stack(std::stack<T> row_LIFO_stack_above)
{
	stack<unsigned> row_LIFO_stack_above_two; // LIFO stack that keeps track of the rows above the current inspected 2xWIDTH frame and gets used in the crank function
	if (row_LIFO_stack_above.size() >= 2) {
        unsigned temp1 = row_LIFO_stack_above.top();
        row_LIFO_stack_above.pop();
        unsigned temp2 = row_LIFO_stack_above.top();
        row_LIFO_stack_above_two.push(temp2);
        row_LIFO_stack_above_two.push(temp1);
        row_LIFO_stack_above.push(temp1); // Push the values back to the first stack to keep it unchanged
    } else if (row_LIFO_stack_above.size() == 1) {
        unsigned temp = row_LIFO_stack_above.top();
        row_LIFO_stack_above_two.push(temp);
    }
	return row_LIFO_stack_above_two;
}

template <typename T>
std::stack<T> duplicateStack(std::stack<T> firstStack) {
    std::stack<T> secondStack;
    std::stack<T> tempStack;
    // Copy the values from the first stack to the temp stack
    while (!firstStack.empty()) {
        tempStack.push(firstStack.top());
        firstStack.pop();
    }
    // Copy the values from the temp stack to the first and second stacks
    while (!tempStack.empty()) {
        T temp = tempStack.top();
        firstStack.push(temp);
        secondStack.push(temp);
        tempStack.pop();
    }
    return secondStack;
}

template <typename T>
std::queue<T> duplicateQueue(std::queue<T> firstQueue) {
    std::queue<T> secondQueue;
    std::queue<T> tempQueue;
    // Copy the values from the first queue to the temp queue
    while (!firstQueue.empty()) {
        tempQueue.push(firstQueue.front());
        firstQueue.pop();
    }
    // Copy the values from the temp queue to the first and second queues
    while (!tempQueue.empty()) {
        T temp = tempQueue.front();
        firstQueue.push(temp);
        secondQueue.push(temp);
        tempQueue.pop();
    }
    return secondQueue;
}

unsigned crank(unsigned state,unsigned piece,unsigned last_hole_idx = (1<<WIDTH)-1)
{
	unsigned close_hole_idx = last_hole_idx | last_hole_idx<<1 | last_hole_idx>>1; // Get all idx in which a new piece can end up
	// find the best next state
	float best =-999999999.9f; // Initialize best next state to -Inf (any state is better)
	unsigned t = 0;
	int loss=9999; // Number of rows added to height when they're 'pushed down' (2xWIDTH board 'moves up')
	queue<unsigned> row_FIFO_queue_below_best; // FIFO queue that keeps track of the rows beneath the top 2 ones. Stores the FIFO queue for the current best next state
	queue<unsigned> row_FIFO_queue_below_tmp; // FIFO queue that keeps track of the rows beneath the top 2 ones. Stores the FIFO queue for one current next state evaluation
	int num_completed_rows = 0; // Set number of completed rows for this play to zero
	for(int a=0;a<WIDTH-1;a++) // Loop over all horizontal positions a
	{
      unsigned pos = (1<<a); // Get insert position
	  if ((close_hole_idx & pos) != pos) // If this pos can't be reached via the holes
	  {
		continue; // Move to the next pos
	  }
	  for(int r=0;r<3;r++) // Loop over all possible rotations r of the piece
	  {
		empty_queue(row_FIFO_queue_below_tmp); // Empty the tmp FIFO queue that keeps track of the rows beneath the top 2 ones for this current next state evaluation
		stack<unsigned> row_LIFO_stack_above_two_tmp; // LIFO stack that keeps track of the rows above the current inspected 2xWIDTH frame and gets used in the crank function
		row_LIFO_stack_above_two_tmp = duplicateStack(row_LIFO_stack_above_two); // Fill the tmp LIFO stack that keeps track of the two rows above the current 2xWIDTH frame in 'state'
		unsigned n = result(state,a,rotate(piece,r)); // Return the game board that would result from placing 'piece' at horizontal position 'a' with rotation 'r'. Can have up to height 4 
		int l=0; // Variable to keep track of how many rows were shifted up (number of extra rows above the normal 2)
		int collision=0; // Bool that keeps track if there's a collision
		while((!collision) && n>>(2*WIDTH)) // While no collision && there is part of a piece above row one
		{
			unsigned bottom_row = n & ((1<<WIDTH)-1); // Extract the bottom row
			row_FIFO_queue_below_tmp.push(bottom_row); // Add the bottom row to a FIFO queue (for later retrieval)
			n>>=WIDTH; // Throw the bottom row away (shift all rows down by one)
			l++; // Increase the counter that keeps track of how many rows were shifted up (number of extra rows above the normal 2)
			unsigned extra_row_one = 0;
			if(row_LIFO_stack_above_two_tmp.size()) // If there are entries in the LIFO above tmp stack
			{
				extra_row_one = row_LIFO_stack_above_two_tmp.top(); // Extract the extra saved blocks on row one that used to be above the current frame in 'state'
				row_LIFO_stack_above_two_tmp.pop();
			}
			if(extra_row_one & n>>WIDTH) // If there's a collision
			{
				collision = 1;
				break;
			}
			n = n | extra_row_one<<WIDTH; // Update n according to the previous row above
		}
		if(collision) // If there's a collision
		{
			continue; // Go to the next iteration (not even consider it as a next state)
		}
		assert(n<(1<< 2*WIDTH)); // Throws an error if there's still part of a piece above row one, which should normally not be the case anymore
		// if((l<loss)||(l==loss && loss*-100 + gamma * Q[n] > best) ){
		if(l*-100 + gamma * Q[n] > best) // If the discount factor times the Q value of this new position (score for how good this new position is) MINOUS a punishment for the number of rows lost (l*-100) is bigger than the current best score for a next state
		{
			t=n; // Save the current best next state
			loss = l; // Save the loss of the current best next state
			best = loss*-100 + gamma * Q[t]; // Update the current best score for a next state
			row_FIFO_queue_below_best = duplicateQueue(row_FIFO_queue_below_tmp); // Save a duplicate of this move's below FIFO queue
		}
	  }
	} // At the end of this loop, the state with the highest 'best' score will still be saved in 't' and its loss in 'loss'

	while(row_FIFO_queue_below_best.size()) // While there are entries in the below_best FIFO queue
	{
		row_LIFO_stack_below.push(row_FIFO_queue_below_best.front()); // Add them to the main below LIFO stack
		row_FIFO_queue_below_best.pop(); // Remove them from the temporary FIFO queue
	}

	for (int l=0; l<loss; l++) // Loop for loss number of times
	{
		if(row_LIFO_stack_above.size()) // If there are entries in the LIFO above stack
		{
			row_LIFO_stack_above.pop(); // Remove these rows from the real LIFO above stack because they are already used in the current 'state'
		}
	}

	//printf("%4d\n",Q[state]);

	//if(loss)printf("Lost %d rows\n",loss);
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

	while (row_LIFO_stack_above.size()) // While there are entries in the above LIFO stack
	{
		unsigned bottom_row = state & ((1<<WIDTH)-1); // Extract the bottom row
		row_LIFO_stack_below.push(bottom_row); // Add the bottom row of the state to the main below LIFO stack
		state>>=WIDTH; // Shift the state one row down
		unsigned new_top_row = row_LIFO_stack_above.top(); // Extract a new top row for the state from the LIFO above stack
		row_LIFO_stack_above.pop(); // Remove it from the above LIFO stack
		state = state | new_top_row<<WIDTH; // Add the new top row to the state
	}

	height = row_LIFO_stack_below.size() + 2;

	return state;
}

unsigned find_holes(unsigned state,unsigned piece,unsigned last_hole_idx = (1<<WIDTH)-1) // If no thrid argument is supplied it will assume all horizontal positions are possible positions the piece could come from
{
	unsigned check_piece_width = 1; // One or two depending on the piece width
	unsigned check_piece = (1<<WIDTH) + 1; // Check_piece has height 2 and width 1 or 2, depending on the piece
	bitset<32> b(piece); // To count the number of bits in piece that are one
	if(b.count() > 2 || piece == (1<<WIDTH) + 2 || piece == (1<<WIDTH+1) + 1) // If the piece has width one
	{
		check_piece = (3<<WIDTH) + 3; // Make check_piece a square
		check_piece_width = 2;
	}
	unsigned hole_idx = 0; // Keeps track of the hole positions (contains a one at a horizontal position if there's a hole (2 deep) at that horizontal position)
	for(int a=0;a<WIDTH-check_piece_width+1;a++) // Loop over all horizontal positions a (if the check_piece width equals 2 there are 5 positions, while there are 6 for the piece width of 1)
	{
		if (!(check_piece & state)) // If there's a hole at this horizontal index a
		{
			hole_idx += 1<<a; // Add a one in the hole_idx at this horizontal index a
			if (check_piece_width == 2)
			{
				hole_idx += 2<<a; // Add a one in the hole_idx left to the current one, since this has already been tested
			}
		}
		check_piece<<=1; // Shift the check_piece horizontally to the left (next check position)
	}
	hole_idx = (hole_idx & last_hole_idx) | (hole_idx & (last_hole_idx>>1)) | (hole_idx & (last_hole_idx<<1)); // Keep only the hole indexed that can be reached from the last hole indexes
	return hole_idx;
}

unsigned Qlearning_iteration(unsigned state, unsigned piece, unsigned last_hole_idx = (1<<WIDTH)-1)
{
	unsigned hole_idx = find_holes(state, piece, last_hole_idx); // Find holes that can be reached from the hole on the layer above (last_hole_idx) and save them
	while(hole_idx && row_LIFO_stack_below.size()) // While there are still holes present && there are saved rows below the current 2xWIDTH frame 'state'
	{
		unsigned top_row = state>>WIDTH & ((1<<WIDTH)-1); // Extract the top row
		row_LIFO_stack_above.push(top_row); // Push the top row to the above LIFO stack
		state = (state<<WIDTH & ((1<<WIDTH+WIDTH)-1)); // Move state one row up and forget the top row
		state = state | row_LIFO_stack_below.top(); // Add old row from below off the below LIFO stack
		row_LIFO_stack_below.pop(); // Remove the old row from below from the LIFO stack
		last_hole_idx = hole_idx; // Set previous (last) hole_idx to the current found one
		hole_idx = find_holes(state, piece, last_hole_idx); // Find holes that can be reached from the hole on the layer above (last_hole_idx) and save them
	} // By the end of this loop there are either no more holes OR we're at the bottom row
	row_LIFO_stack_above_two = copy_top_two_from_LIFO_stack(row_LIFO_stack_above);
	unsigned next_state = crank(state, piece, last_hole_idx); // Use the last_hole_idx to see where the pieces could be coming from
	return next_state;
}

int main(int,char**)
{
	init(); // Initialize all utility values to zero
	int game=0;
	while(game<1<<13) // Play 2^13 games, each consists of 10000 pieces
	{
		//explore = (game%2);  // doing exploration didn't help learning
		srand(174);
		height=0; // This variable keeps track of how heigh the pieces stack up (The game only considers a 2xWIDTH playable game space. If a new piece gets placed in a way that the game can't fit in this 2xWIDTH space and (an) incompleted row(s) get(s) pushed downward, height increases)
		unsigned state =0; // Keeps track of the board state (can take on values from 0 to 2^(2*WIDTH))
		for(int i=0;i<10000;i++) // 10000 pieces get added before the game is over
		{
			unsigned piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
			state = Qlearning_iteration(state,piece); // Do a full Q-learning iteration for the current state and piece. Both the state and Q-table get updated
			//printf("Game: %4d - Iter: %4d - Height: %4d\n",game,i,height);
		}
		empty_stack(row_LIFO_stack_below); // Empty the game LIFO stack
		game++;
		if(0==(game & (game-1)))  // if a power of 2 -> Print the game number + the height of that game (= performance measure)
			printf("%4d %4d %s\n",game,height,(explore)?"learning":"");

	}
	return 0;
}