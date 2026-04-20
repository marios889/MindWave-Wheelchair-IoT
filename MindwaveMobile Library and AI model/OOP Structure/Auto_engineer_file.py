import pandas as pd

def engineer_features(df, window_size=3):
    """
    THE ENGINE: This applies the rolling window math and cleans the data.
    """
    # The core columns we will apply math to
    base_features = [
        "delta", "theta", "low_alpha", "high_alpha", 
        "low_beta", "high_beta", "low_gamma", "mid_gamma"
    ]
    
    # 1. Apply the rolling window math to every base feature
    for col in base_features:
        if col in df.columns:
            df[f"{col}_mean"] = df[col].rolling(window=window_size, min_periods=1).mean()
            df[f"{col}_std"]  = df[col].rolling(window=window_size, min_periods=1).std().fillna(0)
            df[f"{col}_max"]  = df[col].rolling(window=window_size, min_periods=1).max()
            df[f"{col}_min"]  = df[col].rolling(window=window_size, min_periods=1).min()
            df[f"{col}_p2p"]  = df[f"{col}_max"] - df[f"{col}_min"]

    # 2. Clean the garbage once and for all
    drop_cols = ['timestamp', 'poor_signal', 'total_latency(ms)', 'logic_time(ms)', 'blink']
    
    # Drop the columns if they exist in the dataframe
    existing_drops = [col for col in drop_cols if col in df.columns]
    df = df.drop(columns=existing_drops)
    
    return df

# --- RUNNING THE SCRIPT ---
if __name__ == "__main__":
    try:
        print("Reading raw file...")
        file = pd.read_csv("Readings/Neutral_Combined_Raw.csv")
        
        print("Engineering features...")
        x = engineer_features(file)
        
        print("Saving engineered file...")
        x.to_csv("Readings/Neutral_Engineered.csv", index=False)
        print("✅ Success! File saved as Left_Engineered.csv")
        
    except FileNotFoundError:
        print("❌ Error: Could not find 'Readings/Neutral_Combined_Raw.csv'. Check your folder path.")