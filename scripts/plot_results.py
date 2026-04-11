#!/usr/bin/env python3
"""
Plot experimental results for Gossip-Based Failure Detection with CAN-Assisted Recovery.
Reads results/experiment_results.csv and generates 7 comparison graphs.

Run from project root:
    python scripts/plot_results.py
"""

import os
import sys
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# ---------------------------------------------------------------------------
# Output directory
# ---------------------------------------------------------------------------
OUT_DIR = "docs/report_figures"
os.makedirs(OUT_DIR, exist_ok=True)

# ---------------------------------------------------------------------------
# Style
# ---------------------------------------------------------------------------
try:
    plt.style.use("seaborn-v0_8-whitegrid")
except OSError:
    try:
        plt.style.use("seaborn-whitegrid")
    except OSError:
        plt.style.use("ggplot")

matplotlib.rcParams.update({
    "axes.titlesize":  14,
    "axes.titleweight": "bold",
    "axes.labelsize":  12,
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "legend.fontsize": 10,
    "figure.dpi":      100,
    "savefig.dpi":     300,
    "font.family":     "DejaVu Sans",
})

COLORS  = {"Hybrid": "#2ecc71", "PureGossip": "#e74c3c", "PureCAN": "#3498db"}
MARKERS = {"Hybrid": "o",       "PureGossip": "s",        "PureCAN": "^"}
LABELS  = {"Hybrid": "Hybrid (Ours)", "PureGossip": "Pure Gossip", "PureCAN": "Pure CAN"}
LW_MAIN = 2.5
LW_BASE = 2.0
SYSTEMS = ["PureGossip", "PureCAN", "Hybrid"]   # draw order: Hybrid on top

NODE_COUNTS = [10, 25, 50, 100, 200]

# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_data() -> pd.DataFrame:
    csv_path = "results/experiment_results.csv"
    if not os.path.exists(csv_path):
        print(f"ERROR: CSV not found at '{csv_path}'.")
        print("Run the C++ simulator first to generate results.")
        sys.exit(1)
    df = pd.read_csv(csv_path)
    if df.empty:
        print("ERROR: CSV is empty.")
        sys.exit(1)
    # Replace -1 sentinel (no detection / no recovery) with NaN.
    for col in ["detection_latency_ms", "recovery_time_ms"]:
        df[col] = df[col].replace(-1.0, np.nan)
    return df


def mean_std(df: pd.DataFrame, groupby: list, col: str):
    """Return (mean_series, std_series) indexed by groupby keys."""
    g = df.groupby(groupby)[col]
    return g.mean(), g.std().fillna(0)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def save(fig: plt.Figure, name: str):
    base = os.path.join(OUT_DIR, name)
    fig.savefig(base + ".png", bbox_inches="tight")
    fig.savefig(base + ".pdf", bbox_inches="tight")
    plt.close(fig)
    print(f"  saved -> {base}.png / .pdf")


def line_kwargs(system: str, main: bool = True) -> dict:
    return {
        "color":     COLORS[system],
        "marker":    MARKERS[system],
        "linewidth": LW_MAIN if main else LW_BASE,
        "markersize": 7 if main else 6,
        "label":     LABELS[system],
    }


def add_shading(ax, x, means, stds, system):
    ax.fill_between(
        x,
        means - stds,
        means + stds,
        alpha=0.15,
        color=COLORS[system],
    )


# ---------------------------------------------------------------------------
# Graph 1 — Recovery Messages vs N (THE KEY GRAPH)
# ---------------------------------------------------------------------------

