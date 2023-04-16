// Based on: S Melax (https://melax.github.io/tetris/tetris.html)

#include <stack>
#include <queue>
#include <bitset>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <random>

bool DEBUG_MODE = false; // Used to visualize the game

#define WIDTH (6)     // game width (height = 2)
float gamma = 0.80f;  // discount factor
float alpha = 0.02f;  // learning rate

const int NUM_STATES = 1<<(WIDTH+WIDTH)+1;    // Number of states
const int NUM_PIECES = 195+1;    				// Number of pieces
const int NUM_COL = WIDTH-1+1;                // Number of columns
const int NUM_ROTATIONS = 3+1;                // Number of rotations
double EPSILON = 0;                 // Epsilon for epsilon-greedy exploration

std::vector<std::vector<std::vector<std::vector<double>>>> qValues(NUM_STATES,
std::vector<std::vector<std::vector<double>>>(NUM_PIECES,
std::vector<std::vector<double>>(NUM_COL,
std::vector<double>(NUM_ROTATIONS, 0.0))));  // Initialize the Q-values to zeros 

float Q[1<<(WIDTH+WIDTH)];  // utility value -> array of size 2^(2*WIDTH) -> One utility value per possible board state
int   P[1<<(WIDTH+WIDTH)];  // counter (not really needed)
int height =0;
int explore=0;  // flag I used to experiment with exploration
int num_completed_rows_tmp = 0; // Used to track how many rows are completed by playing the piece
unsigned played_piece = 0; // Used in the printGame() function to visualise where the piece has been played

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
		num_completed_rows_tmp += 1;
	}

	if( (t&((1<<WIDTH)-1)) == ((1<<WIDTH)-1)) // Checks if row zero is completely filled
	{
		t>>=WIDTH; // Move all rows above zero down by one (= delete row zero)
		num_completed_rows_tmp += 1;
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

int EpsilonGreedyExplorationMethod(int state, int piece,std::vector<int> cols, std::vector<int> rots) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0, 1);

  assert(!cols.size() == 0);
  
  // Choose the action with the highest Q-value with probability (1-epsilon)
  // Choose a random action with probability epsilon
  int best = 0;
  if (dis(gen) > EPSILON) {
	// greedy action
	for (int i = 0; i < cols.size(); i++) {
		if (DEBUG_MODE) std::cout << "qValues[state = " << state << "][piece = " << piece << "][col = " << cols[i] << "][rot = " << rots[i]<< "] = " << qValues[state][piece][cols[i]][rots[i]] << std::endl;
		if (qValues[state][piece][cols[i]][rots[i]] > qValues[state][piece][cols[best]][rots[best]]) {
			best = i;
		}
	}
  } 
  else 
  {
	// random position and rotation
    best =  dis(gen) * cols.size();
	if (DEBUG_MODE) std::cout << "random action" << std::endl;
  }
  return best;
}

