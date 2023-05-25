// Inspired by: S Melax (https://melax.github.io/tetris/tetris.html)

#include <stack>
#include <queue>
#include <bitset>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <random>
#include <fstream>
#include <string>

bool DEBUG_MODE = false; // Used to visualize the game & show debug info on each piece-playing iteration
const bool FIXED_HEIGHT_TEST = false; // Plays an infinite amount of pieces with a maximum game board height every 'power of two'-th game (could be used as a performance metric)
const bool log_height_data = false; // Used to log the data to a file for post-processing
const int n_games = 11; // Number of games to play (2^n_games)

#define MAX_HEIGHT (10) // Max height used in a FIXED_HEIGHT_TEST
#define WIDTH (6)		// Game width (height of state = 2)

const float gamma = 0.75f;			// Discount factor
const float alpha = 0.15f;			// Learning rate
const bool EPSILON_DECAY = false;	// Indicates if EPSILON should decay over time
double EPSILON = 0;		     		// Epsilon for epsilon-greedy exploration

// Reward function weights
const int kloss = -100;     // Reward weight for Number of rows added to height when they're 'pushed down') 
const int kcomb = 600;      // Reward weight for Number of rows completed
const int kdens = 0;        // Reward weight for Number of 'holes'
const int kbump = 0;        // Reward weight for Number of 'bumps' (number of blocks that are not on the bottom layer and have a block below them)

/*
const int NUM_STATES = (1<<(WIDTH+WIDTH))-(1<<(WIDTH))-1;	// Number of states (State represented by largest unsigned has the following bits: 111110111110)
const int NUM_PIECES = 3<<WIDTH + 3;						// Number of pieces (Piece represented by largest unsigned has the following bits: 11000011)
const int NUM_COL = WIDTH-1;								// Number of columns (1 less than WIDTH, since the pieces are 2 blocks wide)
const int NUM_ROTATIONS = 4;								// Number of rotations
*/

const int NUM_STATES = 1<<(WIDTH+WIDTH)+1;	// Number of states
const int NUM_PIECES = 195+1;				// Number of pieces
const int NUM_COL = WIDTH-1+1;				// Number of columns
const int NUM_ROTATIONS = 3+1;				// Number of rotations

std::vector<std::vector<std::vector<std::vector<double>>>> qValues(NUM_STATES,
std::vector<std::vector<std::vector<double>>>(NUM_PIECES,
std::vector<std::vector<double>>(NUM_COL,
std::vector<double>(NUM_ROTATIONS, 0.0))));  // Initialize the Q-values to zeros 

std::vector<std::vector<std::vector<std::vector<long>>>> N(NUM_STATES,
std::vector<std::vector<std::vector<long>>>(NUM_PIECES,
std::vector<std::vector<long>>(NUM_COL,
std::vector<long>(NUM_ROTATIONS, 0))));  // Initialize N to zeros; keeps track of amount of times a state action pair has been explored; used by the SimpleExplorationMethod

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

unsigned take_single_action(unsigned state, unsigned action, unsigned piece, int &num_completed_rows_tmp) 
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

// Identity exploration method. The agent will always take the action that maximizes the Q-value.
int IdentityExplorationMethod(int state, int piece,std::vector<int> cols, std::vector<int> rots)
{
	assert(!cols.size() == 0);
	int best = 0;
	// Look for action that maximizes the Q-value
	for (int i = 0; i < cols.size(); i++) {
		//if (DEBUG_MODE) std::cout << "qValues[state = " << state << "][piece = " << piece << "][col = " << cols[i] << "][rot = " << rots[i]<< "] = " << qValues[state][piece][cols[i]][rots[i]] << std::endl;
		if (qValues[state][piece][cols[i]][rots[i]] > qValues[state][piece][cols[best]][rots[best]]) {
			best = i;
		}
	}
	return best;
}

// Random exploration method. The agent will take random actions
int RandomExplorationMethod(int state, int piece,std::vector<int> cols, std::vector<int> rots)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0, 1);

	assert(!cols.size() == 0);

	// Choose a random action with uniform distribution
	int random_choice = dis(gen) * cols.size();
	return random_choice;
}

