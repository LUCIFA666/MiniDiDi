function next = controller_step(pose, waypoint, dt)
%CONTROLLER_STEP Differential-drive kinematics + proportional heading control.
% Coordinates are meters, yaw radians; grid Y grows downward in the viewer.
error = waypoint - pose(1:2);
distance = norm(error);
next = pose;
if distance < 0.025
    next(1:2) = waypoint;
    return;
end
desired = atan2(error(2), error(1));
headingError = atan2(sin(desired-pose(3)), cos(desired-pose(3)));
omega = max(-2.8, min(2.8, 5.0*headingError));
velocity = 0;
if abs(headingError) < 0.035
    velocity = min([1.4, 2.0*distance, distance/max(dt, 1e-3)]);
end
next(3) = atan2(sin(pose(3)+omega*dt), cos(pose(3)+omega*dt));
next(1) = pose(1) + velocity*cos(next(3))*dt;
next(2) = pose(2) + velocity*sin(next(3))*dt;
end
