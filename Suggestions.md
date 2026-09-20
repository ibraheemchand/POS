# Suggestions - Core Drawbacks and Improvements

## 1. QSS Inconsistency Between Inline and Global Styles
- **Issue**: Quick action buttons in `dashboard_page.cpp` use inline styles (`border: 1px solid #e0e0e0; border-radius: 12px`) while the global QSS in `main_window.cpp` sets `border: 1px solid #dbc1b7; border-radius: 10px`. This creates visual fragmentation.
- **Suggestion**: Consolidate button styling into the global QSS and remove inline styles from individual widgets. Update the QSS to define a primary button style that can be reused via object names (e.g., `#primary`) rather than duplicating in inline code.

## 2. Missing "Manrope" Font Bundling
- **Issue**: The QSS specifies `"Manrope", "Segoe UI", "Inter", sans-serif` as the font family, but no `.ttf`/`.otf` file is included in the project resources or directory. On systems without "Manrope", the fallback font alters layout metrics.
- **Suggestion**: Either bundle the "Manrope" font file in `src/ui/assets/` and register it in `resources.qrc` with `prefix="/fonts"`, or update the QSS to use a system-available font family that has consistent metrics across platforms (e.g., `"Segoe UI", "Ubuntu", "DejaVu Sans"`).

## 3. Asymmetric Padding in Disabled List Items
- **Issue**: `QListWidget::item:disabled` has padding `14px 12px 3px` (top/right/bottom/left), creating an asymmetric height that differs from enabled items (padding `2px 12px 12px`). This causes visual misalignment in lists.
- **Suggestion**: Fix the padding to be symmetric, e.g., `padding: 12px 12px 12px 12px;` or `padding: 14px 12px 14px 12px;` to match the enabled item style rhythm.

## 4. Mixed Metric Value Formatting
- **Issue**: In `dashboard_page.cpp` line 67, metric values at indices 3 (Low stock) and 5 (Expiring batches) display as `"0"` while others show `"PKR 0.00"`. This inconsistent formatting makes the dashboard harder to scan.
- **Suggestion**: Standardize all metric values to use the same format (`"PKR 0.00"` or just `"0"`). Recommend using `"PKR 0.00"` consistently since it provides clearer monetary context.

## 5. Quick Action Grid Non-Responsive Column Count
- **Issue**: The quick action grid uses a fixed 4-column layout (`i / 4, i % 4`) with 12 actions. On smaller windows or high-DPI displays, the grid may overflow or leave large empty spaces.
- **Suggestion**: Make the column count dynamic based on available width, or increase minimum width per action card to reduce the chance of overflow. Consider using `QGridLayout::setHorizontalSpacing` and `setVerticalSpacing` with proportional sizing.

## 6. SVG Icons Missing for Some Actions
- **Issue**: The quick action emojis are used as text (`\xF0\x9F\x9B\x92` for 🛒 etc.) rather than proper SVG icons. While emojis work, they don't scale well on high-DPI displays and don't match the application's bundled SVG icon style.
- **Suggestion**: Replace emoji-based labels with `QIcon(":/icons/...")` resources for each action, or at least add SVG alternatives alongside the emojis. The project already has SVG icons registered in `resources.qrc`.

## 7. QSS Color Values Not Extracted as Variables
- **Issue**: Color values like `#99461f`, `#dbc1b7`, `#ffdbcd` are repeated inline in QSS strings without being defined as variables. Any color change requires updating multiple locations.
- **Suggestion**: Extract repeated color values into QSS pseudo-selectors or external resource files. Consider using a color scheme system where light/dark modes share base colors with only accent variations.

## 8. No Dedicated Font Resource Collection
- **Issue**: Fonts are referenced in QSS but not managed through the Qt RCC system. This means fonts must be installed on the target system or deployed alongside the executable.
- **Suggestion**: Create a `/fonts` resource prefix in `resources.qrc`, add the "Manrope" (or alternative) font file, and update QSS to use `font-family: "Manrope", ...;` with the font registered as `:/fonts/Manrope`. This ensures the font is always available at runtime.

## 9. Dashboard Metric Cards Hard-Coded Height
- **Issue**: Dashboard metric cards have `setMinimumHeight(84)` hard-coded (dashboard_page.cpp line 58). This may not accommodate longer label text or different font sizes in localization.
- **Suggestion**: Use layout-based sizing with `setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred)` and let the layout determine appropriate height, or define the height as a constant that can be easily adjusted.

## 10. Incomplete Cheque Reversal UI (Backend Logic Present, Frontend Missing)
- **Issue**: The `ReturnService` and `cheque_service` have backend logic for returns and cheques, but the UI pages for returns/refunds are not fully implemented (see TASKS.md line 45: `[ ] Add visible Sales Return and Purchase Return UI workflows backed by ReturnService`).
- **Suggestion**: Implement the missing Return UI workflows in the dashboard/quick-actions area, or at least add menu entries that delegate to the existing `ReturnService`. This completes the visual layer for features that already have business logic.