// Epsilon-greedy exploration method. With probability `epsilon`, the agent will explore, otherwise it will exploit.
int EpsilonGreedyExplorationMethod(int state, int piece,std::vector<int> cols, std::vector<int> rots, double epsilon = EPSILON) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0, 1);

  assert(!cols.size() == 0);
  
  // Choose the action with the highest Q-value with probability (1-epsilon)
  // Choose a random action with probability epsilon
  int best = 0;
  if (dis(gen) > epsilon) {
	// greedy action
	for (int i = 0; i < cols.size(); i++) {
		//if (DEBUG_MODE) std::cout << "qValues[state = " << state << "][piece = " << piece << "][col = " << cols[i] << "][rot = " << rots[i]<< "] = " << qValues[state][piece][cols[i]][rots[i]] << std::endl;
		if (qValues[state][piece][cols[i]][rots[i]] > qValues[state][piece][cols[best]][rots[best]]) {
			best = i;
		}
	}
  } 
  else 
  {
	// random position and rotation
    best =  dis(gen) * cols.size();
	//if (DEBUG_MODE) std::cout << "random action" << std::endl;
  }
  return best;
}

// Simple exploration method. The agent will receive a value of `R_plus` if it has seen the state-action pair less than `N_max` times, otherwise it will receive a reward equal to the Q-value.
int SimpleExplorationMethod(int state, int piece,std::vector<int> cols, std::vector<int> rots, int N_max = 10, double R_plus = 20.0)
{
	assert(!cols.size() == 0);
	int best = 0;
	// Look for action that maximizes the Q-value
	for (int i = 0; i < cols.size(); i++) {
		N[state][piece][cols[i]][rots[i]]++;
	}
	// Argmax
	for (int i = 0; i < cols.size(); i++) {
		double weight = 0;
		if (N[state][piece][cols[i]][rots[i]] < N_max) {  // If this state action pair was encountered less than N_max times
			weight = R_plus;
		}
		else { // Else set the weight to the qValue of this state action pair
			weight = qValues[state][piece][cols[i]][rots[i]];
		}
		if (weight > qValues[state][piece][cols[best]][rots[best]]) {
			best = i;
		}
	}
	return best;
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

void count_holes_and_set_max_height(unsigned &num_holes, vector<unsigned> &max_height, unsigned row, unsigned row_count) {
    for (int i = 0; i < WIDTH; i++) {
        if ((row & (1 << i)) == 0) {
            num_holes++;
        } else {
            max_height[i] = row_count;
        }
    }
	return;
}

double variance(vector<unsigned> max_height) {
    double sum = 0;
    double mean = 0;
    for (int i = 0; i < max_height.size(); i++) {
        sum += max_height[i];
    }
    mean = sum / max_height.size();
    double variance = 0;
    for (int i = 0; i < max_height.size(); i++) {
        variance += pow(max_height[i] - mean, 2);
    }
    variance /= max_height.size();
    return variance;
}

void calc_density_and_bumpiness(double &game_density, double &bumpiness, unsigned state)
{
	assert((!row_LIFO_stack_above.size())); // There may not be any rows stored above before performing this calculation
	
	// Initialize output values
	game_density = 0.0;
	bumpiness = 0;

	// Move all the way down to the bottom of the game
	while (row_LIFO_stack_below.size()) // While there are entries in the below LIFO stack
	{
		unsigned top_row = state>>WIDTH; // Extract the top row
		row_LIFO_stack_above.push(top_row); // Add the top row of the state to the main above LIFO stack
		state<<=WIDTH; // Shift the state one row up
		state = state & ((1<<(WIDTH+WIDTH))-1); // Throw away the upper row
		unsigned new_bottom_row = row_LIFO_stack_below.top(); // Extract a new bottom row for the state from the LIFO below stack
		row_LIFO_stack_below.pop(); // Remove it from the below LIFO stack
		state = state | new_bottom_row; // Add the new bottom row to the state
	}

	// Initialize counting variables
	unsigned num_holes = 0; // Initialize num_holes counter to zero
	std::vector<unsigned> max_height(WIDTH, 0); // Initialize vector that keeps track of the highest block in each column to zero
	unsigned row_count = 1; // Blocks on the bottom row have height one (no block on bottom row -> Height zero)

	// Perform first count for the very bottom row
	unsigned bottom_row = state & ((1<<WIDTH)-1); // Extract the bottom row
	count_holes_and_set_max_height(num_holes, max_height, bottom_row, row_count);

	// Move all the way back up to the current game state
	while (row_LIFO_stack_above.size()) // While there are entries in the above LIFO stack
	{
		unsigned bottom_row = state & ((1<<WIDTH)-1); // Extract the bottom row
		row_LIFO_stack_below.push(bottom_row); // Add the bottom row of the state to the main below LIFO stack
		state>>=WIDTH; // Shift the state one row down
		unsigned new_bottom_row = state; // Extract the new bottom row to use for calculating density and bumpiness
		row_count++; // Increase the row count by one
		count_holes_and_set_max_height(num_holes, max_height, new_bottom_row, row_count);
		unsigned new_top_row = row_LIFO_stack_above.top(); // Extract a new top row for the state from the LIFO above stack
		row_LIFO_stack_above.pop(); // Remove it from the above LIFO stack
		state = state | new_top_row<<WIDTH; // Add the new top row to the state
	}

	// Still need to keep counting for the two layers of the state
	if (state == 0) { // Check if bottom row of state is empty
		row_count--; // Correct for too high initialized value
	} else {
		if ((state>>WIDTH)) { // Check if top row of state is not empty
			// Count the top row layer
			row_count++; // Increase the row count by one
			unsigned top_row = state>>WIDTH; // Extract the top row
			count_holes_and_set_max_height(num_holes, max_height, top_row, row_count);
		}
	}

	// Calculate game_density using num_holes
	double game_area = row_count * WIDTH;
	game_density = (game_area - num_holes) / game_area;

	// Calculate bumpiness using max_height
	//for (int i = 1; i < max_height.size(); i++) { bumpiness += abs(static_cast<int>(max_height[i] - max_height[i-1])); }
	bumpiness = variance(max_height);

	return;
}

unsigned learn(unsigned state,unsigned piece, unsigned next_piece, unsigned &played_piece, int &height, unsigned last_hole_idx = (1<<WIDTH)-1)
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
		//if (DEBUG_MODE) printf("a: %1d - Cannot be reached", a);
		continue; // Move to the next pos
	  }
	  for(int r=0;r<4;r++) // Loop over all possible rotations r of the piece
	  {
		empty_queue(row_FIFO_queue_below_tmp); // Empty the tmp FIFO queue that keeps track of the rows beneath the top 2 ones for this current next state evaluation
		stack<unsigned> row_LIFO_stack_above_two_tmp; // LIFO stack that keeps track of the rows above the current inspected 2xWIDTH frame and gets used in the crank function
		row_LIFO_stack_above_two_tmp = duplicateStack(row_LIFO_stack_above_two); // Fill the tmp LIFO stack that keeps track of the two rows above the current 2xWIDTH frame in 'state'
		int num_completed_rows_tmp = 0; // Used to track how many rows are completed by playing the piece
		unsigned n = take_single_action(state,a,rotate(piece,r),num_completed_rows_tmp); // Return the game board that would result from placing 'piece' at horizontal position 'a' with rotation 'r'. Can have up to height 4 
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
				//if (DEBUG_MODE) { printf("a: %1d - r: %1d - Collision with row above!\n", a, r); }
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
				//if (DEBUG_MODE) printf("a: %1d - r: %1d - Collision with row above! (2)", a, r);
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
		
		//if (DEBUG_MODE) std::cout << "first qValues[state = " << state << "][piece = " << piece << "][col = " << a << "][rot = " << r << "] = " << qValues[state][piece][a][r] << std::endl;
		
	  }
	}

	assert(next_states.size()); // There should be next states

	best = EpsilonGreedyExplorationMethod(state,  piece, cols, rots, EPSILON); // get the action with the exploration function

	unsigned prev_state = state; // Save previous state for q value update

	state = next_states[best]; // Move to the new state
	bestcol = cols[best]; // Horizontal offset in which the piece is played
	bestrot = rots[best]; // Rotation of the played piece
	above_row_completion = above_row_completions[best]; // Number of rows above the state completed by playing the piece
	number_of_completed_rows = num_completed_rows[best]; // Total number of rows completed by playing the piece
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

	// Compute features usable in a reward function
	double game_density = 0.0; // The higher the density, the less holes
	double bumpiness = 0 ; // Variance of heights of all neighboring top blocks of each row
	// if kdens and kbump are not 0, calculate game_density and bumpiness
	if (kdens != 0 || kbump != 0)
	{
		calc_density_and_bumpiness(game_density, bumpiness, state); // Calculate these parameters for the newly obtained state
		if (DEBUG_MODE) std::cout << "game_density = " << game_density << " & bumpiness = " << bumpiness << std::endl;
	}
	


	// Get reward from reward function
	double reward = loss*kloss + (number_of_completed_rows+above_row_completion)*kcomb + game_density*kdens + bumpiness* kbump;  // Reward function can be optimized by hyperparameter tuning (weights: kloss, kcomb, kdens, kbump)
	if (DEBUG_MODE) std::cout << "reward is " << reward << std::endl;

	// Find action maximizing the q_value of the new state
	double maxNextQValue = -9999999999999;
	for (int i = 0; i < NUM_COL; i++) {
		for (int j = 0; j < NUM_ROTATIONS; j++) {
			if (qValues[state][next_piece][i][j] > maxNextQValue) {
				maxNextQValue = qValues[state][next_piece][i][j];
			}
		}
	}

	if (DEBUG_MODE) std::cout << "maxNextQValue = " << maxNextQValue << std::endl;

	if (DEBUG_MODE) std::cout << "old qValue[state = " << prev_state << "][piece = " << piece << "][bestcol = " << bestcol << "][bestrot = " << bestrot << "] = " << qValues[prev_state][piece][bestcol][bestrot] << std::endl;

	qValues[prev_state][piece][bestcol][bestrot] += alpha * (reward + gamma * maxNextQValue - qValues[prev_state][piece][bestcol][bestrot]);

	if (DEBUG_MODE) std::cout << "new qValue[state = " << prev_state << "][piece = " << piece << "][bestcol = " << bestcol << "][bestrot = " << bestrot << "] = " << qValues[prev_state][piece][bestcol][bestrot] << std::endl;
	
	return state; // Return the new state
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

