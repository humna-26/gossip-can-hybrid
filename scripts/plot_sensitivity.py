#!/usr/bin/env python3
"""
Sensitivity analysis plots for Gossip-CAN-Hybrid simulator (Phase 6A).
Reads results/sensitivity_*.csv and generates 5 comparison graphs.

Run from project root:
    python scripts/plot_sensitivity.py
"""

import os
import sys
import math
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
# Style (matches plot_results.py)
# ---------------------------------------------------------------------------
try:
    plt.style.use("seaborn-v0_8-whitegrid")
except OSError:
    try:
        plt.style.use("seaborn-whitegrid")
    except OSError:
        plt.style.use("ggplot")

matplotlib.rcParams.update({
    "axes.titlesize":   14,
    "axes.titleweight": "bold",
    "axes.labelsize":   12,
    "xtick.labelsize":  10,
    "ytick.labelsize":  10,
    "legend.fontsize":  10,
    "figure.dpi":       100,
    "savefig.dpi":      150,
    "font.family":      "DejaVu Sans",
})

COLORS  = {"Hybrid": "#2ecc71", "PureGossip": "#e74c3c", "PureCAN": "#3498db"}
MARKERS = {"Hybrid": "o",       "PureGossip": "s",        "PureCAN": "^"}
LABELS  = {"Hybrid": "Hybrid (Ours)", "PureGossip": "Pure Gossip", "PureCAN": "Pure CAN"}
SYSTEMS = ["PureGossip", "PureCAN", "Hybrid"]

C_GREEN  = "#2ecc71"
C_ORANGE = "#e67e22"
C_RED    = "#e74c3c"

LW = 2.5


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load_csv(name: str) -> pd.DataFrame:
    path = f"results/{name}"
    if not os.path.exists(path):
        print(f"ERROR: CSV not found: '{path}'")
        print("Run './gossip_can_hybrid --sensitivity' first.")
        sys.exit(1)
    df = pd.read_csv(path)
    # Replace -1 sentinel with NaN for optional metrics.
    for col in df.columns:
        if "latency" in col or "time" in col:
            df[col] = df[col].replace(-1.0, np.nan)
    return df


def save(fig: plt.Figure, name: str):
    base = os.path.join(OUT_DIR, name)
    fig.savefig(base + ".png", bbox_inches="tight")
    fig.savefig(base + ".pdf", bbox_inches="tight")
    plt.close(fig)
    print(f"  {name}.png ... saved")


# ---------------------------------------------------------------------------
# Graph S1: sensitivity_fanout.png
# Effect of Gossip Fan-out (b) on Detection Performance
# Dual Y-axis: Detection Latency (left, green) + Total Messages (right, orange)
# ---------------------------------------------------------------------------

def plot_fanout():
    df = load_csv("sensitivity_fanout.csv")
    grp = df.groupby("fanout").agg(
        det_lat_mean=("detection_latency_ms", "mean"),
        tot_msg_mean=("total_messages",       "mean"),
    ).reset_index()

    fanouts = grp["fanout"].tolist()

    fig, ax1 = plt.subplots(figsize=(8, 5))
    ax2 = ax1.twinx()

    l1, = ax1.plot(fanouts, grp["det_lat_mean"], color=C_GREEN,  marker="o",
                   linewidth=LW, markersize=7, label="Detection Latency (ms)")
    l2, = ax2.plot(fanouts, grp["tot_msg_mean"], color=C_ORANGE, marker="s",
                   linewidth=LW, markersize=7, linestyle="--", label="Total Messages")

    ax1.set_xlabel("Gossip Fan-out (b)")
    ax1.set_ylabel("Avg Detection Latency (ms)", color=C_GREEN)
    ax2.set_ylabel("Avg Total Messages",         color=C_ORANGE)
    ax1.tick_params(axis="y", labelcolor=C_GREEN)
    ax2.tick_params(axis="y", labelcolor=C_ORANGE)
    ax1.set_xticks(fanouts)

    ax1.set_title("Effect of Gossip Fan-out (b) on Detection Performance")
    ax1.grid(True, alpha=0.4)
    ax1.legend(handles=[l1, l2], loc="upper right")

    fig.tight_layout()
    save(fig, "sensitivity_fanout")


# ---------------------------------------------------------------------------
# Graph S2: sensitivity_timeout_tradeoff.png  ⭐ KEY GRAPH
# Speed vs Accuracy tradeoff
# Dual Y-axis: Detection Latency (left, green) + False Positive Rate % (right, red)
# Vertical dashed line at T_fail = 4000 ms (baseline)
# ---------------------------------------------------------------------------