def plot_recovery_messages(df: pd.DataFrame):
    print("Generating: Recovery Messages vs N ...", end=" ", flush=True)
    mean_all, std_all = mean_std(df, ["system", "node_count"], "recovery_messages")

    fig, ax = plt.subplots(figsize=(8, 5))

    for sys in SYSTEMS:
        means = mean_all[sys].reindex(NODE_COUNTS)
        stds  = std_all[sys].reindex(NODE_COUNTS)
        ax.plot(NODE_COUNTS, means, **line_kwargs(sys))
        add_shading(ax, NODE_COUNTS, means.values, stds.values, sys)

    # Annotate PureGossip at N=200
    pg_val = mean_all["PureGossip"][200]
    ax.annotate(
        f"{pg_val:.0f} msgs",
        xy=(200, pg_val),
        xytext=(-60, 12),
        textcoords="offset points",
        fontsize=9,
        color=COLORS["PureGossip"],
        arrowprops=dict(arrowstyle="->", color=COLORS["PureGossip"], lw=1.2),
    )

    ax.set_xlabel("Network Size (nodes)")
    ax.set_ylabel("Avg Recovery Messages")
    ax.set_title("Recovery Message Overhead vs Network Size")
    ax.set_xticks(NODE_COUNTS)
    ax.legend()
    ax.set_xlim(5, 210)

    save(fig, "recovery_messages_vs_N")


# ---------------------------------------------------------------------------
# Graph 2 — Detection Latency vs N
# ---------------------------------------------------------------------------

def plot_detection_latency(df: pd.DataFrame):
    print("Generating: Detection Latency vs N ...", end=" ", flush=True)
    mean_all, std_all = mean_std(df, ["system", "node_count"], "detection_latency_ms")

    fig, ax = plt.subplots(figsize=(8, 5))

    for sys in SYSTEMS:
        means = mean_all[sys].reindex(NODE_COUNTS)
        stds  = std_all[sys].reindex(NODE_COUNTS)
        ax.plot(NODE_COUNTS, means, **line_kwargs(sys))
        add_shading(ax, NODE_COUNTS, means.values, stds.values, sys)

    ax.set_xlabel("Network Size (nodes)")
    ax.set_ylabel("Avg Detection Latency (ms)")
    ax.set_title("Failure Detection Latency vs Network Size")
    ax.set_xticks(NODE_COUNTS)
    ax.legend()
    ax.set_xlim(5, 210)

    save(fig, "detection_latency_vs_N")


# ---------------------------------------------------------------------------
# Graph 3 — Recovery Time vs N (with error bars)
# ---------------------------------------------------------------------------

def plot_recovery_time(df: pd.DataFrame):
    print("Generating: Recovery Time vs N ...", end=" ", flush=True)
    mean_all, std_all = mean_std(df, ["system", "node_count"], "recovery_time_ms")

    fig, ax = plt.subplots(figsize=(8, 5))

    for sys in SYSTEMS:
        means = mean_all[sys].reindex(NODE_COUNTS).values
        stds  = std_all[sys].reindex(NODE_COUNTS).values
        ax.errorbar(
            NODE_COUNTS,
            means,
            yerr=stds,
            capsize=4,
            **line_kwargs(sys),
        )

    ax.set_xlabel("Network Size (nodes)")
    ax.set_ylabel("Avg Recovery Time (ms)")
    ax.set_title("Recovery Convergence Time vs Network Size")
    ax.set_xticks(NODE_COUNTS)
    ax.legend()
    ax.set_xlim(5, 210)

    save(fig, "recovery_time_vs_N")


# ---------------------------------------------------------------------------
# Graph 4 — Total Messages vs N (log Y)
# ---------------------------------------------------------------------------

def plot_total_messages(df: pd.DataFrame):
    print("Generating: Total Messages vs N ...", end=" ", flush=True)
    mean_all, std_all = mean_std(df, ["system", "node_count"], "total_messages")

    fig, ax = plt.subplots(figsize=(8, 5))

    for sys in SYSTEMS:
        means = mean_all[sys].reindex(NODE_COUNTS).values
        ax.plot(NODE_COUNTS, means, **line_kwargs(sys))

    ax.set_yscale("log")
    ax.set_xlabel("Network Size (nodes)")
    ax.set_ylabel("Avg Total Messages (log scale)")
    ax.set_title("Total Message Overhead vs Network Size (Log Scale)")
    ax.set_xticks(NODE_COUNTS)
    ax.legend()
    ax.set_xlim(5, 210)

    save(fig, "total_messages_vs_N")


