function test_controller
% Runs without toolboxes. Invoke test_controller from this folder.
pose = [0 0 0];
waypoints = [1 0;1 1;0 1;0 0];
for k = 1:size(waypoints,1)
    target = waypoints(k,:); start = pose(1:2);
    for iteration = 1:500
        pose = controller_step(pose,target,0.05);
        delta = target-start;
        progress = max(0,min(1,dot(pose(1:2)-start,delta)/dot(delta,delta)));
        assert(norm(pose(1:2)-(start+progress*delta))<0.16, 'Left reserved corridor');
        if norm(pose(1:2)-target)<0.025, break; end
    end
    assert(norm(pose(1:2)-target)<0.025, 'Waypoint not reached');
end
disp('PASS differential-drive controller, four turns and corridor bounds');
end
