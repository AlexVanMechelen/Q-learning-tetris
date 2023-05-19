# Project Reinforcement Learning - Tetris

This project expands on [this](https://melax.github.io/tetris/tetris.html) project by S. Melax. We documented their original code [here](./tetris_height-2_2x2-pieces.cpp).

## How to run?

The [Q learning implementation](./tetris-Qlearning.cpp) is done in the c++ programming language. To execute the program it needs to be compiled. Various compilers exist, one example is g++.
The program can be compiled with the following command:
```
g++ -o run.exe tetris-Qlearning.cpp
```
It can now be executed by calling `./run.exe`

## Results

Various experiments have been performed like hyperparameter tuning, playing with the cost function and exploration method. The results of these experiments are processed in a [Matlab Live Script](./plots_tetris.mlx) and further discussed in a paper.
