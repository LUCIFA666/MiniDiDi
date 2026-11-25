function run_twin(baseUrl, maxSeconds)
%RUN_TWIN Independent differential-drive MATLAB car, no extra toolbox required.
% run_twin('http://192.168.1.10:8080')
% Server plans and reserves a safe grid segment. This process integrates the
% vehicle model and publishes its actual continuous pose. The server never
% advances M01 without a measured endpoint acknowledgement.
if nargin < 1, baseUrl = 'http://127.0.0.1:8080'; end
if nargin < 2, maxSeconds = inf; end
baseUrl = char(baseUrl);
options = weboptions('MediaType', 'application/json', ...
    'ContentType', 'json', 'Timeout', 5);
fig = figure('Name', 'MiniDiDi MATLAB digital twin M01', 'Color', [0.96 0.98 0.95]);
ax = axes(fig); hold(ax, 'on'); axis(ax, 'equal'); grid(ax, 'on');
xlabel(ax, 'East / m'); ylabel(ax, 'Grid Y / m');
title(ax, 'Connecting to the digital world ...');
carPlot = plot(ax, nan, nan, 'o', 'MarkerSize', 10, ...
    'MarkerFaceColor', [0.55 0.38 0.76], 'MarkerEdgeColor', 'w');
headingPlot = plot(ax, nan, nan, '-', 'Color', [0.4 0.25 0.6], 'LineWidth', 2);
trackPlot = animatedline(ax, 'Color', [0.65 0.52 0.76], 'MaximumNumPoints', 3000);
targetPlot = plot(ax, nan, nan, 'x', 'Color', [0.3 0.5 0.3], 'MarkerSize', 9);
session = ''; seq = 0; pose = [0 0 0]; car = []; mapDrawn = false; serverPaused = false;
started = tic; previous = tic;
while isgraphics(fig) && toc(started) < maxSeconds
    loopStarted = tic;
    dt = min(toc(previous), 0.15); previous = tic;
    try
        if isempty(session)
            registration = webwrite([baseUrl '/api/devices/register'], struct('kind', 'matlab'), options);
            session = char(registration.session); car = registration.vehicle; seq = 0;
            pose = [car.position.x car.position.y car.yaw];
            if ~mapDrawn
                obstacles = registration.map.obstacles;
                for k = 1:numel(obstacles)
                    rectangle(ax, 'Position', [obstacles(k).x-.5 obstacles(k).y-.5 1 1], ...
                        'FaceColor', [0.73 0.80 0.70], 'EdgeColor', 'none');
                end
                xlim(ax, [-1 registration.map.width]); ylim(ax, [-1 registration.map.height]);
                set(ax, 'YDir', 'reverse'); mapDrawn = true;
            end
        end
        target = [car.waypoint.x car.waypoint.y];
        % Rotate before translation. This keeps the differential-drive body
        % within its reserved corridor when following right-angle grid turns.
        if car.has_command && ~serverPaused
            pose = controller_step(pose, target, dt);
        end
        seq = seq + 1;
        packet = struct('session', session, 'seq', seq, ...
            'command_id', car.command_id, ...
            'pose', struct('x', pose(1), 'y', pose(2), 'yaw', pose(3)));
        response = webwrite([baseUrl '/api/devices/matlab'], packet, options);
        car = response.vehicle; serverPaused = response.paused;
        % Snap only at an acknowledged endpoint, never to an unvisited target.
        if ~car.has_command
            pose(1:2) = [car.position.x car.position.y];
        end
        set(carPlot, 'XData', pose(1), 'YData', pose(2));
        set(headingPlot, 'XData', pose(1)+[0 .7*cos(pose(3))], ...
            'YData', pose(2)+[0 .7*sin(pose(3))]);
        set(targetPlot, 'XData', car.waypoint.x, 'YData', car.waypoint.y);
        addpoints(trackPlot, pose(1), pose(2));
        title(ax, sprintf('M01 | order #%d | %d deliveries | server T=%d', ...
            car.order, car.completed, response.tick));
        uistack(carPlot, 'top'); uistack(headingPlot, 'top');
    catch exception
        % Reset/network failure automatically re-registers and resumes from
        % the server's last accepted pose; no manual mode switch is needed.
        session = '';
        if isgraphics(fig), title(ax, ['Reconnecting: ' exception.message], 'Interpreter', 'none'); end
        pause(1);
    end
    drawnow limitrate;
    pause(max(0, 0.1-toc(loopStarted)));
end
end