# ---------------------------------------------------------------------------
# Graph 5 — Recovery Messages by Scenario (grouped bars at N=100)
# ---------------------------------------------------------------------------

def plot_recovery_by_scenario(df: pd.DataFrame):
    print("Generating: Recovery by Scenario ...", end=" ", flush=True)

    n100 = df[df["node_count"] == 100]
    scenarios = ["single", "dual", "cascading"]
    scenario_labels = ["Single Failure", "Dual Failure", "Cascading Failure"]

    means = (
        n100.groupby(["scenario", "system"])["recovery_messages"]
        .mean()
        .unstack("system")
    )

    x = np.arange(len(scenarios))
    width = 0.25
    offsets = {"PureGossip": -width, "PureCAN": 0, "Hybrid": width}

    fig, ax = plt.subplots(figsize=(9, 5))

    for sys in SYSTEMS:
        vals = [means.loc[sc, sys] if sc in means.index else 0
                for sc in scenarios]
        bars = ax.bar(
            x + offsets[sys],
            vals,
            width,
            label=LABELS[sys],
            color=COLORS[sys],
            edgecolor="white",
            linewidth=0.8,
        )
        # Value labels on bars
        for bar, v in zip(bars, vals):
            if v > 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + 1,
                    f"{v:.0f}",
                    ha="center",
                    va="bottom",
                    fontsize=8,
                )

    ax.set_xlabel("Failure Scenario")
    ax.set_ylabel("Avg Recovery Messages")
    ax.set_title("Recovery Messages by Failure Scenario (N=100)")
    ax.set_xticks(x)
    ax.set_xticklabels(scenario_labels)
    ax.legend()

    save(fig, "recovery_by_scenario")


# ---------------------------------------------------------------------------
# Graph 6 — 2×2 Scalability Dashboard
# ---------------------------------------------------------------------------

def plot_scalability_dashboard(df: pd.DataFrame):
    print("Generating: Scalability Dashboard ...", end=" ", flush=True)

    mean_rec_msg, _ = mean_std(df, ["system", "node_count"], "recovery_messages")
    mean_det_lat, _ = mean_std(df, ["system", "node_count"], "detection_latency_ms")
    mean_rec_time, std_rec_time = mean_std(df, ["system", "node_count"], "recovery_time_ms")
    mean_tot_msg, _ = mean_std(df, ["system", "node_count"], "total_messages")

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle("Scalability Analysis: Hybrid vs Baseline Systems",
                 fontsize=16, fontweight="bold", y=1.01)

    panels = [
        (axes[0, 0], mean_rec_msg, None,          "Recovery Messages",         "Avg Recovery Messages",        False),
        (axes[0, 1], mean_det_lat, None,          "Detection Latency (ms)",    "Avg Detection Latency (ms)",   False),
        (axes[1, 0], mean_rec_time, std_rec_time, "Recovery Time (ms)",        "Avg Recovery Time (ms)",       False),
        (axes[1, 1], mean_tot_msg, None,          "Total Messages (log scale)","Avg Total Messages",           True),
    ]

    handles = []  # for shared legend
    for ax, means, stds, title, ylabel, logscale in panels:
        first = True
        for sys in SYSTEMS:
            m = means[sys].reindex(NODE_COUNTS).values
            kw = line_kwargs(sys, main=False)
            if stds is not None:
                s = stds[sys].reindex(NODE_COUNTS).values
                h = ax.errorbar(NODE_COUNTS, m, yerr=s, capsize=3, **kw)
            else:
                h, = ax.plot(NODE_COUNTS, m, **kw)
            if first or True:
                handles.append(h)
            first = False
        if logscale:
            ax.set_yscale("log")
        ax.set_title(title, fontsize=12, fontweight="bold")
        ax.set_xlabel("Network Size (nodes)", fontsize=10)
        ax.set_ylabel(ylabel, fontsize=10)
        ax.set_xticks(NODE_COUNTS)
        ax.tick_params(labelsize=9)

    # Build legend patches (one per system — deduplicate handles).
    legend_handles = [
        mpatches.Patch(color=COLORS[s], label=LABELS[s]) for s in SYSTEMS
    ]
    fig.legend(
        handles=legend_handles,
        loc="lower center",
        ncol=3,
        fontsize=11,
        bbox_to_anchor=(0.5, -0.03),
        frameon=True,
    )

    fig.tight_layout()
    save(fig, "scalability_dashboard")


