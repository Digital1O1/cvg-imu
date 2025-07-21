import numpy as np
import csv
import matplotlib.pyplot as plt

csv_path = './consistency_test.csv'

# Read timestamps from CSV
timestamps = []
with open(csv_path, 'r') as f:
    reader = csv.reader(f)
    for row in reader:
        # Skip header or empty lines
        if row and row[0].isdigit():
            timestamps.append(int(row[0]))

if len(timestamps) < 2:
    print("Not enough events to calculate frequency.")
    exit(1)

# Calculate intervals in milliseconds
intervals_ms = np.diff(timestamps)
# Convert intervals to seconds
intervals_s = intervals_ms / 1000.0
avg_interval = np.mean(intervals_s)
frequency = 1.0 / avg_interval

print(f"Average interval: {avg_interval:.4f} seconds")
print(f"Event frequency: {frequency:.4f} Hz")

# Output the average interval between points
print(f"Average interval between points: {avg_interval:.4f} seconds")

# Plot event vs time as a dot plot
# Convert timestamps to seconds for plotting
timestamps_s = [t / 1000.0 for t in timestamps]
event_numbers = list(range(1, len(timestamps_s) + 1))

plt.figure(figsize=(8, 4))
plt.plot(timestamps_s, event_numbers, 'o', markersize=4)
plt.xlabel('Time (s)')
plt.ylabel('Event Number')
plt.title('Event vs Time Dot Plot')
plt.grid(True, which='both', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.show()