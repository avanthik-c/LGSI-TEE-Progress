import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import platform

# Use relative paths so it works anywhere
df = pd.read_csv("results/combined.csv")
# FILTER: Keep only the keygen data!
df = df[df.operation == "keygen"]
ops = ["keygen"]

# CHANGED: These must match EXACTLY what is in your combined.csv
raw_libs = ["mlkem-native-portable", "mlkem-native-avx2", "PQClean"]
# We will use this dictionary to map the raw names to pretty display names on the chart
display_names = {
    "mlkem-native-portable": "mlkem-native (C)",
    "mlkem-native-avx2": "mlkem-native (Assembly)",
    "PQClean": "PQClean"
}
colors = {
    "mlkem-native-portable": "#F4A261", # Orange
    "mlkem-native-avx2": "#2E86AB",     # Blue
    "PQClean": "#E76F51"                # Red
}

ARCH = platform.machine()

# --- Chart 1: grouped bar chart of median timing (Keygen Only) ---
fig, ax = plt.subplots(figsize=(6, 5))
x = np.arange(len(ops))
width = 0.25 # Made thinner to fit 3 bars side-by-side

for i, lib in enumerate(raw_libs):
    # Offset calculates to: -0.25, 0, +0.25
    offset = (i - 1) * width
    subset = df[(df.library == lib) & (df.operation == "keygen")]
    
    if subset.empty:
        continue
        
    med = subset.microseconds.median()
    p99 = subset.microseconds.quantile(0.99)
    
    # Use display_names for the legend label
    ax.bar(x + offset, med, width, label=display_names[lib], color=colors[lib])
    
    # error bar showing p99 as upper whisker
    err = p99 - med
    ax.errorbar(x + offset, med, yerr=[[0], [err]],
                fmt='none', ecolor='black', capsize=4, linewidth=1)
    
    ax.text(x + offset, med + 0.5, f"{med:.1f}", ha='center', va='bottom', fontsize=9)

ax.set_xticks(x)
ax.set_xticklabels(["Keygen"])
ax.set_ylabel("Time (microseconds, median, N=5000)")
ax.set_title(f"ML-KEM-512 Keygen: C vs AVX2 vs PQClean\n({ARCH} host build)")
ax.legend()
ax.grid(axis='y', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("results/timing_comparison.png", dpi=150)
plt.close()

# --- Chart 2: code size comparison ---
sizes = {
    "mlkem-native-portable": 45299, 
    "mlkem-native-avx2": 48000, # NOTE: Update with your real AVX2 size!
    "PQClean": 56031
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
    bp = ax.boxplot(data, labels=[display_names[lib] for lib in active_libs], showfliers=False, patch_artist=True)
    for patch, lib in zip(bp['boxes'], active_libs):
        patch.set_facecolor(colors[lib])
        patch.set_alpha(0.7)
        
ax.set_ylabel("microseconds")
ax.grid(axis='y', linestyle='--', alpha=0.5)
plt.title(f"Keygen Timing Distribution (N=5000, outliers hidden)\n({ARCH} host build)")
plt.tight_layout()
plt.savefig("results/distribution_boxplots.png", dpi=150)
plt.close()

print(f"Charts saved. Detected architecture: {ARCH}")
