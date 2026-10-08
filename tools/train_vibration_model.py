#!/usr/bin/env python3
"""Train and export the RA8D1 BMI088 vibration-temperature MLP.

The input is one or more serial-terminal log files produced by this firmware.
``DATA,`` and matching ``TEMP,`` lines are parsed; timestamps or other terminal
messages can remain in the same log.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
from typing import Iterable

import numpy as np
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.neural_network import MLPClassifier
from sklearn.preprocessing import StandardScaler

SAMPLE_RATE_HZ = 800
WINDOW_SAMPLES = 512
FEATURE_COUNT = 26
CLASS_NAMES = ("normal", "imbalance", "loose", "rub", "bearing")
ACCEL_LSB_TO_G = 6.0 / 32768.0
GYRO_LSB_TO_DPS = 500.0 / 32768.0
GOERTZEL_FREQUENCIES = np.asarray(
    (12.5, 25.0, 50.0, 75.0, 100.0, 125.0, 150.0, 200.0, 250.0, 300.0, 350.0, 390.0),
    dtype=np.float64,
)
GOERTZEL_COEFFICIENTS = 2.0 * np.cos(2.0 * np.pi * GOERTZEL_FREQUENCIES / SAMPLE_RATE_HZ)


def parse_logs(paths: Iterable[Path]) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    windows: dict[tuple[int, int, int], dict[int, list[int]]] = defaultdict(dict)
    temperatures: dict[tuple[int, int], tuple[int, int]] = {}

    for source_id, path in enumerate(paths):
        with path.open("r", encoding="utf-8", errors="ignore") as stream:
            for line in stream:
                marker = line.find("DATA,")
                if marker >= 0:
                    fields = line[marker:].strip().split(",")
                    if len(fields) == 10:
                        try:
                            window_id, label, sample_id = map(int, fields[1:4])
                            axes = list(map(int, fields[4:10]))
                        except ValueError:
                            pass
                        else:
                            if 0 <= label < len(CLASS_NAMES) and 0 <= sample_id < WINDOW_SAMPLES:
                                windows[(source_id, window_id, label)][sample_id] = axes

                marker = line.find("TEMP,")
                if marker >= 0:
                    fields = line[marker:].strip().split(",")
                    if len(fields) == 5:
                        try:
                            window_id = int(fields[1])
                            current_millideg_c = int(fields[2])
                            rise_millideg_c = int(fields[3])
                        except ValueError:
                            pass
                        else:
                            temperatures[(source_id, window_id)] = (
                                current_millideg_c,
                                rise_millideg_c,
                            )

    samples: list[np.ndarray] = []
    temperature_features: list[tuple[int, int]] = []
    labels: list[int] = []
    for (source_id, window_id, label), rows in sorted(windows.items()):
        temperature = temperatures.get((source_id, window_id))
        if len(rows) != WINDOW_SAMPLES or temperature is None:
            continue
        ordered = np.asarray([rows[index] for index in range(WINDOW_SAMPLES)], dtype=np.float64)
        samples.append(ordered)
        temperature_features.append(temperature)
        labels.append(label)

    if not samples:
        raise ValueError("No complete 512-sample DATA windows with matching TEMP lines were found")
    return (
        np.stack(samples),
        np.asarray(temperature_features, dtype=np.float64),
        np.asarray(labels, dtype=np.int64),
    )


def extract_features(raw: np.ndarray, temperature: np.ndarray) -> np.ndarray:
    accel = raw[:, :3] * ACCEL_LSB_TO_G
    gyro = raw[:, 3:] * GYRO_LSB_TO_DPS

    accel_centered = accel - accel.mean(axis=0, keepdims=True)
    magnitude = np.linalg.norm(accel, axis=1)
    magnitude_centered = magnitude - magnitude.mean()
    magnitude_rms = np.sqrt(np.mean(magnitude_centered**2))

    result = np.empty(FEATURE_COUNT, dtype=np.float64)
    result[0:3] = np.sqrt(np.mean(accel_centered**2, axis=0))
    result[3] = magnitude_rms
    result[4] = np.max(np.abs(magnitude_centered))
    result[5] = result[4] / (magnitude_rms + 1.0e-12)
    result[6] = np.mean(magnitude_centered**4) / (magnitude_rms**4 + 1.0e-12)
    delta = np.diff(accel_centered, axis=0)
    result[7] = np.sqrt(np.mean(np.sum(delta**2, axis=1)))
    result[8:11] = np.sqrt(np.mean(gyro**2, axis=0))
    result[11] = np.max(np.linalg.norm(gyro, axis=1))

    scale = 1.0 / float(WINDOW_SAMPLES * WINDOW_SAMPLES)
    for band, coefficient in enumerate(GOERTZEL_COEFFICIENTS):
        s1 = 0.0
        s2 = 0.0
        for value in magnitude_centered:
            s0 = value + coefficient * s1 - s2
            s2, s1 = s1, s0
        power = s1 * s1 + s2 * s2 - coefficient * s1 * s2
        result[12 + band] = max(power, 0.0) * scale

    result[24] = temperature[0] / 1000.0
    result[25] = temperature[1] / 1000.0

    return result


def format_c_array(name: str, values: np.ndarray, size_expression: str) -> str:
    flattened = np.asarray(values, dtype=np.float64).reshape(-1)
    lines = []
    for start in range(0, flattened.size, 6):
        chunk = ", ".join(f"{value:.9e}f" for value in flattened[start : start + 6])
        lines.append(f"    {chunk}")
    return f"static const float {name}[{size_expression}] =\n{{\n" + ",\n".join(lines) + "\n};\n"


def export_header(path: Path, scaler: StandardScaler, model: MLPClassifier) -> None:
    hidden = model.coefs_[0].shape[1]
    if not np.array_equal(model.classes_, np.arange(len(CLASS_NAMES))):
        raise ValueError(f"Expected classes 0..4, got {model.classes_.tolist()}")

    content = [
        "#ifndef VIBRATION_MODEL_DATA_H\n#define VIBRATION_MODEL_DATA_H\n\n",
        "/* Generated by tools/train_vibration_model.py. */\n",
        "#define VIBRATION_MODEL_TRAINED  (1)\n",
        f"#define VIBRATION_MODEL_INPUTS   ({FEATURE_COUNT})\n",
        f"#define VIBRATION_MODEL_HIDDEN   ({hidden})\n",
        f"#define VIBRATION_MODEL_OUTPUTS  ({len(CLASS_NAMES)})\n\n",
        format_c_array("g_vibration_input_mean", scaler.mean_, "VIBRATION_MODEL_INPUTS"),
        format_c_array("g_vibration_input_scale", scaler.scale_, "VIBRATION_MODEL_INPUTS"),
        format_c_array(
            "g_vibration_hidden_weights",
            model.coefs_[0],
            "VIBRATION_MODEL_INPUTS * VIBRATION_MODEL_HIDDEN",
        ),
        format_c_array("g_vibration_hidden_bias", model.intercepts_[0], "VIBRATION_MODEL_HIDDEN"),
        format_c_array(
            "g_vibration_output_weights",
            model.coefs_[1],
            "VIBRATION_MODEL_HIDDEN * VIBRATION_MODEL_OUTPUTS",
        ),
        format_c_array("g_vibration_output_bias", model.intercepts_[1], "VIBRATION_MODEL_OUTPUTS"),
        "\n#endif /* VIBRATION_MODEL_DATA_H */\n",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(content), encoding="utf-8", newline="\n")


def main() -> None:
    default_output = Path(__file__).resolve().parents[1] / "src" / "vibration" / "vibration_model_data.h"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path, help="UART log files containing DATA lines")
    parser.add_argument("--output", type=Path, default=default_output, help="generated C header")
    parser.add_argument("--hidden", type=int, default=12, help="hidden neurons (default: 12)")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    raw_windows, temperatures, labels = parse_logs(args.logs)
    counts = np.bincount(labels, minlength=len(CLASS_NAMES))
    missing = [CLASS_NAMES[index] for index, count in enumerate(counts) if count == 0]
    if missing:
        raise ValueError(f"Missing classes: {', '.join(missing)}")
    if np.min(counts) < 5:
        raise ValueError(f"Need at least 5 complete windows per class; counts={counts.tolist()}")

    features = np.stack(
        [extract_features(window, temperature) for window, temperature in zip(raw_windows, temperatures)]
    )
    x_train, x_test, y_train, y_test = train_test_split(
        features,
        labels,
        test_size=0.25,
        random_state=args.seed,
        stratify=labels,
    )
    scaler = StandardScaler().fit(x_train)
    model = MLPClassifier(
        hidden_layer_sizes=(args.hidden,),
        activation="relu",
        solver="lbfgs",
        alpha=1.0e-3,
        max_iter=3000,
        random_state=args.seed,
    )
    model.fit(scaler.transform(x_train), y_train)

    predictions = model.predict(scaler.transform(x_test))
    print("windows per class:", dict(zip(CLASS_NAMES, counts.tolist())))
    print(confusion_matrix(y_test, predictions, labels=np.arange(len(CLASS_NAMES))))
    print(classification_report(y_test, predictions, target_names=CLASS_NAMES, zero_division=0))
    export_header(args.output, scaler, model)
    print(f"exported: {args.output}")


if __name__ == "__main__":
    main()