unsigned Qlearning_iteration(unsigned state, unsigned piece, unsigned next_piece, unsigned &played_piece, int &height, unsigned last_hole_idx = (1<<WIDTH)-1)
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
	unsigned next_state = learn(state, piece, next_piece, played_piece, height, last_hole_idx); // Use the last_hole_idx to see where the pieces could be coming from
	return next_state;
}

void printGame(unsigned state, unsigned played_piece, unsigned print_height = 999999) // Prints out the game to the terminal
{
	assert(print_height >= 2); // At least the state should be printed
	assert((!row_LIFO_stack_above.size())); // There may not be any rows stored above when printing the game
	printPiece(played_piece);
	std::cout << "------------\n"; // Print separator between piece and game
	printPiece(state);
	stack<unsigned> temp_stack;
    while (print_height-2 > 0 && !row_LIFO_stack_below.empty()) {
        unsigned row = row_LIFO_stack_below.top();
        row_LIFO_stack_below.pop();
        temp_stack.push(row);
        printRow(row);
        print_height--;
    }
    while (!temp_stack.empty()) {
        row_LIFO_stack_below.push(temp_stack.top());
        temp_stack.pop();
    }
	return;
}

void debugModeMenu(bool &pressed_g, bool &pressed_2)
{
	char c;
	while ((c = std::cin.get()) != '\n') { // Wait for enter press -> Play next piece
		if (c == 'g') {
			std::cout << "[*] Entered 'g'. Continuing outside DEBUG_MODE until the next game." << std::endl;
			pressed_g = true; // Remember this for the next game
			DEBUG_MODE = false;
			break;
		} else if (c == '2') {
			std::cout << "[*] Entered '2'. Continuing outside DEBUG_MODE until the next power of 2 game." << std::endl;
			pressed_2 = true; // Remember this for the next power of 2 game
			DEBUG_MODE = false;
			break;
		} else if (c == 'f') {
			std::cout << "[*] Entered 'f'. Finishing all games." << std::endl;
			DEBUG_MODE = false;
			break;
		}
	}
	return;
}

