% Read the CSV file
data = readtable('rocking_peaks.csv');

% Compute time since start (in seconds)
time_s = (data.timestamp_ms - data.timestamp_ms(1)) / 1000;
angle = data.angle_deg;

% Plot raw data
figure;
plot(time_s, angle, 'Color', [0.5 0.5 0.5], 'DisplayName', 'Angle (raw)');
hold on;

n_points = length(time_s);
window = 2000;
mov_avg = nan(size(angle));

for i = 1:n_points
    left = max(1, i - window);
    right = min(n_points, i + window);
    mov_avg(i) = mean(angle(left:right), 'omitnan');
end

plot(time_s, mov_avg, 'b-', 'LineWidth', 2, 'DisplayName', 'Moving average');

% Output required info to console
fprintf('Total number of points: %d\n', n_points);
first_avg = mov_avg(find(~isnan(mov_avg), 1, 'first'));
last_avg = mov_avg(find(~isnan(mov_avg), 1, 'last'));
diff_avg = last_avg - first_avg;
fprintf('First moving average: %.6f\n', first_avg);
fprintf('Last moving average: %.6f\n', last_avg);
fprintf('Difference between first and last moving averages: %.6f\n', diff_avg);

xlabel('Time since start (seconds)');
ylabel('Angle (deg)');
title('Angle vs. Time with Moving Average');
legend('show');
grid on;
hold off; 