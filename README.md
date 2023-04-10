# Q-learning-tetris

## TODO (High Prio)

- Understand Q learning as deeply as possible, especially every function in the [general RL methods](./Q-learning.jl).


## TODO (Low-Prio)

- Implement a visualization of the algorithm playing in julia
- Use learned Q-values to play Nicolas' cpp tetris game


## Ideas

- Goal = keep state space as small as possible. How to describe a state? Could take whole board, but this would mean that if 1 block on the bottom row would be different, the state would be different, while it probably doesn't matter much for which action is optimal to take. Most important are the top layers of the stack of pieces on the board. So maybe describe the state as the height of the top piece in each column? This way, if we use an `n x m` playing board, the state could be described with a `1 x m` row vector of heights + an extra integer that indicates which block should be played next. If this turns out to run smooth, we can increase the state space by making the two highest filled blocks in each column visible. In that case, all Q values of the previously trained model can be copied for all childs of that previous model that have the same top layer. Now the Q learning can continue to fine tune between the different cases of this second heighest collection we now take into account.

## Interesting resources

- [Simplified implementation 2x6 with 2x2 pieces](https://melax.github.io/tetris/tetris.html?fbclid=IwAR0ij-SX_MbPs2y9qCsr-IIGOWs0qJ1n0bgo8pS4JO73H_Yu3G6MLckH-qU) Documented code: [here](./tetris_height-2_2x2-pieces.cpp).
