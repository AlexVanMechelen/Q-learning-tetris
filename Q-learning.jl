
using StatsBase
using Plots

abstract type AbstractMarkovDecisionProcess end



# ------------------------------------------------------
# General RL methods
# -———————————————--------------------------------------
abstract type ReinforcementLearningMethod end

"""
    Qlearner(mdp::T) where T <: AbstractMarkovDecisionProcess

Q-learning algorithm.

# Parameters
- `mdp`: the underlying Markov Decision Proces, a from of an `AbstractMarkovDecisionProcess`
- `explorer`: the `ExplorationMethod` that will be used to select the next action (exploration should deminish over time, to avoid "trashing")
- `α`: the learning rate for the Q-learning algorithm (should be between 0 an 1 and decrease over time)
- `N_episodes`: the number of episodes to run
- `trial_length`: the maximum number of steps in each episode

# Other fields
- `Q`: the Q-table
- `state`: the current state
- `action`: the current action
- `reward`: the current reward

"""
mutable struct Qlearner{T,M} <: ReinforcementLearningMethod where {T<:AbstractMarkovDecisionProcess,M<:ExplorationMethod}
    mdp::T
    exploration_method::M
    α::Float64
    Q::Dict
    N_episodes::Int64
    trial_length::Int64
    state
    action
    reward
    function Qlearner(mdp::T,   exploration_method::M=IdentityExplorationMethod();
                                α::Float64=0.9,
                                N_episodes::Int64=1,
                                trial_length::Int64=100) where {T<:AbstractMarkovDecisionProcess, M<:ExplorationMethod}
        
        return new{T, M}(mdp, exploration_method, α, Dict(), N_episodes, trial_length, nothing, nothing, nothing)
    end
end
Base.show(io::IO, m::Qlearner) = print(io, "Q-learning algorithm for $(m.mdp)\n (using $(m.exploration_method), α = $(m.α), $(m.N_episodes) episodes, trial length = $(m.trial_length))")

function init!(rlm::Qlearner)
    rlm.state = initial_state(rlm.mdp)
    rlm.action = nothing
    rlm.reward = nothing
end

function single_trial!(rlm::V) where V<:ReinforcementLearningMethod
    # initialize the trial
    init!(rlm) # this will set the state, action and reward to nothing
    trial_length = 0
    mdp = rlm.mdp
    s = rlm.state
    Q = rlm.Q
    f = rlm.exploration_method.exploration_function
    # run the trial
    while !isnothing(s) && (trial_length < rlm.trial_length)
        # select the best action according to the Q-table and the exploration method
        a = f(Q, s, actions(mdp, s))
        # select the best action
        #a = argmax(a -> get!(Q,(s,a), 0.), actions(mdp, s))
        # take the action in the world
        sᶥ = take_single_action(mdp, s, a)
        # get the reward
        r = reward(mdp, s, a, sᶥ)
        # update the Q-table
        learn!(rlm, s, a, sᶥ, r)
        # update the state
        s = sᶥ
        # update the trial length
        trial_length += 1
    end
end

"""
    learn!(rlm::Qlearner, s, a, sᶥ, r)

Update the Q-table using the Q-learning algorithm when going from state `s` to state `sᶥ` by action `a` and receiving reward `rᶥ`.
"""
function learn!(rlm::Qlearner, s, a, sᶥ, r)
    mdp = rlm.mdp
    Q = rlm.Q
    α = rlm.α
    γ = rlm.mdp.γ
    
    # check for terminal state
    if isnothing(sᶥ)
        Q[nothing] = r
    else
        # update the Q-table
        sample = r + γ* maximum( get!(Q, (sᶥ, aᶥ), 0.) for aᶥ in actions(mdp, sᶥ))
        Q[(s,a)] = (1-α) * get!(Q, (s,a), 0.) + α * sample
    end
end

"""
    learn!(rlm::T) where T<:ReinforcementLearningMethod

Learn from the experience by using a specific algorithm.
"""
function learn!(rlm::T) where T<:ReinforcementLearningMethod
    for i in 1:rlm.N_episodes
        single_trial!(rlm)
    end
end

function sample(rlm::T; initial_state=nothing, max_duration::Int=1000) where T<:ReinforcementLearningMethod
    model = rlm.mdp
    if isnothing(initial_state)
        s = RLMethods.initial_state(model)
        #s = initial_state(rlm.mdp)
    else
        s = initial_state
    end
    states = [s]
    duration = 0
    while !isnothing(s) && (duration < max_duration)
        s = states[end]
        a = argmax(a -> get!(rlm.Q, (s,a), 0.), actions(rlm.mdp, s))
        sᶥ = take_single_action(rlm.mdp, s, a)
        if isnothing(sᶥ)
            break
        end
        push!(states, sᶥ)
        duration += 1
    end
    return states
end



