#!/usr/bin/env python3
"""Draw erase-count CSV files as comparable block heatmaps."""

import csv
import math
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_counts(path):
    counts = []
    with open(path, newline="") as stream:
        for row in csv.DictReader(stream):
            counts.append(
                math.nan if row["factory_bad"] == "1" else int(row["erase_count"])
            )
    return counts


def as_grid(counts):
    columns = 8 if len(counts) <= 64 else 32
    rows = math.ceil(len(counts) / columns)
    grid = np.full(rows * columns, math.nan)
    grid[: len(counts)] = counts
    return grid.reshape(rows, columns)


def main():
    inputs, output = sys.argv[1:-1], sys.argv[-1]
    counts = [read_counts(path) for path in inputs]
    maximum = max((max((value for value in values if not math.isnan(value)), default=0)
                   for values in counts), default=0)
    figure, axes = plt.subplots(1, len(inputs), squeeze=False)
    for index, values in enumerate(counts):
        axes[0][index].imshow(as_grid(values), cmap="magma", vmin=0, vmax=maximum)
        good = [value for value in values if not math.isnan(value)]
        axes[0][index].set_title(
            f"{inputs[index]}\n{min(good, default=0)} to {max(good, default=0)} erases"
        )
        axes[0][index].set_xticks([])
        axes[0][index].set_yticks([])
    figure.colorbar(axes[0][0].images[0], ax=axes.ravel().tolist())
    figure.savefig(output, dpi=150, bbox_inches="tight")


if __name__ == "__main__":
    main()
