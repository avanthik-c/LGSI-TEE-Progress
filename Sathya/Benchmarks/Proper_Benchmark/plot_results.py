import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import platform
import subprocess
import os

# Grab the architecture directly from the system
ARCH = platform.machine()
# Dynamically create the library name expected in the CSV (e.g., "mlkem-native-x86_64", "mlkem-native-aarch64")
native_lib = f"mlkem-native-{ARCH}"
if(ARCH == 'arm64'):
    native_lib = "mlkem-native-aarch64"
# Use relative paths so it works anywhere
df = pd.read_csv("results/combined.csv", header=None, names=["library", "operation", "run_id", "microseconds"])
sf = pd.read_csv("results/combined_stack.csv",header=None,names=["library","operation","iter","stack_bytes"])
# FILTER: Keep only the keygen data!
df = df[df.operation == "keygen"]
sf = sf[sf.operation == "keygen"]
ops = ["keygen"]

raw_libs = ["mlkem-native-portable", native_lib, "PQClean"]

display_names = {
    "mlkem-native-portable": "mlkem-native (C)",
    native_lib: "mlkem-native (Assembly)",
    "PQClean": "PQClean"
}

colors = {
    "mlkem-native-portable": "#F4A261", 
    native_lib: "#2E86AB",   
    "PQClean": "#E76F51"                
}

# Shared styling so every bar chart looks the same:
# legend always in the same place (below the plot, never covering bars) and
# value labels drawn inside the top of each bar (never colliding with the p99 whisker).
def place_legend(ax):
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.04), ncol=3, frameon=False, fontsize=9)

def label_bar(ax, x_pos, value, text):
    ax.annotate(text, xy=(x_pos, value), xytext=(0, -4), textcoords="offset points",
                ha="center", va="top", fontsize=9, color="white", fontweight="bold")

# --- Chart 1: grouped bar chart of median timing (Keygen Only) ---
fig, ax = plt.subplots(figsize=(6, 5))
x = np.arange(len(ops))
width = 0.25 

for i, lib in enumerate(raw_libs):
    offset = (i - 1) * width
    subset = df[(df.library == lib) & (df.operation == "keygen")]
    
    if subset.empty:
        continue
        
    med = subset.microseconds.median()
    p99 = subset.microseconds.quantile(0.99)
    
    ax.bar(x + offset, med, width, label=display_names[lib], color=colors[lib])
    
    err = p99 - med
    ax.errorbar(x + offset, med, yerr=[[0], [err]],
                fmt='none', ecolor='black', capsize=4, linewidth=1)
    label_bar(ax, (x + offset)[0], med, f"{med:.1f}")

ax.set_xticks([])  # one group only; the title already says Keygen
ax.set_ylabel("Time (microseconds, median, N=50000)")
ax.set_title(f"ML-KEM-512 Keygen: C vs Assembly vs PQClean\n({ARCH} host build)")
place_legend(ax)
ax.grid(axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("results/timing_comparison.png", dpi=150)
plt.close()

# --- Chart 2: code size comparison ---
def get_text_size(filepath):
    if not os.path.exists(filepath):
        print(f"Warning: Could not find {filepath}")
        return 0
    try:
        result = subprocess.run(['size', filepath], capture_output=True, text=True, check=True)
        lines = result.stdout.strip().split('\n')
        if len(lines) > 1:
            return int(lines[1].split()[0])
    except Exception as e:
        print(f"Error getting size for {filepath}: {e}")
        return 0
    return 0

sizes = {
    "mlkem-native-portable": get_text_size("mlkem_native_bench/bench_mlkem_c"), 
    native_lib: get_text_size("mlkem_native_bench/bench_mlkem_native"), 
    "PQClean": get_text_size("pqclean_bench/bench_pqclean")
} 

fig, ax = plt.subplots(figsize=(7, 4))
bars = ax.bar([display_names[k] for k in sizes.keys()], sizes.values(), color=[colors[k] for k in sizes])
for bar, v in zip(bars, sizes.values()):
    ax.text(bar.get_x() + bar.get_width()/2, v + 500, f"{v:,} B", ha='center', fontsize=10)
ax.set_ylabel(".text size (bytes)")
ax.set_title(f"Compiled Code Size (.text segment)\nO3, gcc, {ARCH} host build")
ax.grid(axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("results/codesize_comparison.png", dpi=150)
plt.close()

# --- Chart 3: distribution box plot (Keygen Only) ---
fig, ax = plt.subplots(figsize=(7, 4.5))
active_libs = [lib for lib in raw_libs if not df[df.library == lib].empty]
data = [df[df.library == lib].microseconds.values for lib in active_libs]

if data:
    bp = ax.boxplot(data, tick_labels=[display_names[lib] for lib in active_libs], showfliers=False, patch_artist=True)
    for patch, lib in zip(bp['boxes'], active_libs):
        patch.set_facecolor(colors[lib])
        patch.set_alpha(0.7)
        
ax.set_ylabel("microseconds")
ax.grid(axis='y', linestyle='--', alpha=0.5)
plt.title(f"Keygen Timing Distribution (N=50000, outliers hidden)\n({ARCH} host build)")
plt.tight_layout()
plt.savefig("results/distribution_boxplots.png", dpi=150)
plt.close()

# --- Chart 4: grouped bar chart of median stack usage (Keygen Only) ---
fig, ax = plt.subplots(figsize=(6, 5))
x = np.arange(len(ops))
width = 0.25

for i, lib in enumerate(raw_libs):
    offset = (i - 1) * width
    subset = sf[(sf.library == lib) & (sf.operation == "keygen")]
    if subset.empty:
        continue
    med = subset.stack_bytes.median()
    p99 = subset.stack_bytes.quantile(0.99)
    ax.bar(x + offset, med, width, label=display_names[lib], color=colors[lib])
    err = p99 - med
    ax.errorbar(x + offset, med, yerr=[[0], [err]],
                fmt='none', ecolor='black', capsize=4, linewidth=1)
    label_bar(ax, (x + offset)[0], med, f"{med:.0f} B")

ax.set_xticks([])  # one group only; the title already says Keygen
ax.set_ylabel("Stack usage (bytes, median, N=50000)")
ax.set_title(f"ML-KEM-512 Keygen: C vs Assembly vs PQClean\nPeak Stack Depth ({ARCH} host build)")
place_legend(ax)
ax.grid(axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("results/stack_comparison.png", dpi=150)
plt.close()

print(f"Charts saved. Detected architecture: {ARCH}")