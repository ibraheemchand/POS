---
name: Kinetic Inventory System
colors:
  surface: '#f9f9f9'
  surface-dim: '#dadada'
  surface-bright: '#f9f9f9'
  surface-container-lowest: '#ffffff'
  surface-container-low: '#f3f3f3'
  surface-container: '#eeeeee'
  surface-container-high: '#e8e8e8'
  surface-container-highest: '#e2e2e2'
  on-surface: '#1a1c1c'
  on-surface-variant: '#55433b'
  inverse-surface: '#2f3131'
  inverse-on-surface: '#f1f1f1'
  outline: '#88726a'
  outline-variant: '#dbc1b7'
  surface-tint: '#99461f'
  primary: '#99461f'
  on-primary: '#ffffff'
  primary-container: '#f08a5d'
  on-primary-container: '#692400'
  inverse-primary: '#ffb597'
  secondary: '#745b00'
  on-secondary: '#ffffff'
  secondary-container: '#fed972'
  on-secondary-container: '#775e00'
  tertiary: '#535f75'
  on-tertiary: '#ffffff'
  tertiary-container: '#99a5be'
  on-tertiary-container: '#2f3b4f'
  error: '#ba1a1a'
  on-error: '#ffffff'
  error-container: '#ffdad6'
  on-error-container: '#93000a'
  primary-fixed: '#ffdbcd'
  primary-fixed-dim: '#ffb597'
  on-primary-fixed: '#360f00'
  on-primary-fixed-variant: '#7a3008'
  secondary-fixed: '#ffe08b'
  secondary-fixed-dim: '#e6c35e'
  on-secondary-fixed: '#241a00'
  on-secondary-fixed-variant: '#584400'
  tertiary-fixed: '#d7e3fd'
  tertiary-fixed-dim: '#bbc7e0'
  on-tertiary-fixed: '#0f1c2f'
  on-tertiary-fixed-variant: '#3b475c'
  background: '#f9f9f9'
  on-background: '#1a1c1c'
  surface-variant: '#e2e2e2'
typography:
  display-lg:
    fontFamily: Inter
    fontSize: 32px
    fontWeight: '600'
    lineHeight: '1.2'
    letterSpacing: -0.02em
  headline-md:
    fontFamily: Inter
    fontSize: 20px
    fontWeight: '600'
    lineHeight: '1.4'
  body-base:
    fontFamily: Inter
    fontSize: 14px
    fontWeight: '400'
    lineHeight: '1.5'
  table-data:
    fontFamily: Inter
    fontSize: 13px
    fontWeight: '400'
    lineHeight: '1.2'
  label-sm:
    fontFamily: Inter
    fontSize: 12px
    fontWeight: '600'
    lineHeight: '1.2'
    letterSpacing: 0.01em
  monospaced-data:
    fontFamily: Inter
    fontSize: 13px
    fontWeight: '500'
    lineHeight: '1.2'
rounded:
  sm: 0.25rem
  DEFAULT: 0.5rem
  md: 0.75rem
  lg: 1rem
  xl: 1.5rem
  full: 9999px
spacing:
  base: 8px
  gutter: 16px
  margin-page: 24px
  row-padding-y: 10px
  row-padding-x: 12px
  card-gap: 20px
---

## Brand & Style

This design system is built for a desktop-first inventory management environment that bridges the gap between rigid ERP software and modern, approachable fintech tools. It prioritizes high information density and operational efficiency without sacrificing visual warmth. 

The aesthetic is **Corporate / Modern** with a focus on high-clarity data visualization. It utilizes a soft, sunny color palette to reduce the cognitive fatigue often associated with inventory tracking. The system avoids complex rendering techniques like glassmorphism to ensure full compatibility with Qt6/QSS environments, relying instead on solid fills, clean borders, and standard shadows to define hierarchy.

**Key Brand Attributes:**
*   **Operational Clarity:** Maximum legibility for SKU tracking and stock levels.
*   **Approachable Professionalism:** A warm, inviting atmosphere that feels like a modern workspace.
*   **Structural Integrity:** A strict 8px grid system that ensures alignment and predictability across complex tables.

## Colors

The palette is anchored by a soft off-white background to minimize screen glare during long work sessions. 

- **Primary (#f08a5d):** Used for critical actions, low-stock warnings, and primary navigational highlights.
- **Secondary (#f9d56e):** Applied to warning states, reorder alerts, and secondary data visualizations.
- **Tertiary (#c5d1eb):** Used for stable "Good" stock levels and passive organizational elements.
- **Surface & Neutrals:** We use a pure white for card surfaces and table rows to provide maximum contrast against the off-white application background.

## Typography

The design system utilizes **Inter** exclusively to ensure a systematic and utilitarian feel. 

- **High Density:** For data tables, we use a 13px base size to maximize row visibility while maintaining legibility.
- **Tabular Numerals:** All numeric data in tables (SKUs, Prices, Quantities) must use tabular (monospaced) numeral settings to ensure columns align vertically for quick scanning.
- **Visual Hierarchy:** Headlines use a semi-bold weight (600) to stand out against the high-density body text.

## Layout & Spacing

The layout follows a **Fixed-Fluid hybrid** model. Navigation and sidebar elements are fixed, while the main content area (tables and charts) expands to fill the desktop viewport.

- **Grid:** A strict 8px incremental system.
- **Density:** Tables are designed for high-density viewing. Row heights should be kept to a minimum (approx 40-44px) to allow as many items as possible to be visible above the fold.
- **Sections:** Content is grouped into "Containers" with 24px margins between major functional areas (e.g., Stats bar vs. Inventory Table).

## Elevation & Depth

This system avoids heavy shadows and blurs for QSS compatibility. Depth is communicated through **Tonal Layers** and subtle outlines.

1.  **Level 0 (App Background):** `#f9f9f9` (Flat).
2.  **Level 1 (Cards/Tables):** White background with a 1px solid border (`#e0e0e0`).
3.  **Level 2 (Hover/Active):** A very soft, diffused shadow (Offset: 0, 4px; Blur: 12px; Opacity: 0.05 Black) to indicate interactivity.
4.  **Overlays (Menus/Modals):** A slightly stronger shadow to separate from the main canvas.

## Shapes

The shape language is consistently **Rounded**, using a 0.5rem (8px) base radius to soften the technical nature of the ERP data.

- **Buttons & Inputs:** 8px radius.
- **Cards & Tables:** 12px (rounded-lg) for the outer container to create a "contained" feel.
- **Status Chips:** Full pill-shape (100px) to distinguish them from interactive buttons.
- **Table Selection:** 4px radius on the selection highlight to keep it crisp within the row.

## Components

### Buttons
- **Primary:** Solid `#f08a5d` with white text.
- **Secondary:** Solid `#c5d1eb` with dark text.
- **Ghost:** Transparent background with a 1px border.

### High-Density Tables
- **Header:** Light gray background (`#f0f0f0`) with 600 weight text.
- **Rows:** Alternating zebra striping is not required; use 1px bottom borders instead.
- **Cell Alignment:** Text is left-aligned; currency and quantities are right-aligned.

### Stat Cards
- **Visuals:** Use solid background fills from the palette (e.g., a pale orange background for "Low Stock") with high-contrast text.
- **Structure:** Large display metric on the top, descriptive label on the bottom.

### Inputs
- **Style:** 1px solid border (`#e0e0e0`) that changes to Primary Orange on focus. 
- **Height:** Standardized at 36px for a compact desktop feel.

### Navigation (Top Bar)
- **Background:** White with a subtle bottom border.
- **Active State:** A bottom-aligned 3px bar in the Primary color to denote the current module.