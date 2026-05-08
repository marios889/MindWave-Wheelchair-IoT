import os
import pandas as pd
import time

class DataCollector:
    def __init__(self):
        self.headers = [
            "timestamp", "attention", "meditation", "poor_signal",
            "delta", "theta", "low_alpha", "high_alpha",
            "low_beta", "high_beta", "low_gamma", "mid_gamma","blink",
            "label", "total_latency(ms)", "logic_time(ms)" 
        ]

    def get_file_handle(self):
        print("1) Create New File\n2) Append to a File\n3) Replace Existing File\n4) Exit")
        try:
            choice = int(input("Selection: "))
        except ValueError:
            return None

        readings_dir = os.path.join(os.getcwd(), "Readings")
        if not os.path.exists(readings_dir):
            os.makedirs(readings_dir)

        if choice == 1:
            name = input("Enter file name: ")
            filepath = os.path.join(readings_dir, f"{name}.csv")
            df = pd.DataFrame(columns=self.headers)
            df.to_csv(filepath, index=False)
            return filepath
        
        elif choice in [2, 3]: # Both Append and Replace need to list files
            files = [f for f in os.listdir(readings_dir) if f.endswith('.csv')]
            if not files: 
                print("No files found.")
                return None
                
            for i, f in enumerate(files): 
                print(f"{i+1}: {f}")
            
            file_num = int(input("Choose file: "))
            filepath = os.path.join(readings_dir, files[file_num-1])
            
            # If replacing, we overwrite it with fresh headers
            if choice == 3:
                df = pd.DataFrame(columns=self.headers)
                df.to_csv(filepath, index=False)
                
            return filepath
            
        return None

    def save_row(self, filepath, state_dict, label, total_lat, logic_t):
        row = [state_dict.get(h, 0) for h in self.headers[:-3]]
        row.extend([label, total_lat, logic_t])
        
        df = pd.DataFrame([row], columns=self.headers)
        df.to_csv(filepath, mode='a', header=False, index=False)