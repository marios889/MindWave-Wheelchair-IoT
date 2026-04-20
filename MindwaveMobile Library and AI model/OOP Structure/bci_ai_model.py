import pandas as pd
import os
import joblib
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import GridSearchCV, train_test_split
from sklearn.metrics import f1_score, classification_report, roc_auc_score

class BCIModel:
    def __init__(self, model_path="bci_rf_model.joblib", window_size=3):
        self.model_path = model_path
        self.model = None
        self.window_size = window_size
        self.state_buffer = [] # Memory bank for live prediction
        
        # The core columns we will apply math to
        self.base_features = [
            "delta", "theta", "low_alpha", "high_alpha", 
            "low_beta", "high_beta", "low_gamma", "mid_gamma"
        ]
        self.features = [] # Will hold the final 48 column names

    def _engineer_features(self, df):
        """
        THE ENGINE: This applies the rolling window math and cleans the data.
        Both train_from_csv and predict() use this exact same function!
        """
        # 1. Apply the rolling window math to every base feature
        for col in self.base_features:
            if col in df.columns:
                df[f"{col}_mean"] = df[col].rolling(window=self.window_size, min_periods=1).mean()
                df[f"{col}_std"]  = df[col].rolling(window=self.window_size, min_periods=1).std().fillna(0)
                df[f"{col}_max"]  = df[col].rolling(window=self.window_size, min_periods=1).max()
                df[f"{col}_min"]  = df[col].rolling(window=self.window_size, min_periods=1).min()
                df[f"{col}_p2p"]  = df[f"{col}_max"] - df[f"{col}_min"]

        # 2. Clean the garbage once and for all
        drop_cols = ['timestamp', 'poor_signal', 'total_latency(ms)', 'logic_time(ms)', 'blink']
        df = df.drop(columns=[col for col in drop_cols if col in df.columns])
        
        return df

    def train_from_csv(self, csv_path):
        """Trains the model from a RAW CSV by engineering it first."""
        if not os.path.exists(csv_path):
            print(f"Error: {csv_path} not found.")
            return

        print("Engineering features and starting training...")
        
        # Read the raw data
        raw_data = pd.read_csv(csv_path)
        
        # Pass it through our Engine to add math and drop garbage
        data = self._engineer_features(raw_data)
        
        # Dynamically grab all column names EXCEPT 'label' to be our features
        self.features = [col for col in data.columns if col != 'label']
        
        X = data[self.features]
        y = data["label"]

        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.1, random_state=42, shuffle= False
        )

        param_grid = {
            'n_estimators': [50, 100], # Keep it lower. 200 just wastes time here.
            'max_depth': [3, 5, 8],    # KILL 'None'. Force the AI to be shallow and general.
            'min_samples_split': [10, 20], # Demand at least 10-20 rows before it's allowed to make a new rule.
            'min_samples_leaf': [5, 10],   # Force the final decision to be backed by at least 5-10 rows of proof.
            'max_features': ['sqrt', 'log2']
        }

        rf = RandomForestClassifier(random_state=42, class_weight='balanced')
        grid_search = GridSearchCV(estimator=rf, param_grid=param_grid, cv=5, n_jobs=-1, scoring='f1_weighted')
        
        grid_search.fit(X_train, y_train)
        self.model = grid_search.best_estimator_
        
        # --- Metrics & Saving ---
        predictions = self.model.predict(X_test)
        prob_predictions = self.model.predict_proba(X_test)
        
        f1 = f1_score(y_test, predictions, average='weighted')
        if len(self.model.classes_) == 2:
            auc_score = roc_auc_score(y_test, prob_predictions[:, 1])
        else:
            auc_score = roc_auc_score(y_test, prob_predictions, multi_class='ovr', average='weighted')
        
        print(f"\n🧠 F1-SCORE: {f1:.2f} | ROC-AUC: {auc_score:.2f}\n")
        print(classification_report(y_test, predictions))

        # Save both the model and the dynamically generated feature list
        joblib.dump({'model': self.model, 'features': self.features}, self.model_path)
        print(f"Model saved to {self.model_path}")

    def load_or_train(self, csv_path):
        if os.path.exists(self.model_path):
            saved_data = joblib.load(self.model_path)
            self.model = saved_data['model']
            self.features = saved_data['features']
        else:
            self.train_from_csv(csv_path)

    def predict(self, current_state_dict):
        if self.model is None:
            return "NO_MODEL"
            
        # 1. Add current live reading to memory bank
        self.state_buffer.append(current_state_dict)
        if len(self.state_buffer) > self.window_size:
            self.state_buffer.pop(0) 
        
        # 2. Turn the 3-row buffer into a DataFrame
        df_buffer = pd.DataFrame(self.state_buffer)
        
        # 3. Run it through the EXACT SAME Engine used for training
        df_engineered = self._engineer_features(df_buffer)
        
        # 4. Extract ONLY the very last row (which contains the 3-second average)
        live_row = df_engineered.iloc[-1:]
        
        # Safety check: Ensure columns match perfectly
        for f in self.features:
            if f not in live_row.columns:
                live_row[f] = 0
                
        live_row = live_row[self.features]
        
        return self.model.predict(live_row)[0]