def plot_timeout_tradeoff():
    df = load_csv("sensitivity_timeout.csv")
    grp = df.groupby("fail_timeout_ms").agg(
        det_lat_mean=("detection_latency_ms", "mean"),
        fpr_mean    =("false_positive_rate",  "mean"),
    ).reset_index()

    timeouts = grp["fail_timeout_ms"].tolist()
    fpr_pct  = grp["fpr_mean"] * 100.0

    fig, ax1 = plt.subplots(figsize=(9, 5))
    ax2 = ax1.twinx()

    l1, = ax1.plot(timeouts, grp["det_lat_mean"], color=C_GREEN, marker="o",
                   linewidth=LW, markersize=7, label="Detection Latency (ms)")
    l2, = ax2.plot(timeouts, fpr_pct, color=C_RED, marker="^",
                   linewidth=LW, markersize=7, linestyle="--",
                   label="False Positive Rate (%)")

    # Baseline reference line at T_fail = 4000 ms
    ax1.axvline(x=4000, color="gray", linestyle="--", linewidth=1.5, alpha=0.8)
    ax1.text(4100, ax1.get_ylim()[1] * 0.95 if ax1.get_ylim()[1] > 0 else 100,
             "Baseline\n(4000 ms)", fontsize=9, color="gray", va="top")

    ax1.set_xlabel("Failure Timeout (ms)")
    ax1.set_ylabel("Avg Detection Latency (ms)", color=C_GREEN)
    ax2.set_ylabel("False Positive Rate (%)",    color=C_RED)
    ax1.tick_params(axis="y", labelcolor=C_GREEN)
    ax2.tick_params(axis="y", labelcolor=C_RED)
    ax1.set_xticks(timeouts)

    ax1.set_title("Detection Speed vs Accuracy Tradeoff (THE KEY INSIGHT)")
    ax1.grid(True, alpha=0.4)
    ax1.legend(handles=[l1, l2], loc="upper left")

    fig.tight_layout()
    save(fig, "sensitivity_timeout_tradeoff")


# ---------------------------------------------------------------------------
# Graph S3: sensitivity_scalability_fine.png
# Recovery messages vs N (log scale) for all 3 systems
# + O(N log N) reference line calibrated to PureGossip at N=100
# + horizontal dashed line at Hybrid mean (showing O(d) ≈ constant)
# ---------------------------------------------------------------------------

def plot_scalability_fine():
    df = load_csv("sensitivity_scalability.csv")
    grp = df.groupby(["system", "node_count"])["recovery_messages"].mean().unstack("system")

    node_counts = sorted(df["node_count"].unique())

    fig, ax = plt.subplots(figsize=(9, 6))

    for sys in SYSTEMS:
        if sys not in grp.columns:
            continue
        vals = grp[sys].reindex(node_counts)
        ax.plot(node_counts, vals, color=COLORS[sys], marker=MARKERS[sys],
                linewidth=LW, markersize=7, label=LABELS[sys])

    # O(N log N) reference, calibrated to match PureGossip at N=100
    if "PureGossip" in grp.columns and 100 in grp.index.get_level_values(0) if False else True:
        pg_at_100 = grp["PureGossip"].get(100, None) if "PureGossip" in grp.columns else None
        if pg_at_100 is not None and pg_at_100 > 0:
            ref_at_100 = 100.0 * math.log2(100.0)
            scale = pg_at_100 / ref_at_100
            ref_y = [scale * n * math.log2(max(n, 2)) for n in node_counts]
            ax.plot(node_counts, ref_y, color="gray", linestyle=":", linewidth=1.8,
                    label="O(N log N) reference")

    # Horizontal dashed line at Hybrid's grand mean (O(d) ≈ constant)
    if "Hybrid" in grp.columns:
        hybrid_mean = grp["Hybrid"].reindex(node_counts).mean()
        ax.axhline(y=hybrid_mean, color=COLORS["Hybrid"], linestyle="--",
                   linewidth=1.5, alpha=0.6,
                   label=f"Hybrid mean ≈ {hybrid_mean:.0f} (O(d))")

    ax.set_yscale("log")
    ax.set_xlabel("Network Size (nodes)")
    ax.set_ylabel("Avg Recovery Messages (log scale)")
    ax.set_title("Recovery Message Scalability: Fine-Grained Analysis")
    ax.set_xticks(node_counts)
    ax.tick_params(axis="x", rotation=45)
    ax.legend(loc="upper left")
    ax.grid(True, which="both", alpha=0.3)

    fig.tight_layout()
    save(fig, "sensitivity_scalability_fine")


