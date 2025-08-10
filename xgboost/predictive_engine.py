"""
predictive_engine.py

Predictive Analytics Engine for vehicle emissions:
- Preprocessing and cleaning
- XGBoost training with grid search + 5-fold CV
- Anomaly detection with IsolationForest
- Incremental learning (continuation of training)
- Simple decision tree to estimate maintenance interval
- Real-time processing function that can be called every 15 minutes

Author: Generated for your research project.
"""

import os
import json
import time
from typing import Dict, Any, Tuple, Optional

import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split, GridSearchCV, KFold
from sklearn.preprocessing import StandardScaler, OneHotEncoder
from sklearn.compose import ColumnTransformer
from sklearn.pipeline import Pipeline
from sklearn.ensemble import IsolationForest
from sklearn.tree import DecisionTreeRegressor
from sklearn.metrics import mean_squared_error, r2_score
import joblib
import xgboost as xgb

# ---------- Configuration ----------
MODEL_DIR = "models"
os.makedirs(MODEL_DIR, exist_ok=True)

# Column mapping - adapt to your CSV if names differ
COL_MAP = {
    "car_id": "car_id",
    "manufacturer": "manufacturer",
    "model": "model",
    "transmission_type": "transmission_type",
    "engine_size_cm3": "engine_size_cm3",
    "fuel_type": "fuel_type",
    "power_ps": "power_ps",
    "co2": "co2_emissions_gPERkm"
}

NUMERIC_FEATURES = ["engine_size_cm3", "power_ps"]
CATEGORICAL_FEATURES = ["fuel_type", "transmission_type", "manufacturer"]  # manufacturer optional

TARGET = "co2_emissions_gPERkm"

