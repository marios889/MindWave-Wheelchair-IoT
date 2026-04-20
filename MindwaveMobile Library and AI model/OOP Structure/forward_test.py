import pandas as pd
import joblib
import os
from collections import Counter
from bci_ai_model import BCIModel  # Importing your brain!

def evaluate_new_data(csv_path, model_path="test_rf_model.joblib"):
    # 1. Check if files exist
    if not os.path.exists(model_path):
        print(f"❌ Error: Model not found at {model_path}")
        return
    if not os.path.exists(csv_path):
        print(f"❌ Error: Data file not found at {csv_path}")
        return

    # 2. Load the trained model AND the required feature list
    print(f"Loading model from {model_path}...")
    saved_data = joblib.load(model_path)
    model = saved_data['model']
    required_features = saved_data['features'] # The exact 48 columns it expects

    # 3. Load the raw test data
    print(f"Loading test data from {csv_path}...\n")
    df_raw = pd.read_csv(csv_path)

    # 4. THE MAGIC STEP: Feature Engineering
    # Instantiate your class just to use the math engine
    ai = BCIModel()
    print("⚙️ Passing raw data through the Feature Engineering engine...")
    df_engineered = ai._engineer_features(df_raw.copy())
    
    # Check to make sure the math engine didn't miss anything
    missing_cols = [col for col in required_features if col not in df_engineered.columns]
    if missing_cols:
        print(f"❌ Error: The math engine failed to create these columns: {missing_cols}")
        return

    # Force the dataframe into the exact shape and order the model expects
    X_new = df_engineered[required_features]

    # 5. Make Predictions
    predictions = model.predict(X_new)
    
    # 6. Analyze the Results
    total_rows = len(predictions)
    counts = Counter(predictions)
    
    print("\n========================================")
    print("🧠 LIVE PREDICTION RESULTS")
    print("========================================")
    print(f"Total readings analyzed: {total_rows}\n")
    
    for label, count in counts.items():
        percentage = (count / total_rows) * 100
        print(f"Predicted '{label}': {count} times ({percentage:.1f}%)")
        
    # If the file happens to have a 'label' column, calculate real metrics
    if 'label' in df_engineered.columns:
        from sklearn.metrics import accuracy_score, f1_score
        y_true = df_engineered['label']
        acc = accuracy_score(y_true, predictions)
        
        try:
            # Using average='weighted' is safer for general testing
            f1 = f1_score(y_true, predictions, average='weighted')
            print("\n--- Ground Truth Evaluation ---")
            print(f"Actual Accuracy: {acc * 100:.2f}%")
            print(f"F1-Score: {f1:.2f}")
            print(df_engineered.head(3))
        except Exception as e:
            print(f"Could not calculate F1-score: {e}")

if __name__ == "__main__":
    # Change this to whatever raw file you want to test!
    evaluate_new_data(csv_path="Readings/Left41.csv", model_path="test_rf_model.joblib")