int main(int,char**)
{		
	int game = 0;
	std::ofstream log_file;
	std::ofstream log_heights_file;
    //std::string filename = "log_gamma_="+ std::to_string(gamma) + "_alpha_=" + std::to_string(alpha) + "_kloss_=" + std::to_string(kloss) + "_kcomb_=" + std::to_string(kcomb) + "_kdens_=" + std::to_string(kdens) + "_kbump_=" + std::to_string(kbump) + ".txt";
	double first_epsilon = EPSILON; 
	std::string filename = "log_epsilon_="+ std::to_string(first_epsilon) + ".txt";
    log_file.open(filename);
    log_heights_file.open("log_heights.txt");

	//log_file << "gamma = " << gamma << " alpha = " << alpha << " kloss = " << kloss << " kcomb = " << kcomb << " kdens = " << kdens << " kbump = " << kbump << std::endl;

	bool debug_mode_set = DEBUG_MODE;
	bool pressed_g = false;
	bool pressed_2 = false;

	if (FIXED_HEIGHT_TEST) // Plays an infinite amount of pieces with a maximum game board height
		std::cout <<"\ngame | height | average_height | epsilon | number of calculated q values | #pieces played in fixed height game" << std::endl;
	else {
		std::cout <<"\ngame | height | average_height | epsilon | number of calculated q values" << std::endl;
	}

	while(game<1<<n_games) // Play 2^13 games, each consists of 10000 pieces
	{
		srand(0);
		int height = 0; // This variable keeps track of how heigh the pieces stack up (The game only considers a 2xWIDTH playable game space. If a new piece gets placed in a way that the game can't fit in this 2xWIDTH space and (an) incompleted row(s) get(s) pushed downward, height increases)
		int sum_height = 0; // This variable keeps track of the sum of the heights of the game to calculate the average height
		unsigned state =0; // Keeps track of the board state (can take on values from 0 to 2^(2*WIDTH))
		unsigned piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
		for(int i=0;i<1000;i++) // 10000 pieces get added before the game is over
		{
			unsigned next_piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
			//if (DEBUG_MODE) {printf("Next piece:\n"); printPiece(next_piece);}
			unsigned played_piece = next_piece; // Will be changed to the played piece in the Qlearning_iteration(). Used in the printGame() function.
			state = Qlearning_iteration(state,piece,next_piece,played_piece,height); // Do a full Q-learning iteration for the current state and piece. Both the state and Q-table get updated, as well as the played_piece and the height of the game
			sum_height += height; // Add the height of the game to the sum of heights
			if(log_height_data) log_heights_file << height << " "; // Log the game height
			if (DEBUG_MODE)
			{
				printf("Game: %4d - Iter: %4d - Height: %4d\n",game,i,height); // Print game info
				printGame(state, played_piece); // Print game
				debugModeMenu(pressed_g, pressed_2);
			}
			piece = next_piece;
		}
		empty_stack(row_LIFO_stack_below); // Empty the game LIFO stack
		game++;
		if(log_height_data) log_heights_file << std::endl;
		double average_height = sum_height/double(1000); // Calculate the average height of the game
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

			if (debug_mode_set && pressed_2) {
				DEBUG_MODE = true; // Set DEBUG_MODE back to true at the beginning of a game after a power of 2
				pressed_2 = false; // Forget 2 had been pressed for the next power of 2 game
			}

			int prev_height = height; // Save height of 2^n game for logging after FIXED_HEIGHT_TEST

			if (FIXED_HEIGHT_TEST) // Plays an infinite amount of pieces with a maximum game board height
			{
				srand(0);
				int i = 0; // Keeps track of how many pieces were played
				int height = 0; // This variable keeps track of how heigh the pieces stack up
				unsigned state = 0; // Keeps track of the board state (can take on values from 0 to 2^(2*WIDTH))
				unsigned piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
				while (height < MAX_HEIGHT && i < 1<<20) // Stop after about a million pieces
				{
					unsigned next_piece = ((rand()%4)<<WIDTH) +  (rand()%3)+1; // Each piece consisits of a (WIDTH+2)-bit number.The (WIDTH+2) and (WIDTH+1) bits represent the top 2 blocks of the 2x2 piece, the 1st and 2nd bits represent the lower 2 blocks of the 2x2 piece
					unsigned played_piece = next_piece; // Will be changed to the played piece in the Qlearning_iteration(). Used in the printGame() function.
					state = Qlearning_iteration(state,piece,next_piece,played_piece,height); // Do a full Q-learning iteration for the current state and piece. Both the state and Q-table get updated, as well as the played_piece and the height of the game
					i++; // One more piece played
					if (DEBUG_MODE)
					{
						printf("[FIXED_HEIGHT_TEST] Game: %4d - Iter: %4d - Height: %4d\n",game,i,height); // Print game info
						printGame(state, played_piece); // Print game
						debugModeMenu(pressed_g, pressed_2);
					}
					piece = next_piece;
				}
				printf("%4d |  %4d  |     %6.2f     |  %1.4f |           %6d              |             %7d\n", game, prev_height, average_height, EPSILON, number_of_calculated_q_values, i);
				log_file << game << " " << height << " " << EPSILON << " " << number_of_calculated_q_values << " " << average_height << " " << i << std::endl;
			}
			else
			{
				printf("%4d |  %4d  |     %6.2f     |  %1.4f |           %6d\n", game, height, average_height, EPSILON, number_of_calculated_q_values);
				log_file << game << " " << height << " " << EPSILON << " " << number_of_calculated_q_values << " " << average_height << std::endl;
			}
		}
		if (debug_mode_set && pressed_g) {
			DEBUG_MODE = true; // Set DEBUG_MODE back to true at the beginning of a game after a power of 2
			pressed_g = false; // Forget g had been pressed for the next game
		}
		// Decay epsilon
		if (EPSILON_DECAY) {
			if (EPSILON > 0.001) EPSILON *= 0.99; else EPSILON = 0;  // >>>>>>>>>>>>>>>>>>>>>>>> TODO <<<<<<>>>>>> Implement more epsilon decay functions <<<<<<<<<<<<<<<<<<<<<<<<
		}
	}
	log_file.close();
	log_heights_file.close();
	return 0;
}