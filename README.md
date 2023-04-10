# Q-learning-tetris

## TODO (High Prio)

- Understand Q learning as deeply as possible, especially every function in the [general RL methods](./Q-learning.jl).


## TODO (Low-Prio)

- Implement a visualization of the algorithm playing in julia
- Use learned Q-values to play Nicolas' cpp tetris game


## Ideas

- Goal = keep state space as small as possible. How to describe a state? Could take whole board, but this would mean that if 1 block on the bottom row would be different, the state would be different, while it probably doesn't matter much for which action is optimal to take. Most important are the top layers of the stack of pieces on the board. So maybe describe the state as the height of the top piece in each column? This way, if we use an `n x m` playing board, the state could be described with a `1 x m` row vector of heights + an extra integer that indicates which block should be played next. If this turns out to run smooth, we can increase the state space by making the two highest filled blocks in each column visible. In that case, all Q values of the previously trained model can be copied for all childs of that previous model that have the same top layer. Now the Q learning can continue to fine tune between the different cases of this second heighest collection we now take into account.

## State space

The state space needs to remain as small as possibe, since we work with a Q table with 1 entry for every possible state. The example by [Melax](https://melax.github.io/tetris/tetris.html?fbclid=IwAR0ij-SX_MbPs2y9qCsr-IIGOWs0qJ1n0bgo8pS4JO73H_Yu3G6MLckH-qU) has a state space of 2^(2\*6) = 4096, since it looks at a 2x6 area and 2x2 pieces. Let's say we want to use the real tetrominos as pieces. If we would consider a board that is only 8 wide (real board is 10 wide) and considering 4 layers of height (since there's a tetromino 4 blocks long and we don't want our algoritm to prefer placing that long block horizontally, just because of the training environment), we would still have a state space of 2^(4\*8) > 4 billion ~ 1 Million \* 4096 (Melax).

## Interesting resources

- [Simplified implementation 2x6 with 2x2 pieces](https://melax.github.io/tetris/tetris.html?fbclid=IwAR0ij-SX_MbPs2y9qCsr-IIGOWs0qJ1n0bgo8pS4JO73H_Yu3G6MLckH-qU) Documented code: [here](./tetris_height-2_2x2-pieces.cpp).
