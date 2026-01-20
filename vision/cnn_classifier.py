"""
APSV Stage 4: CNN-Based Feature Learning
---------------------------------------
Realtime inference pipeline using OpenCV for preprocessing and a lightweight
CNN for classification (paddy vs non-paddy).

Usage examples:
  python vision/cnn_classifier.py
  python vision/cnn_classifier.py --model models/apsv_cnn.keras
  python vision/cnn_classifier.py --train data/train --epochs 5
"""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np
import tensorflow as tf

DEFAULT_MODEL_PATH = Path("models") / "apsv_cnn.keras"
DEFAULT_INPUT_SIZE = (128, 128)
DEFAULT_LABELS = ["non_paddy", "paddy"]


def build_model(input_shape=(128, 128, 1), num_classes=2) -> tf.keras.Model:
    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=input_shape),
            tf.keras.layers.Conv2D(32, (3, 3), activation="relu"),
            tf.keras.layers.MaxPooling2D((2, 2)),
            tf.keras.layers.Conv2D(64, (3, 3), activation="relu"),
            tf.keras.layers.MaxPooling2D((2, 2)),
            tf.keras.layers.Flatten(),
            tf.keras.layers.Dense(128, activation="relu"),
            tf.keras.layers.Dropout(0.5),
            tf.keras.layers.Dense(num_classes, activation="softmax"),
        ]
    )
    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def preprocess_frame(frame: np.ndarray, input_size=(128, 128)) -> np.ndarray:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    resized = cv2.resize(gray, input_size, interpolation=cv2.INTER_AREA)
    normalized = resized.astype(np.float32) / 255.0
    normalized = np.expand_dims(normalized, axis=-1)
    batch = np.expand_dims(normalized, axis=0)
    return batch


def train_from_directory(model: tf.keras.Model, data_dir: Path, epochs: int) -> tf.keras.Model:
    dataset = tf.keras.utils.image_dataset_from_directory(
        data_dir,
        labels="inferred",
        label_mode="int",
        color_mode="grayscale",
        image_size=DEFAULT_INPUT_SIZE,
        batch_size=32,
        shuffle=True,
    )
    model.fit(dataset, epochs=epochs)
    return model


def run_realtime(model: tf.keras.Model, camera_index: int) -> None:
    cap = cv2.VideoCapture(camera_index)
    if not cap.isOpened():
        print("Camera not opened")
        return

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        batch = preprocess_frame(frame, DEFAULT_INPUT_SIZE)
        preds = model.predict(batch, verbose=0)[0]
        label_idx = int(np.argmax(preds))
        confidence = float(preds[label_idx])
        label = DEFAULT_LABELS[label_idx]

        overlay = f"{label} ({confidence:.2f})"
        cv2.putText(
            frame,
            overlay,
            (10, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.9,
            (0, 255, 0),
            2,
            cv2.LINE_AA,
        )

        cv2.imshow("APSV CNN Classification", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()


def main() -> None:
    parser = argparse.ArgumentParser(description="APSV CNN classifier (Stage 4)")
    parser.add_argument("--model", type=str, default=str(DEFAULT_MODEL_PATH))
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--train", type=str, default=None)
    parser.add_argument("--epochs", type=int, default=5)

    args = parser.parse_args()

    model_path = Path(args.model)
    if model_path.exists():
        model = tf.keras.models.load_model(model_path)
        print(f"Loaded model: {model_path}")
    else:
        model = build_model(input_shape=(*DEFAULT_INPUT_SIZE, 1), num_classes=2)
        print("No trained model found. Using a newly initialized CNN.")

    if args.train:
        data_dir = Path(args.train)
        if not data_dir.exists():
            print(f"Training directory not found: {data_dir}")
            return
        model = train_from_directory(model, data_dir, args.epochs)
        model_path.parent.mkdir(parents=True, exist_ok=True)
        model.save(model_path)
        print(f"Saved trained model to: {model_path}")

    run_realtime(model, args.camera)


if __name__ == "__main__":
    main()
