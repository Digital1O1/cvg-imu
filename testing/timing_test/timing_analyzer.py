import pandas as pd
import numpy as np

# Load CSV files, skip the header row
send_df = pd.read_csv('send_time.csv', skiprows=1, header=None)
test_df = pd.read_csv('timing_test.csv', skiprows=1, header=None)

send_times = send_df[0].values
test_times = test_df[0].values

diffs = test_times - send_times
avg_diff = np.mean(diffs)

print(f"Average difference (timing_test - send_time): {avg_diff:.2f} ms") 