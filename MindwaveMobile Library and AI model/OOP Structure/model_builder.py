import pandas as pd
from bci_ai_model import BCIModel
from sklearn.utils import shuffle

def prepare_and_train():
    print("1. Merging RAW Data...")
    try:
        # IMPORTANT: Load the RAW files here, not the Engineered ones!
        df_n = pd.read_csv('Readings/Neutral_still_raw.csv') 
        df_f = pd.read_csv('Readings/Left_Combined_Raw.csv')     
    except FileNotFoundError:
        print("Error: Please make sure your raw CSVs are in this folder.")
        return

    # Combine the raw files chronologically (Do NOT shuffle yet)
    df_combined = pd.concat([df_n, df_f], ignore_index=True)

    # Save it to a temporary master RAW training file
    master_csv_path = 'master_raw_training_data.csv'
    df_combined.to_csv(master_csv_path, index=False)
    print(f"Raw data combined and saved to {master_csv_path}\n")

    # 2. Initialize the model
    print("2. Firing up the BCI Model Engine...")
    ai = BCIModel(model_path="test_rf_model.joblib")
    
    # --- NEW: Extract and save the modeled (engineered) data ---
    print("3. Generating and saving the Modeled Data...")
    # We pass a copy of the raw dataframe through the AI's internal math engine.
    # Using .copy() prevents Pandas from giving us memory warnings.
    df_modeled = ai._engineer_features(df_combined.copy()) 
    
    # df_modeled = shuffle(df_modeled, random_state=42).reset_index(drop=True) #Useless shuffle
    
    modeled_csv_path = 'master_modeled_training_data.csv'
    df_modeled.to_csv(modeled_csv_path, index=False)
    print(f"✅ Modeled data (with all rolling averages) saved to {modeled_csv_path}\n")

    # 4. Train the model
    print("4. Training the model...")
    # The model will read the raw CSV, do the math internally again, and train.
    ai.train_from_csv(master_csv_path)

if __name__ == "__main__":
    prepare_and_train()