# ---------- Helpers ----------
def load_dataset(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    # rename columns if needed (attempt mapping)
    for k, v in COL_MAP.items():
        if k in df.columns and v not in df.columns:
            df.rename(columns={k: v}, inplace=True)
    return df

def basic_preprocess(df: pd.DataFrame) -> pd.DataFrame:
    # Fill missing values according to your description
    df = df.copy()

    # transmission_type: fill Electric -> Automatic (if electric vehicles are represented differently)
    df["transmission_type"] = df.get("transmission_type", pd.Series(["Automatic"]*len(df)))
    df["transmission_type"] = df["transmission_type"].fillna("Automatic")

    # engine_size_cm3: null -> 0.0 for electric (or fillna 0.0)
    df["engine_size_cm3"] = pd.to_numeric(df.get("engine_size_cm3", 0.0), errors="coerce").fillna(0.0)

    # power_ps: if null but co2==0 -> fill 0.0
    df["power_ps"] = pd.to_numeric(df.get("power_ps", 0.0), errors="coerce")
    mask = (df["power_ps"].isna()) & (df[TARGET] == 0)
    df.loc[mask, "power_ps"] = 0.0
    df["power_ps"] = df["power_ps"].fillna(df["power_ps"].median())

    # fuel_type manual fixes (if blanks)
    df["fuel_type"] = df.get("fuel_type", pd.Series(["Petrol"]*len(df))).fillna("Petrol")

    # Remove unrealistic negative values
    for col in [TARGET, "engine_size_cm3", "power_ps"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce").fillna(0.0).clip(lower=0.0)

    # Example feature engineering: power-to-weight, engine_size categorical bins etc. (extend as needed)
    # If 'weight' not present skip that part.
    return df

# ---------- Feature pipeline ----------
def build_preprocessor(numeric_features=NUMERIC_FEATURES, categorical_features=CATEGORICAL_FEATURES):
    numeric_transformer = Pipeline(steps=[
        ("scaler", StandardScaler())
    ])
    categorical_transformer = Pipeline(steps=[
        ("ohe", OneHotEncoder(handle_unknown="ignore", sparse=False))
    ])

    preprocessor = ColumnTransformer(transformers=[
        ("num", numeric_transformer, numeric_features),
        ("cat", categorical_transformer, categorical_features)
    ], remainder="drop")

    return preprocessor

# ---------- Training ----------
def train_xgboost(df: pd.DataFrame, model_path: str = None, n_jobs=4) -> Tuple[xgb.Booster, Dict[str,Any]]:
    """
    Train XGBoost regressor for CO2. Returns booster and training artifacts dict.
    """

    df = df.copy()
    df = basic_preprocess(df)

    # Drop rows with missing target
    df = df[df[TARGET].notna()]

    X = df[NUMERIC_FEATURES + CATEGORICAL_FEATURES]
    y = df[TARGET].astype(float)

    # Preprocessor + create design matrix
    preproc = build_preprocessor(NUMERIC_FEATURES, CATEGORICAL_FEATURES)
    X_trans = preproc.fit_transform(X)

    # Convert to DMatrix
    dtrain = xgb.DMatrix(X_trans, label=y)

    # grid-search hyperparams (small example)
    param_grid = {
        "eta": [0.01, 0.05, 0.1],
        "max_depth": [4, 6, 8],
        "subsample": [0.7, 0.9],
        "colsample_bytree": [0.6, 0.8],
    }

    # We'll do a manual grid search with cross validation (xgboost cv)
    best_rmse = float("inf")
    best_params = None
    best_booster = None

    for eta in param_grid["eta"]:
        for md in param_grid["max_depth"]:
            for ss in param_grid["subsample"]:
                for cs in param_grid["colsample_bytree"]:
                    params = {
                        "objective": "reg:squarederror",
                        "eta": eta,
                        "max_depth": md,
                        "subsample": ss,
                        "colsample_bytree": cs,
                        "tree_method": "hist",
                        "verbosity": 0,
                    }
                    # 5-fold cv
                    cv_results = xgb.cv(params, dtrain, num_boost_round=200, nfold=5, metrics="rmse", early_stopping_rounds=10, seed=42, as_pandas=True)
                    rmse = cv_results["test-rmse-mean"].min()
                    rounds = cv_results.shape[0]
                    if rmse < best_rmse:
                        best_rmse = rmse
                        best_params = dict(params, num_boost_round=rounds)
                    # small progress print
                    print(f"tested eta={eta} md={md} ss={ss} cs={cs} -> rmse={rmse:.4f}")
    print("Best RMSE:", best_rmse, "Best params:", best_params)

    # Train final booster on full training set with best params
    num_boost = best_params.pop("num_boost_round")
    full_params = best_params
    booster = xgb.train(full_params, dtrain, num_boost_round=num_boost)

    # Save preprocessor and booster
    artifacts = {
        "preprocessor": preproc,
        "best_params": full_params,
        "num_boost_round": num_boost
    }
    if model_path is None:
        model_path = os.path.join(MODEL_DIR, "xgb_co2_model.json")
    preproc_path = os.path.join(MODEL_DIR, "preproc.joblib")
    joblib.dump(preproc, preproc_path)
    booster.save_model(model_path)
    joblib.dump(artifacts, os.path.join(MODEL_DIR, "artifacts.joblib"))
    print("Saved model to", model_path)
    return booster, artifacts

# ---------- Incremental update ----------
def incremental_update(new_df: pd.DataFrame, existing_model_path: str, artifacts_path: str):
    """
    Continue training the saved xgboost booster using new_df.
    new_df: must contain same feature columns as training set.
    This uses xgboost.train with xgb_model parameter to continue.
    """
    # load preprocessor / artifacts
    preproc = joblib.load(os.path.join(MODEL_DIR, "preproc.joblib"))
    artifacts = joblib.load(artifacts_path)
    booster = xgb.Booster()
    booster.load_model(existing_model_path)

    # preprocess new data
    df = basic_preprocess(new_df)
    df = df[df[TARGET].notna()]
    X = df[NUMERIC_FEATURES + CATEGORICAL_FEATURES]
    y = df[TARGET].astype(float)
    X_trans = preproc.transform(X)
    dnew = xgb.DMatrix(X_trans, label=y)

    params = artifacts["best_params"]
    # continue for small number of rounds
    booster_updated = xgb.train(params, dnew, num_boost_round=50, xgb_model=booster)
    booster_updated.save_model(existing_model_path)
    print("Incremental update complete; model overwritten at", existing_model_path)
    return booster_updated

# ---------- Anomaly detection ----------
def build_anomaly_detector(train_df: pd.DataFrame) -> IsolationForest:
    # use numeric features and target residual-ish for anomaly modeling
    df = basic_preprocess(train_df)
    X = df[NUMERIC_FEATURES].fillna(0.0)
    iso = IsolationForest(contamination=0.01, random_state=42)
    iso.fit(X)
    joblib.dump(iso, os.path.join(MODEL_DIR, "anomaly_if.joblib"))
    return iso

# ---------- Maintenance Decision Tree ----------
def train_maintenance_tree(df: pd.DataFrame) -> DecisionTreeRegressor:
    """
    Build a toy decision tree mapping predicted emission changes to maintenance interval.
    For a production system you'd derive labels from service logs: time-to-service after a reading.
    Here we synthesize labels for demo: higher co2 -> shorter interval.
    """
    df = basic_preprocess(df)
    X = df[NUMERIC_FEATURES + CATEGORICAL_FEATURES]
    # dummy labels: map CO2 to days-to-service inversely
    y = np.maximum(7, 180 - (df[TARGET] / df[TARGET].max()) * 180)  # between 7 and 180 days
    # preprocess features
    preproc = joblib.load(os.path.join(MODEL_DIR, "preproc.joblib"))
    X_trans = preproc.transform(X)
    tree = DecisionTreeRegressor(max_depth=6, random_state=42)
    tree.fit(X_trans, y)
    joblib.dump(tree, os.path.join(MODEL_DIR, "maintenance_tree.joblib"))
    return tree

# ---------- Real-time processing (called every 15 minutes) ----------
def process_realtime_record(record: Dict[str, Any]) -> Dict[str, Any]:
    """
    record: dict with keys vehicleId, region, timestamp, co2, nox, pm25 (or HC)
    Returns: result dict with keys: anomaly(bool), predicted_co2(float), maintenance_days(int), action(str)
    """
    # load preproc and models
    preproc = joblib.load(os.path.join(MODEL_DIR, "preproc.joblib"))
    booster = xgb.Booster()
    booster.load_model(os.path.join(MODEL_DIR, "xgb_co2_model.json"))
    iso = joblib.load(os.path.join(MODEL_DIR, "anomaly_if.joblib"))
    tree = joblib.load(os.path.join(MODEL_DIR, "maintenance_tree.joblib"))

    # create dataframe from record
    df = pd.DataFrame([{
        "engine_size_cm3": float(record.get("engine_size_cm3", 0.0)),
        "power_ps": float(record.get("power_ps", 0.0)),
        "fuel_type": record.get("fuel_type", "Petrol"),
        "transmission_type": record.get("transmission_type", "Automatic"),
        "manufacturer": record.get("manufacturer", "Unknown"),
        TARGET: float(record.get("co2", 0.0))
    }])

    # anomaly check using numeric features
    X_num = df[NUMERIC_FEATURES].fillna(0.0)
    is_anom = iso.predict(X_num)[0] == -1

    # if anomaly: flag and request more data (mock)
    if is_anom:
        action = "ANOMALY: request more samples or flag MVI"
        return {"anomaly": True, "action": action, "predicted_co2": None, "maintenance_days": None}

    # predict co2 (use preproc)
    X_trans = preproc.transform(df[NUMERIC_FEATURES + CATEGORICAL_FEATURES])
    dtest = xgb.DMatrix(X_trans)
    pred_co2 = float(booster.predict(dtest)[0])

    # maintenance days prediction
    maint_days = int(tree.predict(X_trans)[0])

    # Compare predicted_co2 to threshold (example threshold per fuel)
    thresholds = {"Diesel": 1500.0, "Petrol": 1200.0, "CNG": 1000.0}
    fuel = record.get("fuel_type", "Petrol")
    limit = thresholds.get(fuel, 1200.0)
    compliance = pred_co2 <= limit

    # Decide action
    if not compliance:
        action = "ALERT: predicted non-compliance -> notify user + MVI"
    elif maint_days <= 14:
        action = "NOTICE: recommend maintenance within {} days".format(maint_days)
    else:
        action = "OK"

    result = {"anomaly": False, "predicted_co2": pred_co2, "maintenance_days": maint_days, "action": action}
    return result

# ---------- Utilities ----------
def evaluate_on_test(booster: xgb.Booster, df_test: pd.DataFrame):
    preproc = joblib.load(os.path.join(MODEL_DIR, "preproc.joblib"))
    X_test = df_test[NUMERIC_FEATURES + CATEGORICAL_FEATURES]
    y_test = df_test[TARGET].astype(float)
    Xt = preproc.transform(X_test)
    preds = booster.predict(xgb.DMatrix(Xt))
    rmse = np.sqrt(mean_squared_error(y_test, preds))
    r2 = r2_score(y_test, preds)
    print(f"Test RMSE: {rmse:.3f}, R2: {r2:.3f}")
    return rmse, r2

# ---------- Example usage ----------
if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--train_csv", type=str, help="Path to historic dataset CSV for training", default=None)
    parser.add_argument("--do_train", action="store_true")
    parser.add_argument("--do_anomaly", action="store_true")
    parser.add_argument("--simulate_realtime", action="store_true", help="simulate realtime by reading new lines from CSV")
    args = parser.parse_args()

    if args.do_train:
        if args.train_csv is None:
            raise SystemExit("Provide --train_csv path")
        df = load_dataset(args.train_csv)
        df = basic_preprocess(df)
        # split
        train_df, test_df = train_test_split(df, test_size=0.2, random_state=42)
        booster, artifacts = train_xgboost(train_df)
        joblib.dump(artifacts, os.path.join(MODEL_DIR, "artifacts.joblib"))
        evaluate_on_test(booster, test_df)
        if args.do_anomaly:
            build_anomaly_detector(train_df)
        # train maintenance tree
        train_maintenance_tree(train_df)
        print("Training complete.")

    if args.simulate_realtime:
        # tail a CSV or iterate through test set
        if args.train_csv is None:
            raise SystemExit("Provide --train_csv to simulate")
        df = load_dataset(args.train_csv)
        df = basic_preprocess(df)
        # simulate reading new incoming records every 15 minutes -> we'll sleep shorter for demo
        for i, row in df.head(200).iterrows():
            rec = {
                "co2": float(row[TARGET]),
                "engine_size_cm3": float(row.get("engine_size_cm3", 0.0)),
                "power_ps": float(row.get("power_ps", 0.0)),
                "fuel_type": row.get("fuel_type", "Petrol"),
                "transmission_type": row.get("transmission_type", "Automatic"),
                "manufacturer": row.get("manufacturer", "Unknown"),
            }
            out = process_realtime_record(rec)
            print(i, out)
            time.sleep(1.0)  # in real system wait 15*60
