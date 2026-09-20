# Issues Found

## 1. QSS Inconsistency - Quick Action Buttons
- **Location**: `src/ui/pages/dashboard_page.cpp` (quick action buttons inline styles) and `src/ui/main_window.cpp` (global QSS)
- **Description**: Quick action buttons have inline styles with `border: 1px solid #e0e0e0` and `border-radius: 12px`, while the global QSS sets `border: 1px solid #dbc1b7` and `border-radius: 10px`. This creates visual inconsistency where quick action buttons render with different border colors and rounded corners compared to other push buttons in the application.
- **Impact**: UI appears fragmented with inconsistent button styling across the application.
- **Severity**: Medium - aesthetic issue, does not affect functionality.

## 2. Asymmetric Padding in QSS
- **Location**: `src/ui/main_window.cpp` line 94: `QListWidget::item:disabled { padding: 14px 12px 3px; ... }`
- **Description**: The disabled list item padding has asymmetric values (14px top, 12px right, 3px bottom). This causes disabled items to have different height than enabled items, potentially leading to layout misalignment in list-based UI components.
- **Impact**: List items may appear visually misaligned when some items are disabled.
- **Severity**: Low - minor layout inconsistency.

## 3. Metric Value Display Inconsistency
- **Location**: `src/ui/pages/dashboard_page.cpp` line 67: `value->setText(i == 3 || i == 5 ? "0" : "PKR 0.00");`
- **Description**: Metric values at indices 3 (Low stock) and 5 (Expiring batches) display as "0" while other metrics display as "PKR 0.00". This creates inconsistent formatting across the dashboard metrics row.
- **Impact**: Users may find the mixed formatting confusing when scanning dashboard values quickly.
- **Severity**: Low - minor formatting inconsistency.

## 4. Quick Action Grid Layout - Fixed 3x4 Column Structure
- **Location**: `src/ui/pages/dashboard_page.cpp` lines 118-143
- **Description**: The quick action grid uses a fixed 4-column layout (`i / 4, i % 4`) with 12 actions. On smaller windows or different DPI settings, the grid may not wrap responsively, causing horizontal scrolling or awkward spacing.
- **Impact**: On reduced window sizes, quick actions may overflow or appear cramped.
- **Severity**: Medium - usability issue on small windows.

## 5. QSS Font Family Fallback
- **Location**: `src/ui/main_window.cpp` light/dark stylesheets
- **Description**: The QSS specifies `"Manrope", "Segoe UI", "Inter", sans-serif` as font families. If "Manrope" is not installed on the system, it falls back to "Segoe UI" or the system default, which may render with different metrics and line heights, causing layout shifts.
- **Impact**: Inconsistent typography across different developer/operator machines.
- **Severity**: Low - requires "Manrope" font to be bundled or a system-available alternative.

## 6. Missing "Manrope" Font Asset
- **Location**: Project resources - no font file bundled for "Manrope"
- **Description**: The QSS relies on "Manrope" font but no corresponding `.ttf` or `.otf` file is included in the resource collection or project directory.
- **Impact**: Font fallback occurs on all systems, potentially causing layout shifts due to different font metrics.
- **Severity**: Medium - font must be bundled or QSS font family updated.