import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from sklearn.manifold import TSNE
from sklearn.preprocessing import StandardScaler
from sklearn.ensemble import RandomForestClassifier

def visualize_high_dimensional_data(file1, name1, file2, name2):
    print(f"Loading {file1} and {file2}...")
    
    try:
        df1 = pd.read_csv(file1)
        df2 = pd.read_csv(file2)
    except FileNotFoundError as e:
        print(f"❌ Error: {e}")
        return

    # Tag and combine
    df1['temp_label'] = 0 # 0 for Neutral
    df2['temp_label'] = 1 # 1 for Left
    df_combined = pd.concat([df1, df2], ignore_index=True)

    # Dynamic Drop (Keep all your engineered 48 columns!)
    drop_cols = ['timestamp', 'poor_signal', 'total_latency(ms)', 'logic_time(ms)', 'blink', 'label', 'temp_label']
    features_df = df_combined.drop(columns=[col for col in drop_cols if col in df_combined.columns])
    
    # Drop rows with NaN values 
    valid_indices = features_df.dropna().index
    X_clean = features_df.loc[valid_indices]
    y_labels = df_combined.loc[valid_indices, 'temp_label']

    # Scale the Data
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X_clean)

    print("\nCrunching t-SNE... (This might take a few seconds)")

    # --- 1. t-SNE (The Cluster Map) ---
    # perplexity=30 is standard, but you might need to lower it to 10-15 if your dataset is small (like 60 rows)
    perplexity_val = min(30, len(X_clean) - 1) 
    tsne = TSNE(n_components=2, perplexity=perplexity_val, random_state=42)
    X_tsne = tsne.fit_transform(X_scaled)

    # --- 2. Feature Importance (The "What Matters" Bar Chart) ---
    rf = RandomForestClassifier(n_estimators=100, random_state=42)
    rf.fit(X_clean, y_labels)
    
    # Extract the top 15 most important columns
    importances = rf.feature_importances_
    indices = np.argsort(importances)[::-1][:15] # Top 15
    top_features = [X_clean.columns[i] for i in indices]
    top_scores = importances[indices]

    # --- PLOTTING ---
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # Chart 1: t-SNE Scatter Plot
    # 0 is Neutral (Blue), 1 is Left (Red)
    scatter = ax1.scatter(X_tsne[:, 0], X_tsne[:, 1], c=y_labels, cmap='bwr', alpha=0.7, edgecolors='w', s=60)
    ax1.set_title("t-SNE: True High-Dimensional Clustering", fontweight='bold')
    ax1.set_xlabel("t-SNE Dimension 1")
    ax1.set_ylabel("t-SNE Dimension 2")
    ax1.grid(True, linestyle='--', alpha=0.5)

    # Custom Legend
    from matplotlib.lines import Line2D
    legend_elements = [Line2D([0], [0], marker='o', color='w', label=name1, markerfacecolor='blue', markersize=10),
                    Line2D([0], [0], marker='o', color='w', label=name2, markerfacecolor='red', markersize=10)]
    ax1.legend(handles=legend_elements)

    # Chart 2: Feature Importances
    ax2.barh(range(len(top_features)), top_scores, color='purple', align='center')
    ax2.set_yticks(range(len(top_features)))
    ax2.set_yticklabels(top_features)
    ax2.invert_yaxis()  # Put the most important at the top
    ax2.set_title("Top 15 Columns Driving the AI's Decision", fontweight='bold')
    ax2.set_xlabel("Relative Importance Score")
    ax2.grid(True, axis='x', linestyle='--', alpha=0.5)

    plt.tight_layout()
    print("\n✅ Charts Generated! Look at the Bar Chart to see if your engineered features worked.")
    plt.show()

if __name__ == "__main__":
    visualize_high_dimensional_data(
        file1="Readings/Neutral_still_raw.csv", name1="Neutral (Idle)", 
        file2="Readings/Left_Combined_Raw.csv",        name2="Left (علاء)"
    )