# ---------------------------------------------------------------------------
# Graph S4: sensitivity_interval.png
# Effect of Gossip Interval on Detection Performance
# Dual Y-axis: Detection Latency (left, green) + Total Messages (right, orange)
# ---------------------------------------------------------------------------

def plot_interval():
    df = load_csv("sensitivity_interval.csv")
    grp = df.groupby("gossip_interval_ms").agg(
        det_lat_mean=("detection_latency_ms", "mean"),
        tot_msg_mean=("total_messages",       "mean"),
    ).reset_index()

    intervals = grp["gossip_interval_ms"].tolist()

    fig, ax1 = plt.subplots(figsize=(8, 5))
    ax2 = ax1.twinx()

    l1, = ax1.plot(intervals, grp["det_lat_mean"], color=C_GREEN,  marker="o",
                   linewidth=LW, markersize=7, label="Detection Latency (ms)")
    l2, = ax2.plot(intervals, grp["tot_msg_mean"], color=C_ORANGE, marker="s",
                   linewidth=LW, markersize=7, linestyle="--", label="Total Messages")

    ax1.set_xlabel("Gossip Interval (ms)")
    ax1.set_ylabel("Avg Detection Latency (ms)", color=C_GREEN)
    ax2.set_ylabel("Avg Total Messages",         color=C_ORANGE)
    ax1.tick_params(axis="y", labelcolor=C_GREEN)
    ax2.tick_params(axis="y", labelcolor=C_ORANGE)
    ax1.set_xticks(intervals)

    ax1.set_title("Effect of Gossip Interval on Detection Performance")
    ax1.grid(True, alpha=0.4)
    ax1.legend(handles=[l1, l2], loc="upper left")

    fig.tight_layout()
    save(fig, "sensitivity_interval")


# ---------------------------------------------------------------------------
# Graph S5: sensitivity_dashboard.png
# 2×2 subplot grid of S1–S4
# ---------------------------------------------------------------------------