unsigned crank(unsigned state,unsigned piece, unsigned next_piece, unsigned last_hole_idx = (1<<WIDTH)-1)
{
	std::random_device rd;
  	std::mt19937 gen(rd());
  	std::uniform_real_distribution<> dis(0, 1);

	// We have our state (state,piece), now we need to determine our action (position, rotation)
	// With Greedy we'll take the action that maximises the Q-value

	int bestcol;
	int bestrot;

	int above_row_completion ;
	int number_of_completed_rows;

	unsigned close_hole_idx = last_hole_idx | last_hole_idx<<1 | last_hole_idx>>1; // Get all idx in which a new piece can end up
	// find the best next state
	int best = 0;
	int best2 = -9999999;
	unsigned t = 0;
	int loss=9999; // Number of rows added to height when they're 'pushed down' (2xWIDTH board 'moves up')
	int num_rows_from_above=0; // Counter that keeps track of how many rows from above are used by the best solution
	queue<unsigned> row_FIFO_queue_below_best; // FIFO queue that keeps track of the rows beneath the top 2 ones. Stores the FIFO queue for the current best next state
	queue<unsigned> row_FIFO_queue_below_tmp; // FIFO queue that keeps track of the rows beneath the top 2 ones. Stores the FIFO queue for one current next state evaluation
	
	std::vector<int> next_states; // Vector to store the next states
	std::vector<int> cols; // Vector to store the best columns
	std::vector<int> rots; // Vector to store the best rotations
	std::vector<int> above_row_completions; // Vector to store the above row completions
	std::vector<int> num_completed_rows; // Vector to store the number of completed rows
	std::vector<int> losses; // Vector to store the losses of the best next states
	std::vector<queue<unsigned>> row_FIFO_queue_below_bests; // Vector to store the best below FIFO queues
	std::vector<int> num_rows_from_aboves; // Vector to store the number of rows from above used in the best solution

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
		num_completed_rows_tmp = 0; // Set tracker of number of completed rows in this play to zero
		unsigned n = result(state,a,rotate(piece,r)); // Return the game board that would result from placing 'piece' at horizontal position 'a' with rotation 'r'. Can have up to height 4 
		int l=0; // Variable to keep track of how many rows were shifted up (number of extra rows above the normal 2)
		int collision=0; // Bool that keeps track if there's a collision
		int above_row_completion_flag=0; // Will be used to keep track if playing the piece in the best way completes 
		unsigned TMP_BEFORE_STATE_N = n;
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
				collision = 1; // Set collision flag to one
				break; // Break out of this while loop
			}
			n = n | extra_row_one<<WIDTH; // Update n according to the previous row above
			if( (n&(((1<<WIDTH)-1)<<WIDTH)) == (((1<<WIDTH)-1)<<WIDTH)) // Checks if row one is completely filled
			{
				assert((!(n>>(2*WIDTH)))); // Can't be any row even higher if it just completed the row above
				// printf("Tmp above stack size: %1d - l: %1d\n", row_LIFO_stack_above_two_tmp.size(), l);
				// printf("State: %3d - Piece: %3d - a: %1d - r: %1d - Result state: %5d - New state: %5d\n",state,piece,a,r,TMP_BEFORE_STATE_N,n);
				// exit(-1);
				above_row_completion_flag += 1; // Increase the counter that keeps track of how many rows were shifted up (number of extra rows above the normal 2)
				extra_row_one = 0;
				if(row_LIFO_stack_above_two_tmp.size()) // If there are entries in the LIFO above tmp stack
				{
					extra_row_one = row_LIFO_stack_above_two_tmp.top(); // Extract the extra saved blocks on row one that used to be above the current frame in 'state'
					row_LIFO_stack_above_two_tmp.pop();
				}
				n = (n & ((1<<WIDTH)-1)) | extra_row_one<<WIDTH; // Make n equal to row zero of n + the extra_row_one on row one
			}
		}
		// if((l>0 && num_completed_rows_tmp>0))
		// {
		// 	printf("State: %3d - Piece: %3d - n_comp: %1d - a: %1d - r: %1d - Result state: %5d - New state: %5d\n",state,piece,num_completed_rows_tmp,a,r,TMP_BEFORE_STATE_N,n);
		// }
		assert((!(l>0 && num_completed_rows_tmp>0))); // There can never have been an overflow to rows above the 2xWIDTH playable region if there were also rows completed
		if(num_completed_rows_tmp == 1) // If one row has been completed the compatibility of the move of the piece needs to be checked with the row above and that row above needs to be moved out of memory into the state first row
		{
			unsigned extra_row_one = 0;
			if(row_LIFO_stack_above_two_tmp.size()) // If there are entries in the LIFO above tmp stack
			{
				extra_row_one = row_LIFO_stack_above_two_tmp.top(); // Extract the extra saved blocks on row one that used to be above the current frame in 'state'
				row_LIFO_stack_above_two_tmp.pop();
			}
			if(extra_row_one & n>>WIDTH) // If there's a collision
			{
				collision = 1; // Set collision flag to one
			}
			n = n | extra_row_one<<WIDTH; // Update n according to the previous row above
			if( (n&(((1<<WIDTH)-1)<<WIDTH)) == (((1<<WIDTH)-1)<<WIDTH)) // Checks if row one is completely filled
			{
				extra_row_one = 0;
				if(row_LIFO_stack_above_two_tmp.size()) // If there are entries in the LIFO above tmp stack
				{
					extra_row_one = row_LIFO_stack_above_two_tmp.top(); // Extract the extra saved blocks on row one that used to be above the current frame in 'state'
					row_LIFO_stack_above_two_tmp.pop();
					num_completed_rows_tmp += 1;
				}
				n = (n & ((1<<WIDTH)-1)) | extra_row_one<<WIDTH; // Make n equal to row zero of n + the extra_row_one on row one
			}
		} else if(num_completed_rows_tmp == 2)
		{
			//printf("State: %3d - Piece: %3d - a: %1d - r: %1d - New state: %3d\n",state,piece,a,r,n);
			assert(n == 0); // If the number of completed rows equals two, the current game state should be empty
			if(row_LIFO_stack_above_two_tmp.size()) // If there are entries in the LIFO above tmp stack
			{
				n = n | row_LIFO_stack_above_two_tmp.top(); // Move the top row on the above LIFO stack to row zero of the state
				row_LIFO_stack_above_two_tmp.pop();
			}
			if(row_LIFO_stack_above_two_tmp.size()) // If there are entries in the LIFO above tmp stack
			{
				n = n | row_LIFO_stack_above_two_tmp.top()<<WIDTH; // Move the top row on the above LIFO stack to row one of the state
				row_LIFO_stack_above_two_tmp.pop();
			}
		}
		if(collision) // If there's a collision
		{
			continue; // Go to the next iteration (not even consider it as a next state)
		}
		assert(n<(1<< 2*WIDTH)); // Throws an error if there's still part of a piece above row one, which should normally not be the case anymore
		// if((l<loss)||(l==loss && loss*-100 + gamma * Q[n] > best) ){

		// save the actions and the next state

		next_states.push_back(n); // Vector to store the next states
		cols.push_back(a); // Vector to store the best columns
		rots.push_back(r); // Vector to store the best rotations
		above_row_completions.push_back(above_row_completion_flag); // Vector to store the above row completions
		num_completed_rows.push_back(num_completed_rows_tmp); // Vector to store the number of completed rows
		losses.push_back(l); // Vector to store the losses of the best next states
		row_FIFO_queue_below_bests.push_back(duplicateQueue(row_FIFO_queue_below_tmp)); // Vector to store the best below FIFO queues
		num_rows_from_aboves.push_back(row_LIFO_stack_above_two_tmp.size()); // Vector to store the number of rows from above used in the best solution
		
		if (DEBUG_MODE) std::cout << "first qValues[state = " << state << "][piece = " << piece << "][col = " << a << "][rot = " << r << "] = " << qValues[state][piece][a][r] << std::endl;
		/*
		if(qValues[state][piece][a][r] > best2) // If the discount factor times the Q value of this new position (score for how good this new position is) MINOUS a punishment for the number of rows lost (l*-100) is bigger than the current best score for a next state
		{
			// Save the current best next state
			t=n;
			bestcol = a;
			bestrot = r;
			above_row_completion = above_row_completion_flag;
			number_of_completed_rows = num_completed_rows_tmp;
			loss = l; // Save the loss of the current best next state
			best2 = qValues[state][piece][a][r]; // Update the current best score for a next state
			row_FIFO_queue_below_best = duplicateQueue(row_FIFO_queue_below_tmp); // Save a duplicate of this move's below FIFO queue
			num_rows_from_above = row_LIFO_stack_above_two_tmp.size(); // Keep track of how many rows from above are used in the best solution to later delete them from the real above LIFO stack
			played_piece = rotate(piece,r)<<a; // Save where the current piece is played
		}
		*/
	  }
	} 

	if (next_states.size() == 0) // If there are no next states 
	{
		if (DEBUG_MODE) std::cout << "No next states" << std::endl;
		return state; // Go to the next iteration (not even consider it as a next state)
	}

	best = EpsilonGreedyExplorationMethod(state,  piece, cols, rots); // get the action with the exploration function

	t = next_states[best]; 
	bestcol = cols[best]; 
	bestrot = rots[best];
	above_row_completion = above_row_completions[best];
	number_of_completed_rows = num_completed_rows[best];
	loss = losses[best]; // Save the loss of the current best next state
	row_FIFO_queue_below_best = row_FIFO_queue_below_bests[best]; // Save a duplicate of this move's below FIFO queue
	num_rows_from_above = num_rows_from_aboves[best]; // Keep track of how many rows from above are used in the best solution to later delete them from the real above LIFO stack
	played_piece = rotate(piece,bestrot)<<bestcol; // Save where the current piece is played


	if (DEBUG_MODE) std::cout<< "best col is " << bestcol << " and best rot is " << bestrot <<std::endl; 

	while(row_FIFO_queue_below_best.size()) // While there are entries in the below_best FIFO queue
	{
		row_LIFO_stack_below.push(row_FIFO_queue_below_best.front()); // Add them to the main below LIFO stack
		row_FIFO_queue_below_best.pop(); // Remove them from the temporary FIFO queue
	}
	for (int l=0; l<(row_LIFO_stack_above_two.size() - num_rows_from_above); l++) // Loop for num_rows_from_above number of times
	{
		if(row_LIFO_stack_above.size()) // If there are entries in the LIFO above stack
		{
			row_LIFO_stack_above.pop(); // Remove these rows from the real LIFO above stack because they are already used in the current 'state'
		}
	}

	//get reward from reward function
	double reward = loss*-100 + (number_of_completed_rows+above_row_completion)*500;
	if (DEBUG_MODE) std::cout<< "reward is " << reward <<std::endl;

	//find action maximising the q_value of the new state
	
	double maxNextQValue = -9999999999999;
	for (int i = 0; i < NUM_COL; i++) {
	for (int j = 0; j < NUM_ROTATIONS; j++) {
		if (qValues[state][next_piece][i][j] > maxNextQValue) {
			maxNextQValue = qValues[t][next_piece][i][j];
		}
	}
	}
	qValues[state][piece][bestcol][bestrot] += alpha * (reward + gamma * maxNextQValue - qValues[state][piece][bestcol][bestrot]);
	if (DEBUG_MODE) std::cout << "qValues[state = " << state << "][piece = " << piece << "][bestcol = " << bestcol << "][bestrot = " << bestrot << "] = " << qValues[state][piece][bestcol][bestrot] << std::endl;

	P[state] ++; // counter (not used)

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

	height = row_LIFO_stack_below.size() + ((state & ((1<<WIDTH)-1)) > 0) + (state > ((1<<WIDTH)-1)); // Set game height to the current LIFO stack below size + the height of the state
	
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