# ---------------------------------------------------------------------------
# Graph 7 — Stacked bar: Detection vs Recovery messages at N=100
# ---------------------------------------------------------------------------

def plot_message_breakdown(df: pd.DataFrame):
    print("Generating: Message Breakdown ...", end=" ", flush=True)

    n100 = df[df["node_count"] == 100]
    means = n100.groupby("system")[["recovery_messages", "total_messages"]].mean()
    # Detection messages = total - recovery
    means["detection_messages"] = (means["total_messages"] - means["recovery_messages"]).clip(lower=0)

    systems_ordered = ["PureGossip", "PureCAN", "Hybrid"]
    det_vals = [means.loc[s, "detection_messages"] for s in systems_ordered]
    rec_vals = [means.loc[s, "recovery_messages"]  for s in systems_ordered]
    xlabels  = [LABELS[s] for s in systems_ordered]

    x = np.arange(len(systems_ordered))
    width = 0.45

    fig, ax = plt.subplots(figsize=(8, 5))

    # Detection messages (lighter shade = 60% alpha via hex lighten)
    det_colors = [matplotlib.colors.to_rgba(COLORS[s], alpha=0.5) for s in systems_ordered]
    rec_colors = [matplotlib.colors.to_rgba(COLORS[s], alpha=1.0) for s in systems_ordered]

    bars_det = ax.bar(x, det_vals, width, color=det_colors, edgecolor="white", linewidth=0.8,
                      label="Detection Messages")
    bars_rec = ax.bar(x, rec_vals, width, bottom=det_vals, color=rec_colors,
                      edgecolor="white", linewidth=0.8, label="Recovery Messages")

    # Value labels
    for i, (d, r) in enumerate(zip(det_vals, rec_vals)):
        total = d + r
        ax.text(x[i], total + total * 0.02, f"{total:.0f}", ha="center", va="bottom", fontsize=9)
        # Recovery sub-label inside bar if tall enough
        if r > total * 0.08:
            ax.text(x[i], d + r / 2, f"R:{r:.0f}", ha="center", va="center",
                    fontsize=8, color="white", fontweight="bold")

    ax.set_xlabel("System")
    ax.set_ylabel("Message Count")
    ax.set_title("Message Composition: Detection vs Recovery (N=100)")
    ax.set_xticks(x)
    ax.set_xticklabels(xlabels)

    # Custom legend: one entry per shade
    det_patch = mpatches.Patch(facecolor="grey", alpha=0.5, label="Detection Messages")
    rec_patch = mpatches.Patch(facecolor="grey", alpha=1.0, label="Recovery Messages")
    ax.legend(handles=[det_patch, rec_patch])

    save(fig, "message_breakdown_stacked")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    df = load_data()
    print(f"Loaded {len(df)} experiment results\n")

    plot_recovery_messages(df)
    plot_detection_latency(df)
    plot_recovery_time(df)
    plot_total_messages(df)
    plot_recovery_by_scenario(df)
    plot_scalability_dashboard(df)
    plot_message_breakdown(df)

    print(f"\nAll 7 graphs saved to {OUT_DIR}/")


if __name__ == "__main__":
    main()
