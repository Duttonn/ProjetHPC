#!/usr/bin/env python3
"""
Script pour générer les graphes de présentation
Nécessite: pip install matplotlib pandas
"""

import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# Configuration matplotlib pour un style professionnel
plt.style.use('seaborn-v0_8-darkgrid')
plt.rcParams['figure.figsize'] = (10, 6)
plt.rcParams['font.size'] = 12

def graph1_openmp_scaling():
    """Graphe 1: Scaling OpenMP (CM4)"""
    df = pd.read_csv('graph_data_openmp_scaling.csv')

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    # Sous-graphe 1: FPS vs Threads
    ax1.plot(df['Threads'], df['AvgFPS'], 'o-', linewidth=2, markersize=8, color='#2E86AB')
    ax1.set_xlabel('Nombre de Threads', fontsize=14, fontweight='bold')
    ax1.set_ylabel('FPS Moyen', fontsize=14, fontweight='bold')
    ax1.set_title('Performance vs Threads (CM4 OpenMP)', fontsize=16, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.set_xticks(df['Threads'])

    # Ajouter les valeurs sur les points
    for x, y in zip(df['Threads'], df['AvgFPS']):
        ax1.annotate(f'{y:.0f}', (x, y), textcoords="offset points",
                    xytext=(0,10), ha='center', fontsize=10)

    # Sous-graphe 2: Speedup vs Threads
    ax2.plot(df['Threads'], df['Speedup'], 'o-', linewidth=2, markersize=8, color='#A23B72')
    ax2.axhline(y=1.0, color='red', linestyle='--', label='Baseline (1 thread)', alpha=0.5)

    # Speedup linéaire idéal
    ideal = df['Threads'] / df['Threads'].iloc[0]
    ax2.plot(df['Threads'], ideal, 'g--', alpha=0.3, label='Speedup idéal')

    ax2.set_xlabel('Nombre de Threads', fontsize=14, fontweight='bold')
    ax2.set_ylabel('Speedup', fontsize=14, fontweight='bold')
    ax2.set_title('Efficacité du Parallélisme (CM4)', fontsize=16, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.set_xticks(df['Threads'])
    ax2.legend()

    # Ajouter les valeurs sur les points
    for x, y in zip(df['Threads'], df['Speedup']):
        ax2.annotate(f'{y:.2f}×', (x, y), textcoords="offset points",
                    xytext=(0,10), ha='center', fontsize=10)

    plt.tight_layout()
    plt.savefig('graph1_openmp_scaling.png', dpi=300, bbox_inches='tight')
    print("✓ Graphe 1 généré: graph1_openmp_scaling.png")
    plt.close()


def graph2_latencies_comparison():
    """Graphe 2: Comparaison des latences par étape"""
    df = pd.read_csv('graph_data_latencies.csv')

    # Exclure Video_decoding et Total pour la clarté
    df_plot = df[~df['Stage'].isin(['Video_decoding', 'Total'])]

    fig, ax = plt.subplots(figsize=(12, 6))

    x = np.arange(len(df_plot))
    width = 0.35

    bars1 = ax.bar(x - width/2, df_plot['motion2_ms'], width, label='motion2 (référence)',
                   color='#F18F01', alpha=0.8)
    bars2 = ax.bar(x + width/2, df_plot['motion_optimized_ms'], width, label='motion (optimisé)',
                   color='#006E90', alpha=0.8)

    # Ajouter les speedups au-dessus des barres
    for i, (m2, opt, spd) in enumerate(zip(df_plot['motion2_ms'],
                                            df_plot['motion_optimized_ms'],
                                            df_plot['Speedup'])):
        if spd > 1.0:
            color = 'green'
            arrow = '↓'
        else:
            color = 'red'
            arrow = '↑'
        ax.text(i, max(m2, opt) + 0.2, f'{spd:.2f}×{arrow}',
               ha='center', fontsize=10, fontweight='bold', color=color)

    ax.set_xlabel('Étape du Pipeline', fontsize=14, fontweight='bold')
    ax.set_ylabel('Latence (ms)', fontsize=14, fontweight='bold')
    ax.set_title('Comparaison des Latences: motion2 vs motion optimisé (CM2/CM3)',
                fontsize=16, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(df_plot['Stage'].str.replace('_', ' '), rotation=15, ha='right')
    ax.legend(fontsize=12)
    ax.grid(True, axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig('graph2_latencies_comparison.png', dpi=300, bbox_inches='tight')
    print("✓ Graphe 2 généré: graph2_latencies_comparison.png")
    plt.close()


def graph3_fps_comparison():
    """Graphe 3: FPS total motion2 vs motion (toutes configs)"""
    df = pd.read_csv('graph_data_fps_comparison.csv')

    fig, ax = plt.subplots(figsize=(12, 6))

    colors = ['#F18F01'] + ['#006E90', '#0090B2', '#00A6C7', '#00B8D4', '#00CAE3']
    bars = ax.bar(df['Configuration'], df['AvgFPS'], color=colors, alpha=0.8)

    # Ligne de référence motion2
    ax.axhline(y=df['AvgFPS'].iloc[0], color='red', linestyle='--',
              label='Baseline motion2', alpha=0.5)

    # Ajouter les valeurs et speedups sur les barres
    for i, (bar, fps, spd) in enumerate(zip(bars, df['AvgFPS'], df['Speedup'])):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height + 5,
               f'{fps:.0f} FPS\n({spd:.2f}×)',
               ha='center', va='bottom', fontsize=10, fontweight='bold')

    ax.set_xlabel('Configuration', fontsize=14, fontweight='bold')
    ax.set_ylabel('FPS Moyen', fontsize=14, fontweight='bold')
    ax.set_title('Performances Globales: Toutes Optimisations Combinées',
                fontsize=16, fontweight='bold')
    ax.legend(fontsize=12)
    ax.grid(True, axis='y', alpha=0.3)
    plt.xticks(rotation=15, ha='right')

    plt.tight_layout()
    plt.savefig('graph3_fps_comparison.png', dpi=300, bbox_inches='tight')
    print("✓ Graphe 3 généré: graph3_fps_comparison.png")
    plt.close()


def graph4_stacked_latencies():
    """Graphe 4: Latences empilées (breakdown du temps total)"""
    df = pd.read_csv('graph_data_latencies.csv')
    df_plot = df[~df['Stage'].isin(['Total'])]

    fig, ax = plt.subplots(figsize=(10, 6))

    stages = df_plot['Stage'].str.replace('_', ' ')
    motion2_data = df_plot['motion2_ms']
    motion_data = df_plot['motion_optimized_ms']

    x = np.arange(2)
    width = 0.5

    colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8']

    bottom_m2 = np.zeros(1)
    bottom_opt = np.zeros(1)

    for i, (stage, m2, opt) in enumerate(zip(stages, motion2_data, motion_data)):
        ax.bar(0, m2, width, bottom=bottom_m2, label=stage if i < len(stages) else '',
              color=colors[i % len(colors)], alpha=0.8)
        ax.bar(1, opt, width, bottom=bottom_opt, color=colors[i % len(colors)], alpha=0.8)

        # Annotations
        if m2 > 0.3:
            ax.text(0, bottom_m2 + m2/2, f'{m2:.2f}', ha='center', va='center',
                   fontsize=10, fontweight='bold')
        if opt > 0.3:
            ax.text(1, bottom_opt + opt/2, f'{opt:.2f}', ha='center', va='center',
                   fontsize=10, fontweight='bold')

        bottom_m2 += m2
        bottom_opt += opt

    # Total au-dessus
    ax.text(0, bottom_m2 + 0.3, f'Total: {bottom_m2:.2f} ms',
           ha='center', fontsize=12, fontweight='bold', color='red')
    ax.text(1, bottom_opt + 0.3, f'Total: {bottom_opt:.2f} ms',
           ha='center', fontsize=12, fontweight='bold', color='green')

    ax.set_ylabel('Latence (ms)', fontsize=14, fontweight='bold')
    ax.set_title('Décomposition du Temps de Traitement par Frame',
                fontsize=16, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(['motion2\n(référence)', 'motion\n(optimisé)'], fontsize=12)
    ax.legend(loc='upper left', bbox_to_anchor=(1, 1), fontsize=10)
    ax.grid(True, axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig('graph4_stacked_latencies.png', dpi=300, bbox_inches='tight')
    print("✓ Graphe 4 généré: graph4_stacked_latencies.png")
    plt.close()


if __name__ == '__main__':
    print("Génération des graphes de présentation...")
    print("=" * 50)

    try:
        graph1_openmp_scaling()
        graph2_latencies_comparison()
        graph3_fps_comparison()
        graph4_stacked_latencies()

        print("=" * 50)
        print("✓ Tous les graphes ont été générés avec succès!")
        print("\nFichiers créés:")
        print("  - graph1_openmp_scaling.png     (CM4: Scaling OpenMP)")
        print("  - graph2_latencies_comparison.png (CM2/CM3: Latences par étape)")
        print("  - graph3_fps_comparison.png      (Vue globale: FPS)")
        print("  - graph4_stacked_latencies.png   (Breakdown du temps)")
        print("\nUtilisez ces graphes dans votre présentation PowerPoint.")

    except Exception as e:
        print(f"\n✗ Erreur lors de la génération: {e}")
        print("\nAssurez-vous d'avoir installé les dépendances:")
        print("  pip install matplotlib pandas")