unsigned Qlearning_iteration(unsigned state, unsigned piece, unsigned next_piece, unsigned last_hole_idx = (1<<WIDTH)-1)
{
	unsigned hole_idx = find_holes(state, piece, last_hole_idx); // Find holes that can be reached from the hole on the layer above (last_hole_idx) and save them
	while(hole_idx && row_LIFO_stack_below.size()) // While there are still holes present && there are saved rows below the current 2xWIDTH frame 'state'
	{
		unsigned top_row = state>>WIDTH; // Extract the top row
		row_LIFO_stack_above.push(top_row); // Push the top row to the above LIFO stack
		state = (state<<WIDTH & ((1<<WIDTH+WIDTH)-1)); // Move state one row up and forget the top row
		state = state | row_LIFO_stack_below.top(); // Add old row from below off the below LIFO stack
		row_LIFO_stack_below.pop(); // Remove the old row from below from the LIFO stack
		last_hole_idx = hole_idx; // Set previous (last) hole_idx to the current found one
		hole_idx = find_holes(state, piece, last_hole_idx); // Find holes that can be reached from the hole on the layer above (last_hole_idx) and save them
	} // By the end of this loop there are either no more holes OR we're at the bottom row
	row_LIFO_stack_above_two = copy_top_two_from_LIFO_stack(row_LIFO_stack_above);
	unsigned next_state = crank(state, piece, next_piece, last_hole_idx); // Use the last_hole_idx to see where the pieces could be coming from
	return next_state;
}