def plot_dashboard():
    # Load all datasets.
    df_fanout   = load_csv("sensitivity_fanout.csv")
    df_timeout  = load_csv("sensitivity_timeout.csv")
    df_scale    = load_csv("sensitivity_scalability.csv")
    df_interval = load_csv("sensitivity_interval.csv")

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle("Sensitivity Analysis: Protocol Parameter Impact",
                 fontsize=16, fontweight="bold")

    # --- (1,1): Fan-out effect ---
    ax = axes[0, 0]
    ax2 = ax.twinx()
    grp = df_fanout.groupby("fanout").agg(
        det=("detection_latency_ms", "mean"),
        msg=("total_messages",       "mean"),
    ).reset_index()
    ax.plot(grp["fanout"], grp["det"], color=C_GREEN,  marker="o",
            linewidth=2, markersize=6, label="Detection Latency (ms)")
    ax2.plot(grp["fanout"], grp["msg"], color=C_ORANGE, marker="s",
             linewidth=2, markersize=6, linestyle="--", label="Total Messages")
    ax.set_xlabel("Gossip Fan-out (b)");  ax.set_ylabel("Det. Latency (ms)", color=C_GREEN)
    ax2.set_ylabel("Total Messages", color=C_ORANGE)
    ax.tick_params(axis="y", labelcolor=C_GREEN)
    ax2.tick_params(axis="y", labelcolor=C_ORANGE)
    ax.set_xticks(grp["fanout"].tolist())
    ax.set_title("Fan-out Effect", fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.4)
    handles = [
        matplotlib.lines.Line2D([0], [0], color=C_GREEN,  marker="o", label="Det. Latency"),
        matplotlib.lines.Line2D([0], [0], color=C_ORANGE, marker="s", linestyle="--", label="Total Msgs"),
    ]
    ax.legend(handles=handles, fontsize=8, loc="upper right")

    # --- (1,2): Timeout tradeoff ---
    ax = axes[0, 1]
    ax2 = ax.twinx()
    grp = df_timeout.groupby("fail_timeout_ms").agg(
        det=("detection_latency_ms", "mean"),
        fpr=("false_positive_rate",  "mean"),
    ).reset_index()
    ax.plot(grp["fail_timeout_ms"], grp["det"], color=C_GREEN, marker="o",
            linewidth=2, markersize=6, label="Detection Latency")
    ax2.plot(grp["fail_timeout_ms"], grp["fpr"] * 100, color=C_RED, marker="^",
             linewidth=2, markersize=6, linestyle="--", label="FPR (%)")
    ax.axvline(x=4000, color="gray", linestyle="--", linewidth=1.2, alpha=0.7)
    ax.set_xlabel("Fail Timeout (ms)");  ax.set_ylabel("Det. Latency (ms)", color=C_GREEN)
    ax2.set_ylabel("FPR (%)", color=C_RED)
    ax.tick_params(axis="y", labelcolor=C_GREEN)
    ax2.tick_params(axis="y", labelcolor=C_RED)
    ax.set_title("Timeout Tradeoff", fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.4)
    handles = [
        matplotlib.lines.Line2D([0], [0], color=C_GREEN, marker="o", label="Det. Latency"),
        matplotlib.lines.Line2D([0], [0], color=C_RED,   marker="^", linestyle="--", label="FPR (%)"),
    ]
    ax.legend(handles=handles, fontsize=8, loc="upper left")

    # --- (2,1): Fine-grained scalability ---
    ax = axes[1, 0]
    grp_s = df_scale.groupby(["system", "node_count"])["recovery_messages"].mean().unstack("system")
    ncs   = sorted(df_scale["node_count"].unique())
    for sys in SYSTEMS:
        if sys not in grp_s.columns:
            continue
        vals = grp_s[sys].reindex(ncs)
        ax.plot(ncs, vals, color=COLORS[sys], marker=MARKERS[sys],
                linewidth=2, markersize=5, label=LABELS[sys])
    if "PureGossip" in grp_s.columns:
        pg100 = grp_s["PureGossip"].get(100, None)
        if pg100 is not None and pg100 > 0:
            ref100 = 100.0 * math.log2(100.0)
            scale  = pg100 / ref100
            ref_y  = [scale * n * math.log2(max(n, 2)) for n in ncs]
            ax.plot(ncs, ref_y, color="gray", linestyle=":", linewidth=1.5,
                    label="O(N log N)")
    ax.set_yscale("log")
    ax.set_xlabel("Node Count");  ax.set_ylabel("Recovery Msgs (log)")
    ax.set_title("Fine-Grained Scalability", fontsize=12, fontweight="bold")
    ax.tick_params(axis="x", rotation=45, labelsize=8)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=7, loc="upper left")

    # --- (2,2): Gossip interval effect ---
    ax = axes[1, 1]
    ax2 = ax.twinx()
    grp = df_interval.groupby("gossip_interval_ms").agg(
        det=("detection_latency_ms", "mean"),
        msg=("total_messages",       "mean"),
    ).reset_index()
    ax.plot(grp["gossip_interval_ms"], grp["det"], color=C_GREEN,  marker="o",
            linewidth=2, markersize=6, label="Detection Latency")
    ax2.plot(grp["gossip_interval_ms"], grp["msg"], color=C_ORANGE, marker="s",
             linewidth=2, markersize=6, linestyle="--", label="Total Messages")
    ax.set_xlabel("Gossip Interval (ms)");  ax.set_ylabel("Det. Latency (ms)", color=C_GREEN)
    ax2.set_ylabel("Total Messages", color=C_ORANGE)
    ax.tick_params(axis="y", labelcolor=C_GREEN)
    ax2.tick_params(axis="y", labelcolor=C_ORANGE)
    ax.set_title("Gossip Interval Effect", fontsize=12, fontweight="bold")
    ax.grid(True, alpha=0.4)
    handles = [
        matplotlib.lines.Line2D([0], [0], color=C_GREEN,  marker="o", label="Det. Latency"),
        matplotlib.lines.Line2D([0], [0], color=C_ORANGE, marker="s", linestyle="--", label="Total Msgs"),
    ]
    ax.legend(handles=handles, fontsize=8, loc="upper left")

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    save(fig, "sensitivity_dashboard")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    print("=== Sensitivity Analysis Graphs (Phase 6A) ===\n")
    plot_fanout()
    plot_timeout_tradeoff()
    plot_scalability_fine()
    plot_interval()
    plot_dashboard()
    print(f"\nAll 5 graphs saved to {OUT_DIR}/")


if __name__ == "__main__":
    main()
