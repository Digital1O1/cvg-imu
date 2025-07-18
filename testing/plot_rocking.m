% Read the CSV file
data = readtable('rocking_peaks.csv');

% Compute time since start (in seconds)
time_s = (data.timestamp_ms - data.timestamp_ms(1)) / 1000;
angle = data.angle_deg;

% Plot raw data
figure;
plot(time_s, angle, 'Color', [0.5 0.5 0.5], 'DisplayName', 'Angle (raw)');
hold on;

N = 27; % Window size
n_points = length(time_s);
fit_all = nan(size(angle)); % Preallocate for best-fit line
counts = zeros(n_points, 1); % For averaging overlapping fits
midlines = nan(n_points, 1); % Store midline for each point

for i = 1:(n_points-N+1)
    idx = i:(i+N-1);
    x = time_s(idx);
    y = angle(idx);
    if numel(x) < 5
        continue;
    end
    % Estimate frequency using FFT for better initial guess
    Fs = 1/mean(diff(x));
    Y = fft(y - mean(y));
    L = length(y);
    P2 = abs(Y/L);
    P1 = P2(2:floor(L/2)+1); % Ignore DC
    [~, idx_f] = max(P1);
    f_dom = idx_f / (L * mean(diff(x)));
    w_guess = 2 * pi * f_dom;
    % Initial guesses
    A_guess = (max(y) - min(y)) / 2;
    phi_guess = 0;
    c_guess = mean(y);
    sinusoid = @(b, x) b(1) * sin(b(2) * x + b(3)) + b(4);
    beta0 = [A_guess, w_guess, phi_guess, c_guess];
    try
        errfun = @(b) sum((sinusoid(b, x) - y).^2);
        beta = fminsearch(errfun, beta0);
        y_fit = sinusoid(beta, x);
        fit_all(idx) = sum([fit_all(idx), y_fit], 2, 'omitnan'); % Sum fits for averaging
        midlines(idx) = sum([midlines(idx), repmat(beta(4), N, 1)], 2, 'omitnan');
        counts(idx) = counts(idx) + 1;
    catch ME
        fprintf('Segment %d-%d fit failed: %s\n', idx(1), idx(end), ME.message);
    end
end

% Average overlapping fits
fit_all = fit_all ./ max(counts, 1);
midlines = midlines ./ max(counts, 1);

% Plot the concatenated best-fit line
% plot(time_s, fit_all, 'r-', 'LineWidth', 2, 'DisplayName', 'Concatenated best-fit');

% Plot the concatenated midlines
% plot(time_s, midlines, 'b-', 'LineWidth', 2, 'DisplayName', 'Concatenated midline');

% Print the difference between the first and last midline values
first_midline = midlines(find(~isnan(midlines), 1, 'first'));
last_midline = midlines(find(~isnan(midlines), 1, 'last'));
diff_midline = last_midline - first_midline;
fprintf('Difference between first and last midline values: %.6f\n', diff_midline);

xlabel('Time since start (seconds)');
ylabel('Angle (deg)');
title('Angle vs. Time with Concatenated Sinusoidal Best-Fit (Sliding 10-point Window)');
legend('show');
grid on;
hold off; 