void printRow(unsigned row) // Prints out one row to stdout
{
	bitset<WIDTH> bits(row);
	for (int i = WIDTH - 1; i >= 0; i--) {
		char* block = (char*)"  ";
		if (bits[i])
		{
			block = (char*)"[]";
		}
		std::cout << block;
	}
	std::cout << '\n';
	return;
}

void printPiece(unsigned piece)
{
	unsigned top_row = piece>>WIDTH; // Extract the top row
	unsigned bottom_row = piece & ((1<<WIDTH)-1); // Extract the bottom row
	printRow(top_row);
	printRow(bottom_row);
}

void printGame(unsigned state, bool clearscreen = false, unsigned height = 999999) // Prints out the game to the terminal
{
	assert(height >= 2); // At least the state should be printed
	assert((!row_LIFO_stack_above.size())); // There may not be any rows stored above when printing the game
	if (clearscreen)
	{
		system("cls"); // Clear screen
	}
	printPiece(played_piece);
	std::cout << "------------\n"; // Print separator between piece and game
	printPiece(state);
	stack<unsigned> temp_stack;
    while (height-2 > 0 && !row_LIFO_stack_below.empty()) {
        unsigned row = row_LIFO_stack_below.top();
        row_LIFO_stack_below.pop();
        temp_stack.push(row);
        printRow(row);
        height--;
    }
    while (!temp_stack.empty()) {
        row_LIFO_stack_below.push(temp_stack.top());
        temp_stack.pop();
    }
	return;
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
		unsigned piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
		for(int i=0;i<1000;i++) // 10000 pieces get added before the game is over
		{
			unsigned next_piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
			state = Qlearning_iteration(state,piece,next_piece); // Do a full Q-learning iteration for the current state and piece. Both the state and Q-table get updated
			if (DEBUG_MODE)
			{
				printf("Game: %4d - Iter: %4d - Height: %4d\n",game,i,height); // Print game info
				printGame(state); // Print game
				while (std::cin.get() != '\n'); // Wait for enter press
			}
			piece = next_piece;
		}
		empty_stack(row_LIFO_stack_below); // Empty the game LIFO stack
		game++;
		 // if a power of 2 -> Print the game number + the height of that game (= performance measure)
		if(0==(game & (game-1))){
			// number of calculated q values
			int number_of_calculated_q_values = 0;
			for (int i = 0; i < NUM_STATES; i++)
			{
				for (int j = 0; j < NUM_PIECES; j++)
				{
					for (int k = 0; k < NUM_COL; k++)
					{
						for (int l = 0; l < NUM_ROTATIONS; l++)
						{
							if (qValues[i][j][k][l] != 0) number_of_calculated_q_values++;
						}
					}
				}
			}
			printf("%4d %4d %4f %4d\n",game,height,EPSILON, number_of_calculated_q_values);
		}


		// Decay epsilon
		if (EPSILON > 0.001) EPSILON *= 0.99; else EPSILON = 0;

		if (game == 1024)
		{
			//DEBUG_MODE = true;
		}
	}
	return 0;
}