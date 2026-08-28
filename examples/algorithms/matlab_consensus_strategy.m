function output = matlab_consensus_strategy(state)
% MATLAB strategy function example for the external-process adapter.

goal = state.parameters.goal;
commands = repmat(struct("id", "", "left", 0.0, "right", 0.0), 1, numel(state.robots));
for index = 1:numel(state.robots)
    robot = state.robots(index);
    pose = robot.pose;
    desiredHeading = atan2(goal(2) - pose(2), goal(1) - pose(1));
    headingError = atan2(sin(desiredHeading - pose(3)), cos(desiredHeading - pose(3)));
    distance = hypot(goal(1) - pose(1), goal(2) - pose(2));
    forward = min(0.45, max(0.0, distance * 0.01));
    turn = min(0.35, max(-0.35, headingError * 0.35));
    commands(index).id = robot.id;
    commands(index).left = max(-1.0, min(1.0, forward - turn));
    commands(index).right = max(-1.0, min(1.0, forward + turn));
end

output = struct("type", "command", ...
                "sequence", state.sequence, ...
                "commands", commands);
end
